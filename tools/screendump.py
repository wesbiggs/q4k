#!/usr/bin/env python3
"""Read the C64 screen out of a running VICE, headlessly.

Boots a .prg in x64sc with the binary monitor enabled, waits for the program
to paint something, then reads the 40x25 screen matrix ($0400) and colour RAM
($D800) straight out of emulated memory and prints them as text.

This is the assertion behind `make check`: a diffable, scriptable answer to
"what did my program actually put on screen", with no screenshots to eyeball.

  tools/screendump.py dist/hello.prg           # text view
  tools/screendump.py dist/hello.prg --hex     # raw screen codes (authoritative)
  tools/screendump.py dist/hello.prg --color   # colour RAM nybbles
"""

import argparse
import os
import socket
import struct
import subprocess
import sys
import time

STX = 0x02
API_VERSION = 0x02

CMD_MEM_GET = 0x01
CMD_MEM_SET = 0x02
CMD_DISPLAY_GET = 0x84
CMD_EXIT = 0xAA
CMD_QUIT = 0xBB

SCREEN_RAM = 0x0400
COLOR_RAM = 0xD800
VIC_ADDR = 0xD018  # VIC-II memory pointers: which charset is on screen
KBD_BUFFER = 0x0277  # KERNAL keyboard queue
KBD_COUNT = 0x00C6   # ...and how many characters are in it
KBD_MAX = 10
PROBE = 0xCF00
PROBE_MAGIC = (0x51, 0x34)  # 'Q', '4' -- see include/q4k/probe.h
SCR_W, SCR_H = 40, 25

# PETSCII graphics have no ASCII equivalent; these approximations exist so the
# text view stays legible. `--hex` is the authoritative view.
GFX_APPROX = {
    0x66: "%",   # checkerboard
    0x71: "o",   # filled circle
    0x7E: "/",   # diagonal hatch
}


def load_tile_legend(path="assets/tiles.txt"):
    """Map custom tile codes back to the characters the map was authored in.

    Once the game patches its own glyphs into the charset, a screen code like
    $61 is a forest, not a PETSCII graphic -- and the only place that knows
    so is the tile asset. Reusing it here makes the text dump read like the
    map source.
    """
    if not os.path.exists(path):
        return {}
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    try:
        import mktiles
        return {t.code: t.mapchar for t in mktiles.parse(path)}
    except Exception:
        return {}


class Monitor:
    """Minimal client for VICE's binary monitor protocol."""

    def __init__(self, sock):
        self.sock = sock
        self.request_id = 0

    def _send(self, command, body=b""):
        self.request_id += 1
        header = struct.pack(
            "<BBIIB", STX, API_VERSION, len(body), self.request_id, command
        )
        self.sock.sendall(header + body)
        return self.request_id

    def _read_exactly(self, count):
        buf = b""
        while len(buf) < count:
            chunk = self.sock.recv(count - len(buf))
            if not chunk:
                raise ConnectionError("VICE closed the monitor connection")
            buf += chunk
        return buf

    def _await(self, request_id):
        # VICE interleaves unsolicited events (stopped/resumed) with replies.
        while True:
            stx, _api, length, rtype, error, rid = struct.unpack(
                "<BBIBBI", self._read_exactly(12)
            )
            if stx != STX:
                raise ValueError("desynced from monitor stream (bad magic)")
            body = self._read_exactly(length)
            if rid == request_id:
                if error:
                    raise RuntimeError(
                        "monitor error 0x%02x on response type 0x%02x" % (error, rtype)
                    )
                return body

    def mem_get(self, start, end, side_effects=0, memspace=0, bank=0):
        body = struct.pack("<BHHBH", side_effects, start, end, memspace, bank)
        data = self._await(self._send(CMD_MEM_GET, body))
        (length,) = struct.unpack("<H", data[:2])
        return data[2 : 2 + length]

    def mem_set(self, start, data, side_effects=0, memspace=0, bank=0):
        body = struct.pack("<BHHBH", side_effects, start,
                           start + len(data) - 1, memspace, bank) + data
        self._await(self._send(CMD_MEM_SET, body))

    def type_keys(self, text, timeout=15.0):
        """Type text into the running machine, a bufferful at a time.

        Writing the KERNAL's keyboard queue ($0277) and its count ($C6)
        directly is deterministic in a way VICE's own key injection is not:
        the characters are simply there, and the program reads them through
        the ordinary KERNAL path. The queue holds ten, so longer text goes in
        chunks, each one waiting for the program to drain the last.
        """
        # As the keyboard would send them: unshifted letters are PETSCII
        # $41-$5A, shifted ones $C1-$DA. So "Alaric" types as a C64 player
        # would, with SHIFT on the A. Everything else goes through as latin-1
        # (not ascii: the function keys are $85-$8C).
        def petscii(c):
            if "a" <= c <= "z":
                return ord(c) - 0x20
            if "A" <= c <= "Z":
                return ord(c) + 0x80
            return c.encode("latin-1")[0]
        data = bytes(petscii(c) for c in text)
        for i in range(0, len(data), KBD_MAX):
            chunk = data[i:i + KBD_MAX]
            self.mem_set(KBD_BUFFER, chunk)
            self.mem_set(KBD_COUNT, bytes([len(chunk)]))
            self.resume()

            deadline = time.time() + timeout
            while time.time() < deadline:
                time.sleep(0.02)
                if self.mem_get(KBD_COUNT, KBD_COUNT)[0] == 0:
                    break
                self.resume()
            else:
                raise TimeoutError("program never consumed typed input")

    def display(self):
        """VICE's own picture of the last frame: (width, height, pixels),
        a palette index (0-15) a pixel, border and all. What the raster
        really drew -- splits, sprites past eight -- which --shot, drawn
        from memory, cannot know."""
        body = self._await(self._send(CMD_DISPLAY_GET, bytes([1, 0])))
        (head,) = struct.unpack("<I", body[:4])
        width, height, x0, y0, iw, ih = struct.unpack("<6H", body[4:16])
        (length,) = struct.unpack("<I", body[4 + head - 4:4 + head])
        pixels = body[4 + head:4 + head + length]
        # The part a monitor shows (the debug picture has the blanking too).
        rows = [pixels[(y0 + y) * width + x0:(y0 + y) * width + x0 + iw]
                for y in range(ih)]
        return iw, ih, b"".join(rows)

    def resume(self):
        """Let the CPU run again.

        VICE halts the machine whenever a monitor command arrives (and on the
        initial connect), so every read has to be followed by a resume or the
        emulated C64 simply never gets anywhere.
        """
        self._await(self._send(CMD_EXIT))

    def quit(self):
        try:
            self._send(CMD_QUIT)
        except OSError:
            pass


def load_symbols(prg, lbl=None):
    """Read ld65's label file for the program, so tests can name globals.

    Poking state directly is how a test reaches the middle of a long quest
    without walking there -- and it keeps the debug hooks out of the game
    itself, which would otherwise have to carry a cheat channel it does not
    need.
    """
    # dist/<sub>/x.prg was linked in build/<sub>/ (make online's builds are
    # dist/online/ and build/online/), and the plain build is just dist/.
    if not lbl:
        sub = os.path.relpath(os.path.dirname(os.path.abspath(prg)),
                              os.path.abspath("dist"))
        if sub.startswith(".."):
            sub = "."
        lbl = os.path.join("build", sub,
                           os.path.splitext(os.path.basename(prg))[0] + ".lbl")
    syms = {}
    if not os.path.exists(lbl):
        return syms
    for line in open(lbl):
        parts = line.split()
        # "al 00A370 ._quest_stage"
        if len(parts) == 3 and parts[0] == "al" and parts[2].startswith("._"):
            syms[parts[2][2:]] = int(parts[1], 16)
    return syms


def apply_pokes(mon, syms, pokes):
    for spec in pokes:
        name, _, value = spec.partition("=")
        # "party+1=16" addresses into an array or struct.
        name, _, offset = name.partition("+")
        offset = int(offset, 0) if offset else 0
        if name not in syms:
            raise SystemExit("screendump: no symbol %r (is it referenced by "
                             "the program?)" % name)
        v = int(value, 0)
        data = bytes([v & 0xFF]) if v < 256 else bytes([v & 0xFF, v >> 8])
        mon.mem_set(syms[name] + offset, data)
        print("screendump: poked %s+%d=%d at $%04X"
              % (name, offset, v, syms[name] + offset), file=sys.stderr)


def report_peeks(mon, syms, peeks):
    for spec in peeks:
        name, _, width = spec.partition(":")
        name, _, offset = name.partition("+")
        offset = int(offset, 0) if offset else 0
        width = int(width) if width else 1
        if name[:1] == "$":         # an address, not a symbol: $B000
            addr = int(name[1:], 16) + offset
        elif name not in syms:
            raise SystemExit("screendump: no symbol %r" % name)
        else:
            addr = syms[name] + offset
        data = mon.mem_get(addr, addr + width - 1)
        if width > 8:
            # Too long to be a number: just the bytes, last one first, as
            # hex -- the same digits a number would have printed.
            print("screendump: %s+%d @ $%04X = - ($%s)"
                  % (name, offset, addr, data[:width][::-1].hex().upper()),
                  file=sys.stderr)
            continue
        value = int.from_bytes(data[:width], "little")
        print("screendump: %s+%d @ $%04X = %d ($%0*X)"
              % (name, offset, addr, value, width * 2, value),
              file=sys.stderr)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def launch(prg, port, rs232=None, disk=None, extra=()):
    argv = [
        "x64sc",
        "-default",
        "-silent",
        "-binarymonitor",
        "-binarymonitoraddress", "ip4://127.0.0.1:%d" % port,
        "-autostart", prg,
        "-autostartprgmode", "1",
        "+sound",
        "-jamaction", "5",   # a CPU jam quits rather than popping a dialog
    ]
    if disk:
        # A D81 (the open world's) wants a 1581 in unit 8; anything else the
        # 1541 VICE starts with.
        if disk.lower().endswith(".d81"):
            argv += ["-drive8type", "1581"]
        argv += ["-8", disk]
    argv += list(extra)
    if not rs232:
        # Warp is free speed for a self-contained program, but it decouples
        # emulated time from wall-clock time. With a live host on the other
        # end of the modem the C64's timeouts would expire in a fraction of a
        # real second, long before any answer could arrive.
        argv.append("-warp")
    else:
        # User port RS-232 (the modem port), bridged to a TCP socket on the
        # host. VICE dials out to it the first time the C64 opens the port.
        # Without warp the disk runs at a real drive's speed too, and the game
        # loads an engine for every conversation and back: about a minute
        # each way. So the drive is not emulated: VICE serves the KERNAL's
        # LOAD from the image at once instead.
        if disk:
            argv += ["+drive8truedrive", "-trapdevice8"]
        argv += [
            "-userportdevice", "2",
            "-rsuserdev", "0",
            "-rsuserbaud", str(rs232[1]),
            "-rsdev1", "127.0.0.1:%d" % rs232[0],
        ]
    return subprocess.Popen(
        argv,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def connect(port, timeout=30.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = socket.create_connection(("127.0.0.1", port), timeout=5.0)
            sock.settimeout(10.0)
            return sock
        except OSError:
            time.sleep(0.15)
    raise TimeoutError("VICE's binary monitor never came up on port %d" % port)


def wait_for_stage(mon, stage, timeout=60.0):
    """Block until the program publishes >= `stage` on the probe page.

    Polling a handshake beats sleeping: it is as fast as the program is, and
    it cannot read a half-drawn screen or -- as a naive wait does -- mistake
    the power-on RAM garbage for output.
    """
    deadline = time.time() + timeout
    seen = None
    while time.time() < deadline:
        lo, hi, got = mon.mem_get(PROBE, PROBE + 2)[:3]
        if (lo, hi) == PROBE_MAGIC:
            seen = got
            if got >= stage:
                return got  # leave the machine stopped: a consistent snapshot
        mon.resume()
        time.sleep(0.05)
    raise TimeoutError(
        "probe never reached stage %d (last seen: %s). Did the program call "
        "probe_stage()?" % (stage, "none" if seen is None else seen)
    )


# The smooth-scrolling prototype's splash screen reads F1 and F3 from the
# keyboard matrix, with the KERNAL out, so nothing typed into the KERNAL's
# buffer reaches it. While it waits it holds a mark in its hand-off block
# (OW->hook, include/q4k/ow.h), and takes a key written there as pressed.
SPLASH_HOOK = 0x02A7 + 62
SPLASH_WAIT = 0xA5
SPLASH_KEYS = {"f1": 0x85, "f3": 0x86, "quick": 0x87}


def press_splash(mon, key, timeout=60.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if mon.mem_get(SPLASH_HOOK, SPLASH_HOOK)[0] == SPLASH_WAIT:
            mon.mem_set(SPLASH_HOOK, bytes([SPLASH_KEYS[key]]))
            mon.resume()
            return
        mon.resume()
        time.sleep(0.05)
    raise TimeoutError("the splash screen never waited for a key")


def to_text(code, lowercase_charset=False, legend=None):
    """Screen code -> a printable stand-in.

    Screen codes index whichever character set $D018 points at, so the same
    byte is a different glyph depending on the charset. In the uppercase/
    graphics set $01-$1A are A-Z; in the lowercase/mixed set those are a-z and
    A-Z live at $41-$5A instead. Decoding without checking turns readable text
    into line noise.
    """
    if legend and code in legend:
        return legend[code]

    reverse = bool(code & 0x80)
    c = code & 0x7F
    if c == 0x00:
        ch = "@"
    elif c < 0x1B:
        ch = chr((0x60 if lowercase_charset else 0x40) + c)
    elif c < 0x20:
        ch = "[\u00a3]^_"[c - 0x1B]
    elif c < 0x40:
        ch = chr(c)                       # space, punctuation, digits: 1:1
    elif lowercase_charset and 0x41 <= c <= 0x5A:
        ch = chr(c)                       # A-Z live up here in this charset
    else:
        ch = GFX_APPROX.get(c, "\u00b7")
    if reverse and ch == " ":
        ch = "\u2588"                    # reversed space = a solid block
    return ch


# The VIC-II's fixed 16 colours.
PALETTE = [
    (0x00, 0x00, 0x00), (0xFF, 0xFF, 0xFF), (0x88, 0x00, 0x00),
    (0xAA, 0xFF, 0xEE), (0xCC, 0x44, 0xCC), (0x00, 0xCC, 0x55),
    (0x00, 0x00, 0xAA), (0xEE, 0xEE, 0x77), (0xDD, 0x88, 0x55),
    (0x66, 0x44, 0x00), (0xFF, 0x77, 0x77), (0x33, 0x33, 0x33),
    (0x77, 0x77, 0x77), (0xAA, 0xFF, 0x66), (0x00, 0x88, 0xFF),
    (0xBB, 0xBB, 0xBB),
]


def render_png(path, screen, colors, charset, border, background, scale=2,
               sprites=None, vic=None, text_from=SCR_H):
    """Draw the screen exactly as the VIC-II would, from emulated memory.

    VICE's -exitscreenshot only fires when the emulator quits, which is no use
    for catching a screen mid-conversation. Reading screen RAM, colour RAM and
    the character generator over the monitor gives the same picture at any
    moment, and works while the machine is stopped.
    """
    from PIL import Image

    margin = 16
    img = Image.new("RGB", (SCR_W * 8 + margin * 2, SCR_H * 8 + margin * 2),
                    PALETTE[border & 0x0F])
    px = img.load()
    bg = PALETTE[background & 0x0F]

    # Multicolour characters, fine scroll and the 38-column and 24-row
    # borders, when the registers ask for them (the smooth-scrolling
    # prototype does; the game uses none of it).
    d016 = vic[0x16] if vic else 0x08
    d011 = vic[0x11] if vic else 0x1B
    multi = bool(d016 & 0x10)
    shared = [bg, PALETTE[vic[0x22] & 0x0F] if vic else bg,
              PALETTE[vic[0x23] & 0x0F] if vic else bg]
    dx, dy = d016 & 7, (d011 & 7) - 3

    for cy in range(SCR_H):
        if cy == text_from:
            # The multicolour fight's rows of text: the background is black
            # from here down (its interrupt changes it in the row above).
            bg = shared[0] = PALETTE[0]
        for cx in range(SCR_W):
            code = screen[cy * SCR_W + cx]
            nyb = colors[cy * SCR_W + cx] & 0x0F
            fg = PALETTE[nyb]
            glyph = charset[code * 8:code * 8 + 8]
            for row in range(8):
                bits = glyph[row] if row < len(glyph) else 0
                y = margin + cy * 8 + row + dy
                if not margin <= y < margin + SCR_H * 8:
                    continue
                for bit in range(8):
                    x = margin + cx * 8 + bit + dx
                    if x >= margin + SCR_W * 8:
                        continue
                    if multi and nyb & 8:
                        pair = (bits >> (6 - (bit & 6))) & 3
                        px[x, y] = shared[pair] if pair < 3 else PALETTE[nyb & 7]
                    else:
                        px[x, y] = fg if bits & (0x80 >> bit) else bg

    if sprites:
        draw_sprites(px, img.size, margin, vic, *sprites)

    edge = PALETTE[border & 0x0F]
    left, right = (7, 9) if not d016 & 0x08 else (0, 0)
    top, bottom = (4, 4) if not d011 & 0x08 else (0, 0)
    for y in range(margin, margin + SCR_H * 8):
        for x in range(margin, margin + SCR_W * 8):
            if (x < margin + left or x >= margin + SCR_W * 8 - right
                    or y < margin + top or y >= margin + SCR_H * 8 - bottom):
                px[x, y] = edge

    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(path)


def hw_sprites(vic, pointers):
    """The VIC's enabled sprites as (x, y, block, colour, multi, wide, tall),
    sprite 0 first."""
    return [(vic[n * 2] | (0x100 if vic[0x10] & (1 << n) else 0),
             vic[n * 2 + 1], pointers[n], vic[0x27 + n],
             bool(vic[0x1C] & (1 << n)),
             2 if vic[0x1D] & (1 << n) else 1,
             2 if vic[0x17] & (1 << n) else 1)
            for n in range(8) if vic[0x15] & (1 << n)]


def mux_sprites(mon, syms):
    """The sprites the multicolour fight asks its multiplexer for (vs_* in
    src/fight/mux.s), all of them: the VIC's own eight registers hold
    whichever of them the raster had got to when the machine was stopped,
    which is no two runs the same. One is on while its y is not 0; all are
    multicolour."""
    n = 16
    get = lambda name: mon.mem_get(syms[name], syms[name] + n - 1)
    x, xh, y, ptr, col = (get(k) for k in
                          ("vs_x", "vs_xh", "vs_y", "vs_ptr", "vs_col"))
    return [(x[i] | (xh[i] & 1) << 8, y[i], ptr[i], col[i], True, 1, 1)
            for i in range(n) if y[i]]


def draw_sprites(px, size, margin, vic, sprites, fetch):
    """Overlay the sprites, the way the VIC would.

    Hi-res and multicolour, expanded or not, the last drawn first so
    the first ends up on top. Sprite-behind-background priority is ignored:
    q4k never sets it. `fetch(addr)` returns the sprite's 63 bytes, given its
    address in the VIC's bank.
    """
    for x, y, block, own, multi, wide, tall in reversed(sprites):
        colours = (None, PALETTE[vic[0x25] & 0x0F],
                   PALETTE[own & 0x0F], PALETTE[vic[0x26] & 0x0F])
        data = fetch(block * 64)
        # Screen column 0 is sprite X 24; row 0 is sprite Y 50.
        left = margin + x - 24
        top = margin + y - 50
        for row in range(21):
            bits = (data[row * 3] << 16) | (data[row * 3 + 1] << 8) | data[row * 3 + 2]
            for col in range(24):
                if multi:
                    pair = (bits >> (22 - (col & ~1))) & 3
                    colour = colours[pair]
                else:
                    colour = colours[2] if bits & (0x800000 >> col) else None
                if colour is None:
                    continue
                for dy in range(tall):
                    for dx in range(wide):
                        sx, sy = left + col * wide + dx, top + row * tall + dy
                        if 0 <= sx < size[0] and 0 <= sy < size[1]:
                            px[sx, sy] = colour


# What a sprite looks like in the text dump. Screen RAM does not hold the
# hero any more -- the VIC draws it over the square -- so without this the
# goldens would stop recording where the hero is standing.
SPRITE_MARK = "@"
OW_FONT = 0xA0          # the open world's text window: its letters' first code
OW_TEXT_ROW = 22        # and its first row (src/ow/smooth.s)
SPRITE_CODE = 0x100


def sprite_cells(sprites, fetch, square=False):
    """The screen cell under the middle of each sprite's pixels -- or, with
    `square`, of the 16 by 16 it is drawn in (the multicolour fight's, whose
    figures shift their feet between two frames, so that the middle of
    their pixels is not always the same cell)."""
    cells = set()
    for x, y, block, _, _, wide, tall in sprites:
        if square:
            cx, cy = (x - 24 + 8) // 8, (y - 50 + 7) // 8
            if 0 <= cx < SCR_W and 0 <= cy < SCR_H:
                cells.add((cx, cy))
            continue
        data = fetch(block * 64)
        lit = [(col, row) for row in range(21) for col in range(24)
               if data[row * 3 + col // 8] & (0x80 >> (col % 8))]
        if not lit:
            continue
        mid_x = (min(c for c, _ in lit) + max(c for c, _ in lit)) * wide // 2
        mid_y = (min(r for _, r in lit) + max(r for _, r in lit)) * tall // 2
        cx = (x + mid_x - 24) // 8
        cy = (y + mid_y - 50) // 8
        if 0 <= cx < SCR_W and 0 <= cy < SCR_H:
            cells.add((cx, cy))
    return cells


def grid(rows, render, gutter="", by_row=False):
    """by_row: render takes the row as well as the code."""
    width = len(render(0, 0) if by_row else render(0))
    out = ["    +" + "-" * (width * SCR_W) + "+"]
    for y, row in enumerate(rows):
        out.append("%3d |%s|" % (y, "".join(
            render(c, y) if by_row else render(c) for c in row)))
    out.append("    +" + "-" * (width * SCR_W) + "+")
    return "\n".join(out) + gutter


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("prg")
    ap.add_argument("--hex", action="store_true", help="raw screen codes")
    ap.add_argument("--color", action="store_true", help="colour RAM nybbles")
    ap.add_argument("--stage", type=int, default=1,
                    help="probe stage to wait for (default: 1)")
    ap.add_argument("--keys", metavar="STR",
                    help=r"type STR once the program is up, then wait for "
                         r"--stage. Escapes are interpreted, so \r is RETURN.")
    ap.add_argument("--ready", type=int, default=1,
                    help="stage meaning 'running and accepting input' (default: 1)")
    ap.add_argument("--raw", action="store_true",
                    help="ignore assets/tiles.txt and decode as plain PETSCII")
    ap.add_argument("--rs232", type=int, metavar="PORT",
                    help="bridge the user port modem to 127.0.0.1:PORT")
    ap.add_argument("--baud", type=int, default=300)
    ap.add_argument("--splash", choices=sorted(SPLASH_KEYS),
                    help="press this key at the smooth-scrolling "
                         "prototype's splash screen (f1 a new game, f3 the "
                         "saved one), before anything else")
    ap.add_argument("--poke", action="append", metavar="SYM=VALUE",
                    default=[],
                    help="set a global by name once the program is running")
    ap.add_argument("--lbl", metavar="FILE",
                    help="the label file for --poke and --peek, when the "
                         "program that runs is not the one started (the "
                         "smooth-scrolling prototype's bootstrap loads an "
                         "engine: build/ow-cpu.lbl or build/ow-reu.lbl)")
    ap.add_argument("--disk", metavar="D64",
                    help="attach a disk image as unit 8, for save and load")
    ap.add_argument("--peek", action="append", metavar="SYM|$ADDR[+OFF][:WIDTH]",
                    default=[], help="print a global by name")
    ap.add_argument("--shot", metavar="PNG",
                    help="also render the screen to a PNG from emulated memory")
    ap.add_argument("--vicshot", metavar="PNG",
                    help="also save VICE's own picture of the last frame "
                         "(the real raster: splits and all)")
    ap.add_argument("--mux", metavar="LBL",
                    help="show the sprites the multicolour fight asks its "
                         "multiplexer for, from its label file "
                         "(build/fight.lbl), not the eight the VIC has; and "
                         "its text rows on black")
    ap.add_argument("--settle", type=float, metavar="SECS",
                    help="after typing, let the machine run this long, then dump")
    ap.add_argument("--timeout", type=float, default=60.0)
    ap.add_argument("--vice", action="append", metavar="ARG", default=[],
                    help="pass ARG to x64sc as well, e.g. --vice=-reu "
                         "--vice=-ntsc (one argument each)")
    args = ap.parse_args()
    if args.keys:
        # So a shell can pass "look\r" without needing $'...' quoting.
        args.keys = args.keys.encode("latin-1", "replace").decode("unicode_escape")

    port = free_port()
    vice = launch(args.prg, port,
                  (args.rs232, args.baud) if args.rs232 else None,
                  args.disk, args.vice)
    sock = None
    try:
        sock = connect(port)
        mon = Monitor(sock)
        mon.resume()   # connecting drops VICE into the monitor; let it boot
        if args.splash:
            press_splash(mon, args.splash, args.timeout)
        stage = wait_for_stage(mon, args.ready, args.timeout)
        if args.poke:
            apply_pokes(mon, load_symbols(args.prg, args.lbl), args.poke)

        if args.keys:
            mon.resume()
            mon.type_keys(args.keys)
        if args.settle:
            mon.resume()
            time.sleep(args.settle)
        elif args.stage > args.ready:
            stage = wait_for_stage(mon, args.stage, args.timeout)
        # Which 16K the VIC sees ($DD00 bits 1-0, inverted) and where in
        # it the screen is ($D018 bits 7-4). The game keeps bank 0 and
        # $0400; the smooth-scrolling prototype does not.
        bank = (3 - (mon.mem_get(0xDD00, 0xDD00)[0] & 3)) * 0x4000
        d018 = mon.mem_get(VIC_ADDR, VIC_ADDR)[0]
        screen_at = bank + (d018 >> 4) * 0x400
        screen = mon.mem_get(screen_at, screen_at + SCR_W * SCR_H - 1)
        # $D018 bits 3-1 select the charset base. $1000 is the uppercase ROM
        # set; $1800 is the lowercase one. Anything else is a RAM charset,
        # and q4k's is a copy of the lowercase set with tiles patched over it.
        charset_base = (d018 & 0x0E) << 10
        lowercase = charset_base != 0x1000
        rom_charset = bank in (0, 0x8000) and charset_base in (0x1000, 0x1800)
        charset_base += bank
        colors = mon.mem_get(COLOR_RAM, COLOR_RAM + SCR_W * SCR_H - 1)
        # The open world's engine is the one thing in VIC bank 1 with a
        # text window (the fight module's is bank 1 too, but --mux).
        ow_engine = bank == 0x4000 and not args.mux
        if args.peek:
            syms = load_symbols(args.prg, args.lbl)
            if args.mux:
                syms = dict(load_symbols(args.prg, args.mux), **syms)
            report_peeks(mon, syms, args.peek)
        scratch = mon.mem_get(PROBE, PROBE + 19)
        vic = mon.mem_get(0xD000, 0xD02E)
        pointers = mon.mem_get(screen_at + 0x3F8, screen_at + 0x3FF)
        if args.mux:
            sprites = mux_sprites(mon, load_symbols(args.prg, args.mux))
        else:
            sprites = hw_sprites(vic, pointers)
        sprite_data = {p * 64: mon.mem_get(bank + p * 64, bank + p * 64 + 62)
                       for p in {sp[2] for sp in sprites}}
        if args.vicshot:
            from PIL import Image
            mon.resume()            # a frame or two drawn since the last stop
            time.sleep(0.1)
            w, h, pixels = mon.display()
            img = Image.new("RGB", (w, h))
            img.putdata([PALETTE[p & 0x0F] for p in pixels])
            img.save(args.vicshot)
            print("screendump: wrote %s" % args.vicshot, file=sys.stderr)
        if args.shot:
            if rom_charset:
                print("screendump: charset is in ROM; --shot needs a RAM "
                      "charset", file=sys.stderr)
            else:
                charset = mon.mem_get(charset_base, charset_base + 0x7FF)
                render_png(args.shot, screen, colors, charset,
                           vic[0x20], vic[0x21],
                           sprites=(sprites, sprite_data.__getitem__),
                           vic=vic, text_from=19 if args.mux else SCR_H)
                print("screendump: wrote %s" % args.shot, file=sys.stderr)
        mon.quit()
    finally:
        if sock:
            sock.close()
        try:
            vice.wait(timeout=5)
        except subprocess.TimeoutExpired:
            vice.kill()

    print("probe: stage=%d counter=%d scratch=%s | charset=%s"
          % (stage, scratch[3], " ".join("%02x" % b for b in scratch[4:20]),
             "$%04X %s" % (charset_base,
                           "lowercase" if lowercase else "uppercase")),
          file=sys.stderr)
    legend = {} if args.raw else load_tile_legend()
    rows = [screen[y * SCR_W : (y + 1) * SCR_W] for y in range(SCR_H)]
    if args.hex:
        print(grid(rows, lambda c: "%02x" % c))
    else:
        # A sprite goes in as a code no byte can hold, so it cannot be
        # mistaken for anything the charset draws.
        if not args.raw:
            rows = [list(row) for row in rows]
            for cx, cy in sprite_cells(sprites, sprite_data.__getitem__,
                                       bool(args.mux)):
                rows[cy][cx] = SPRITE_CODE
        # The open world's engine: rows 22-23 are its text window (smooth.s),
        # whose letters are screen codes from OW_FONT. As text, not tiles.
        def cell(c, y):
            if c == SPRITE_CODE:
                return SPRITE_MARK
            if ow_engine and y >= OW_TEXT_ROW and c >= OW_FONT:
                return to_text(c - OW_FONT, True)
            return to_text(c, lowercase, legend)
        print(grid(rows, cell, by_row=True))

    if args.color:
        crows = [colors[y * SCR_W : (y + 1) * SCR_W] for y in range(SCR_H)]
        print()
        print(grid(crows, lambda c: "%x" % (c & 0x0F)))


if __name__ == "__main__":
    sys.exit(main())
