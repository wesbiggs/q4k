#!/usr/bin/env python3
"""q4k NPC dialogue daemon -- the other end of the C64's modem line.

The C64 dials in over its user port (bridged by VICE to a TCP socket), sends a
line of whatever the player typed, and gets one line of in-character reply
back. A local Ollama model supplies the reply; assets/canon.md supplies the
world it has to stay inside.

The client only ever sends an NPC id. Every system prompt -- the world canon,
the character's voice, what they privately know -- lives here and is looked up
by that id, so the C64 never carries a byte of prompt.

Protocol, CR-terminated ASCII in both directions:

    ->  HI <npc> <stage>           open a conversation
    ->  SAY <npc> <stage> <text>   say something
    <-  OK <reply>
    <-  ERR <reason>

<stage> is how far the player has got in the quest. The C64 sends the number;
the sentence describing what it means lives here, in STAGES, so the story can
be reworded without touching a byte of 6502.

Every response is preceded by a bare CR. That leading byte is deliberate: the
first byte the C64's KERNAL RS-232 receiver sees after the line comes up is
reliably mangled by a bit of clock slip, so it gets something disposable.

Run with --mock to serve canned lines from assets/npcs.json instead of calling
a model -- deterministic, instant, and what the golden test uses.

    host/npcd.py                       # live, needs ollama running
    host/npcd.py --model llama3.2:1b   # smaller and faster, drifts more
    host/npcd.py --mock                # canned lines
"""

import argparse
import json
import os
import re
import socket
import sys
import threading
import urllib.error
import urllib.request
import zlib

# Ollama's OpenAI-compatible chat endpoint. Plain HTTP against the local
# daemon, so this file has no third-party dependencies at all.
#
# Both are configurable three ways, in increasing precedence: the constants
# here, the Q4K_OLLAMA_URL / Q4K_MODEL environment variables, and the --url /
# --model flags. Anything that speaks the OpenAI chat shape will do -- another
# Ollama host, llama.cpp's server, an OpenAI-compatible proxy.
DEFAULT_URL = os.environ.get("Q4K_OLLAMA_URL", "http://localhost:11434/v1")
DEFAULT_MODEL = os.environ.get("Q4K_MODEL", "llama3.2:3b")

# Quest progress. The client sends the index; keep in step with
# include/q4k/quest.h.
# Written from the NPC's point of view, about the person in front of them --
# a small model will otherwise read the situation as its own and start
# roleplaying the player.
STAGES = [
    "The traveller has the king's letter and has not yet set out from Millford.",
    "The traveller is on the road to Castle Matheson, summoned but not yet received.",
    "The king has given the traveller the quest and the Iron Sigil. No stones recovered yet.",
    "The traveller has brought the Emberstone out of the Hollow Crown.",
    "The traveller has the Emberstone, and the Tidal Key to the Drowned Stair.",
    "The traveller has brought the Silver Tear out of the Drowned Stair.",
    "The traveller has two stones, and the Ashen Key to Blackpit.",
    "The traveller has all three stones: Emberstone, Silver Tear, Dragon's Heart.",
    "Garrett has reforged Dragonbane. The traveller is going after Maldrath.",
    "Maldrath is banished. It is over, and everyone knows it.",
]

MAX_REPLY = 150          # what fits in the C64's dialogue box
MAX_TOKENS = 120         # a little slack over MAX_REPLY, trimmed after
HISTORY_TURNS = 8        # keep the last few exchanges per NPC

# Written for a small local model: the binding constraints go last, where a
# 1B model weights them most, and they are phrased as what to do rather than
# what to avoid. The canon goes in front of this block.
# Written for a small local model. The character's identity is stated before
# the canon as well as after it -- buried behind two pages of world the model
# forgets who it is -- and the binding constraints go last, where they are
# weighted most.
SYSTEM_PREFACE = """You are {name}. {role}
You are a person who lives in this world. You are NOT the traveller who is
speaking to you, and you are not a narrator.

Here is the world you live in.
"""

SYSTEM_RULES = """
Now, again: you are {name}. {role}
You are in {where}.

Your manner: {voice}

Something you know, and may share if it comes up: {knows}

How things stand with the traveller in front of you: {situation}

That traveller has just spoken to you. Answer them, as yourself.

How to answer, every single time:
- Write ONE or TWO short sentences. Never more. Under {limit} characters.
- Speak in the first person, as yourself. You ARE {name}: say "I", never
  refer to {name} as though he or she were someone else.
- The traveller's quest is theirs, not yours. Never describe their plans or
  their journey as your own.
- Write only the words {name} says out loud.
- No narration. No asterisks. No quotation marks around your reply.
- Do not write your own name before your reply.
- Everything you say must fit the world described above. Do not invent
  names, dates, places or events that are not in it. If you are asked about
  something it does not cover, say plainly that you do not know, the way
  {name} would.
- You are a person in this world, not an assistant. Never say otherwise.
"""

# The C64 can render the printable ASCII range and nothing else.
_ALLOWED = re.compile(r"[^ -~]")
_SMART = {
    "\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"',
    "\u2013": "-", "\u2014": " - ", "\u2026": "...", "\u00a0": " ",
}
# A 1B model reaches for stage directions and speaker labels no matter how
# firmly the prompt says not to, so they come off here as well.
_STAGE = re.compile(r"\*[^*]*\*")
_ASSISTANT = re.compile(r"^\s*(?:assistant|ai|system)\s*:\s*", re.I)
_SENTENCE = re.compile(r"(?<=[.!?])\s+")
MAX_SENTENCES = 2


def for_c64(text, limit=MAX_REPLY, speaker=None, max_sentences=None):
    """Flatten a reply into something the C64 can actually display."""
    for bad, good in _SMART.items():
        text = text.replace(bad, good)

    text = _STAGE.sub(" ", text)
    text = _ASSISTANT.sub("", text)
    if speaker:
        # "Merrit: ..." / "Steward Merrit: ..."
        for name in (speaker, speaker.split()[-1]):
            prefix = name.lower() + ":"
            if text.strip().lower().startswith(prefix):
                text = text.strip()[len(prefix):]
                break

    text = _ALLOWED.sub("", text)
    text = " ".join(text.split())

    # Models like to wrap the whole line in quotes; drop them only when they
    # enclose everything, so a quoted phrase inside a reply survives.
    if len(text) > 1 and text[0] == text[-1] and text[0] in "\"'":
        text = text[1:-1].strip()

    # The prompt asks for two sentences; a 1B model obliges perhaps half the
    # time. Enforcing it here is free and always works. Authored mock lines
    # are already written to fit, so they are left alone.
    if max_sentences:
        parts = _SENTENCE.split(text)
        if len(parts) > max_sentences:
            text = " ".join(parts[:max_sentences])

    if len(text) > limit:
        cut = text[:limit]
        # Prefer to end on a word, and on a sentence if one is close enough.
        stop = max(cut.rfind(". "), cut.rfind("! "), cut.rfind("? "))
        text = cut[:stop + 1] if stop > limit * 0.6 else cut.rsplit(" ", 1)[0]
    return text


class Npcs:
    def __init__(self, canon_path, npcs_path):
        self.canon = open(canon_path).read()
        self.npcs = json.load(open(npcs_path))

    def system_for(self, npc_id, stage=0):
        """The whole system prompt for one NPC, assembled host-side.

        The client sends nothing but an id and a stage number; world canon,
        voice, private knowledge and the state of the story are looked up and
        stitched together here.
        """
        npc = self.npcs[npc_id]
        situation = STAGES[stage] if 0 <= stage < len(STAGES) else STAGES[0]
        return (SYSTEM_PREFACE.format(**npc)
                + "\n" + self.canon + "\n"
                + SYSTEM_RULES.format(limit=MAX_REPLY, situation=situation,
                                      **npc))

    def mock_reply(self, npc_id, text):
        lines = self.npcs[npc_id]["mock"]
        # Deterministic across runs -- Python's hash() is salted per process,
        # and the golden tests need the same answer every time.
        return lines[zlib.crc32(text.strip().lower().encode()) % len(lines)]


class Dialogue:
    """One conversation per NPC, with a little memory."""

    def __init__(self, npcs, url=DEFAULT_URL, model=DEFAULT_MODEL,
                 temperature=0.7, timeout=60.0, mock=False):
        self.npcs = npcs
        self.url = url.rstrip("/") + "/chat/completions"
        self.model = model
        self.temperature = temperature
        self.timeout = timeout
        self.mock = mock
        self.history = {}

    def _chat(self, messages):
        """POST to the OpenAI-compatible endpoint and return the reply text."""
        body = json.dumps({
            "model": self.model,
            "messages": messages,
            "max_tokens": MAX_TOKENS,
            "temperature": self.temperature,
            "stream": False,
        }).encode()

        request = urllib.request.Request(
            self.url, data=body,
            headers={
                "Content-Type": "application/json",
                # Ollama ignores the credential, but the OpenAI shape wants one.
                "Authorization": "Bearer ollama",
            },
        )
        with urllib.request.urlopen(request, timeout=self.timeout) as resp:
            payload = json.load(resp)
        return payload["choices"][0]["message"]["content"]

    def reply(self, npc_id, text, stage=0):
        if npc_id not in self.npcs.npcs:
            return None, "no such person here"

        if self.mock:
            return for_c64(self.npcs.mock_reply(npc_id, text)), None

        name = self.npcs.npcs[npc_id]["name"]
        turns = self.history.setdefault((npc_id, stage), [])
        turns.append({"role": "user", "content": text})

        messages = [{"role": "system",
                     "content": self.npcs.system_for(npc_id, stage)}]
        messages += turns[-HISTORY_TURNS:]

        try:
            said = self._chat(messages)
        except urllib.error.URLError as exc:
            turns.pop()
            reason = getattr(exc, "reason", exc)
            print("[npcd] model call failed: %s" % (reason,), flush=True)
            return None, "the line is bad"
        except (KeyError, ValueError, TimeoutError) as exc:
            turns.pop()
            print("[npcd] bad model response: %r" % (exc,), flush=True)
            return None, "the line is bad"

        cleaned = for_c64(said, speaker=name,
                          max_sentences=MAX_SENTENCES)
        if not cleaned:
            # Small models sometimes return nothing at all, or nothing but
            # stage directions. An authored line keeps the scene intact.
            turns.pop()
            return for_c64(self.npcs.npcs[npc_id]["mock"][0]), None

        # Remember what the model actually said, not the trimmed version, so
        # it stays coherent with its own previous turn.
        turns.append({"role": "assistant", "content": said.strip()})
        return cleaned, None


def handle(conn, addr, dialogue, verbose):
    print("[npcd] connected: %s" % (addr,), flush=True)
    buf = bytearray()

    def send(line):
        # Leading CR absorbs the C64's first-byte sync slip.
        payload = b"\r" + line.encode("ascii", "replace") + b"\r"
        if verbose:
            print("[npcd] tx %r" % payload, flush=True)
        conn.sendall(payload)

    try:
        while True:
            chunk = conn.recv(256)
            if not chunk:
                break
            buf += chunk
            while True:
                idx = min((buf.find(t) for t in (b"\r", b"\n") if t in buf),
                          default=-1)
                if idx < 0:
                    break
                raw, buf[:] = bytes(buf[:idx]), buf[idx + 1:]
                line = raw.decode("ascii", "replace").strip()
                if not line:
                    continue
                if verbose:
                    print("[npcd] rx %r" % line, flush=True)

                verb, _, rest = line.partition(" ")
                npc_id, _, rest = rest.partition(" ")
                stage_tok, _, said = rest.partition(" ")
                verb, npc_id = verb.upper(), npc_id.lower()

                try:
                    stage = int(stage_tok)
                except ValueError:
                    # Tolerate a client that omits the stage.
                    stage, said = 0, (stage_tok + " " + said).strip()

                if verb == "HI":
                    said = said or "Hello."
                elif verb != "SAY":
                    send("ERR unknown request")
                    continue

                text, err = dialogue.reply(npc_id, said or "Hello.", stage)
                send("ERR " + err if err else "OK " + text)
    except OSError as exc:
        print("[npcd] %s" % exc, flush=True)
    finally:
        print("[npcd] disconnected: %s" % (addr,), flush=True)
        conn.close()


def check_model(url, model):
    """Return a human-readable problem with the model backend, or None."""
    try:
        with urllib.request.urlopen(url.rstrip("/") + "/models",
                                    timeout=5.0) as resp:
            names = [m.get("id") for m in json.load(resp).get("data", [])]
    except urllib.error.URLError as exc:
        return ("cannot reach %s (%s). Is ollama running?"
                % (url, getattr(exc, "reason", exc)))
    except (ValueError, KeyError):
        return "%s did not answer with a model list" % url

    if model not in names:
        return ("%s is not pulled. Run: ollama pull %s\n[npcd] available: %s"
                % (model, model, ", ".join(names) or "none"))
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", type=int, default=25232)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--mock", action="store_true",
                    help="serve canned lines instead of calling a model")
    ap.add_argument("--model", default=DEFAULT_MODEL)
    ap.add_argument("--url", default=DEFAULT_URL,
                    help="OpenAI-compatible base URL (default: local ollama)")
    ap.add_argument("--temperature", type=float, default=0.7)
    ap.add_argument("--canon", default="assets/canon.md")
    ap.add_argument("--npcs", default="assets/npcs.json")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    npcs = Npcs(args.canon, args.npcs)
    dialogue = Dialogue(npcs, url=args.url, model=args.model,
                        temperature=args.temperature, mock=args.mock)

    if not args.mock:
        problem = check_model(args.url, args.model)
        if problem:
            print("[npcd] %s\n[npcd] serving canned lines instead (--mock)"
                  % problem, file=sys.stderr)
            dialogue.mock = True

    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.host, args.port))
    srv.listen(4)
    print("[npcd] %s on %s:%d -- npcs: %s"
          % ("mock" if dialogue.mock else args.model, args.host, args.port,
             ", ".join(npcs.npcs)), flush=True)

    try:
        while True:
            conn, addr = srv.accept()
            threading.Thread(target=handle,
                             args=(conn, addr, dialogue, not args.quiet),
                             daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        srv.close()


if __name__ == "__main__":
    sys.exit(main())
