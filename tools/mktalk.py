#!/usr/bin/env python3
"""assets/talk.txt + assets/hints.json -> the offline build's dialogue files.

    tools/mktalk.py OUTDIR ROOM

writes OUTDIR/0 .. OUTDIR/8, one per NPC (NPC_* in town_data.h), and
OUTDIR/9 .. OUTDIR/11, the rumours of each inn (inn_ids[] in town_data.c, then
every inn the town generator makes). The Makefile puts them on the open
world's d81 under those names, and src/lib/talk.c loads one into the talk's buffer
(src/lib/talkbuf.s) when it is needed. Each is a PRG whose load address is ignored, then entries of

    flags            0 a greeting or a rumour, 1 a topic; | 0x80: give
    lo << 4 | hi     the quest stages it fits, inclusive
    label, NUL       topics only
    text, NUL        ASCII

and a 0xFF where the next flags would be. Every file must fit in ROOM bytes,
the talk's buffer (TALK_SIZE in the Makefile): anything longer would load
over whatever follows the talk's engine.
"""

import json
import os
import re
import sys

TALK = "assets/talk.txt"
HINTS = "assets/hints.json"
TOWN_H = "include/q4k/town_data.h"
TOWN_C = "src/lib/town_data.c"

LAST_STAGE = 10      # Q_REWARDED
HOST_LAST = 9        # the host is sent 9 for 10, so its 9 means 9-10 here
MAX_TOPICS = 9       # keys 1-9; 0 is Leave
MAX_LABEL = 16       # two columns of 20: digit, space, label, gap
MAX_TEXT = 200       # about six rows of the transcript's twelve
PROC_INN = "wayside" # every inn town_gen makes; inn.c's proc_inn_id

GREET, TOPIC, GIVE, END = 0x00, 0x01, 0x80, 0xFF


def die(where, msg):
    sys.exit("%s: %s" % (where, msg))


def npc_count():
    return len(re.findall(r"#define\s+NPC_\w+\s+\d+", open(TOWN_H).read()))


def inn_ids():
    m = re.search(r"inn_ids\[[^]]*\]\s*=\s*\{([^}]*)\}", open(TOWN_C).read())
    if not m:
        die(TOWN_C, "no inn_ids")
    return re.findall(r'"([^"]+)"', m.group(1)) + [PROC_INN]


def stages(spec, where):
    if spec == "*":
        return 0, LAST_STAGE
    m = re.fullmatch(r"(\d+)(?:-(\d+))?", spec)
    if not m:
        die(where, "bad stages %r" % spec)
    lo = int(m.group(1))
    hi = int(m.group(2) or lo)
    if not lo <= hi <= LAST_STAGE:
        die(where, "stages %r out of 0-%d" % (spec, LAST_STAGE))
    return lo, hi


def check_text(text, where, limit):
    if not text:
        die(where, "nothing said")
    bad = [c for c in text if not " " <= c <= "~"]
    if bad:
        die(where, "not plain ASCII: %r" % "".join(bad))
    if len(text) > limit:
        die(where, "%d characters, the most is %d" % (len(text), limit))


def parse_talk(path=TALK):
    """{npc: [(flags, lo, hi, label, text, where)]}"""
    npcs = {}
    cur = None
    last = None
    for n, raw in enumerate(open(path), 1):
        where = "%s:%d" % (path, n)
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if line[0] in " \t":
            if last is None:
                die(where, "continues nothing")
            last[4] += " " + line.strip()
            continue
        m = re.fullmatch(r"\[npc (\d+)\].*", line)
        if m:
            cur = int(m.group(1))
            if cur in npcs:
                die(where, "npc %d twice" % cur)
            npcs[cur] = []
            last = None
            continue
        if cur is None:
            die(where, "before any [npc N]")
        fields = [f.strip() for f in line.split("|")]
        head = fields[0].split()
        kind = head[0] if head else ""
        if kind not in ("greet", "topic") or len(head) not in (2, 3) \
                or (len(head) == 3 and head[2] != "give"):
            die(where, "expected greet|topic STAGES [give] | ...")
        want = 3 if kind == "topic" else 2
        if len(fields) != want:
            die(where, "%s takes %d fields" % (kind, want))
        lo, hi = stages(head[1], where)
        flags = (TOPIC if kind == "topic" else GREET) | (GIVE if len(head) == 3 else 0)
        label = fields[1] if kind == "topic" else None
        last = [flags, lo, hi, label, fields[-1], where]
        npcs[cur].append(last)
    return npcs


def parse_hints(path=HINTS):
    """{inn: [(GREET, lo, hi, None, text, where)]}"""
    inns = inn_ids()
    out = {i: [] for i in range(len(inns))}
    for k, h in enumerate(json.load(open(path))["hints"]):
        where = "%s: hint %d" % (path, k)
        st = sorted(h.get("stages", range(LAST_STAGE + 1)))
        if st != list(range(st[0], st[-1] + 1)):
            die(where, "stages %s are not one run" % st)
        lo, hi = st[0], st[-1]
        if hi == HOST_LAST:
            hi = LAST_STAGE
        for i, inn in enumerate(inns):
            if inn in h.get("inns", [inn]):
                out[i].append([GREET, lo, hi, None, h["text"], where])
    return out


def check(entries, name, rumours):
    for flags, lo, hi, label, text, where in entries:
        check_text(text, where, MAX_TEXT)
        if label is not None:
            check_text(label, where, MAX_LABEL)
    for s in range(LAST_STAGE + 1):
        fit = [e for e in entries if e[1] <= s <= e[2]]
        if not any(e[0] & TOPIC == GREET for e in fit):
            die(name, "nothing to %s at stage %d"
                % ("say" if rumours else "greet with", s))
        topics = [e for e in fit if e[0] & TOPIC]
        if len(topics) > MAX_TOPICS:
            die(name, "%d topics at stage %d, the most is %d"
                % (len(topics), s, MAX_TOPICS))
        labels = [e[3] for e in topics]
        if len(set(labels)) != len(labels):
            die(name, "the same topic twice at stage %d" % s)


def pack(entries):
    out = bytearray([0x00, 0x40])        # load address, which LOAD ignores
    for flags, lo, hi, label, text, where in entries:
        out += bytes([flags, lo << 4 | hi])
        if label is not None:
            out += label.encode("ascii") + b"\0"
        out += text.encode("ascii") + b"\0"
    out.append(END)
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    outdir = sys.argv[1]
    room = int(sys.argv[2], 0)
    os.makedirs(outdir, exist_ok=True)

    npcs = parse_talk()
    count = npc_count()
    if sorted(npcs) != list(range(count)):
        die(TALK, "has npcs %s, the game has 0-%d" % (sorted(npcs), count - 1))
    files = {n: (npcs[n], "npc %d" % n, False) for n in npcs}
    for i, hints in parse_hints().items():
        files[count + i] = (hints, "inn %d" % i, True)

    for num, (entries, name, rumours) in sorted(files.items()):
        check(entries, name, rumours)
        data = pack(entries)
        size = len(data) - 2
        if size > room:
            die(name, "%d bytes, the buffer holds %d" % (size, room))
        with open(os.path.join(outdir, str(num)), "wb") as f:
            f.write(data)
        print("talk %-3s %-7s %4d bytes (%d free)" % (num, name, size, room - size))


if __name__ == "__main__":
    main()
