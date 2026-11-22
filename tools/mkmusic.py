#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turn assets/theme.txt into the tables the theme's player reads.

Writes src/sid/theme_data.s, which src/sid/theme.s plays: the two are
assembled into a SID file of their own (build/sid/theme.sid, see
tools/mksid.py), which the game loads from its disk like any other. A quiet second voice holds a note a bar, a
perfect fourth below one of the bar's melody notes, worked out here so the
score stays just the melody. Each note is two bytes: pitch index in bits 0-4
with the slur flag at bit 6 and the vibrato flag at bit 7, then its length in
eighths. Pitches are indices into a table of the song's own distinct notes,
because the SID's frequency register depends on the machine's clock: the
table holds the NTSC values and then the PAL ones, and the player starts at
whichever half it finds itself on.
"""

import re
import sys

SRC = "assets/theme.txt"
OUT = "src/sid/theme_data.s"

NTSC_CLOCK = 1022727
PAL_CLOCK = 985248
# How often play is called: 60 Hz on both machines. The SID file says so
# (its speed flag is CIA, the PSID convention for 60 Hz), and the game calls
# it from the KERNAL's interrupt, which the KERNAL times at 60 Hz on PAL as
# well as NTSC (CIA timer $4025 on PAL, $4295 on NTSC). An earlier version
# took PAL for 50 Hz and played the tune about 18% fast there.
RATE = 60

LETTER = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
TOKEN = re.compile(r"^([A-G])(b?)(\d):(\d+)([~vh]*)$")


def die(msg):
    sys.exit("%s: %s" % (SRC, msg))


def parse():
    tempo, notes, bar = None, [], 0
    for lineno, raw in enumerate(open(SRC), 1):
        line = raw.split("#")[0].strip()
        if not line:
            continue
        if line.startswith("tempo"):
            tempo = int(line.split()[1])
            continue
        eighths = 0
        for tok in line.replace("|", " ").split():
            m = TOKEN.match(tok)
            if not m:
                die("line %d: cannot read %r" % (lineno, tok))
            midi = 12 * (int(m[3]) + 1) + LETTER[m[1]] - (m[2] == "b")
            length = int(m[4])
            notes.append([midi, length, "~" in m[5], "v" in m[5], "h" in m[5]])
            eighths += length
        if "|" in line:
            bar += 1
            if eighths != 8:
                die("line %d: bar %d has %d eighths, not 8" % (lineno, bar, eighths))
    if tempo is None:
        die("no tempo")
    return tempo, notes


FOURTH = 5          # semitones: the harmony sits a perfect fourth down
HARMONY = "bar"     # "bar": one held note a bar; "long": moves on long notes
LONG_MIN = 3        # "long" mode: eighths a note needs for the harmony to move
HARMONY_REST = True # rest the voice for the last eighth before each fresh strike;
                    # False never re-strikes it: the pitch just changes
CONSONANT = (0, 3, 4, 5, 7, 8, 9)   # semitones above the harmony, mod 12


def clash(melody, harm):
    """Does the melody note sound badly over this harmony note?"""
    d = melody - harm
    return d < 3 or d % 12 not in CONSONANT


def whole_notes(notes):
    """One note a bar, struck on the bar line and held through it.

    Each bar's note is a perfect fourth below one of the bar's own melody
    notes: whichever leaves the least of the bar clashing, counting each
    melody note by how long it lasts. (A fourth below the first note alone
    puts a second under the long G of bar 2.)
    """
    bars, pos = [], 0
    for i, note in enumerate(notes):
        if pos % 8 == 0:
            bars.append([])
        bars[-1].append(i)
        pos += note[1]
    pitch, hold = [0] * len(notes), [True] * len(notes)
    for bar in bars:
        best = min((sum(notes[i][1] for i in bar if clash(notes[i][0], notes[j][0] - FOURTH)),
                    -notes[j][1], k, notes[j][0] - FOURTH)
                   for k, j in enumerate(bar))
        for i in bar:
            pitch[i] = best[3]
        hold[bar[0]] = False
    return pitch, hold


def harmony(notes):
    """The second voice, in parallel fourths below the tune.

    Returns (pitch, hold, release) for each note. "hold" tells the player not
    to strike the voice again on that note. "release" says to rest the voice for
    the last eighth of the note, because the next note strikes it afresh
    (unless the melody slurs into it, when the pitch just changes).
    """
    n = len(notes)
    if HARMONY == "bar":
        pitch, hold = whole_notes(notes)
    else:
        pitch, hold, cur = [], [], None
        for midi, length, tie, vib, force in notes:
            move = cur is None or length >= LONG_MIN or force
            if move:
                cur = midi - FOURTH
            pitch.append(cur)
            hold.append(not move)
    release = [HARMONY_REST and not notes[i][2] and not hold[(i + 1) % n]
               and notes[i][1] >= 2 for i in range(n)]
    return pitch, hold, release


def sid_freq(midi, clock):
    hz = 440.0 * 2 ** ((midi - 69) / 12)
    return round(hz * 2 ** 24 / clock)


# The player reads a note's two bytes with Y, so 128 notes is all an index can
# reach, and a pitch index is five bits.
MAX_PITCHES = 32
MAX_NOTES = 128


def main():
    tempo, notes = parse()
    harm, hold, release = harmony(notes)
    pitches = sorted({n[0] for n in notes} | set(harm))
    if len(pitches) > MAX_PITCHES:
        die("more than %d distinct pitches" % MAX_PITCHES)
    if len(notes) > MAX_NOTES:
        die("more than %d notes" % MAX_NOTES)
    # Frames in an eighth, the same on both machines.
    fpe = round(RATE * 30 / tempo)
    lo, hi = [], []
    for clock in (NTSC_CLOCK, PAL_CLOCK):
        for p in pitches:
            f = sid_freq(p, clock)
            lo.append(f & 255)
            hi.append(f >> 8)

    def bytes_(name, vals):
        return "%s:\n    .byte %s\n" % (name, ", ".join("$%02X" % v for v in vals))

    flat = []
    for midi, length, tie, vib, force in notes:
        flat += [pitches.index(midi) | (0x40 if tie else 0) | (0x80 if vib else 0), length]
    hflat = [pitches.index(h) | (0x80 if hd else 0) | (0x40 if r else 0)
             for h, hd, r in zip(harm, hold, release)]
    out = [
        "; GENERATED by tools/mkmusic.py from assets/theme.txt -- do not edit.",
        "",
        '        .segment "RODATA"',
        "        .export music_notes, music_harm, music_count, music_npitch, music_tempo",
        "        .export music_freq_lo, music_freq_hi",
        "",
        bytes_("music_count", [len(notes)]),
        bytes_("music_npitch", [len(pitches)]),
        "; frames to an eighth, at 60 calls a second",
        bytes_("music_tempo", [fpe]),
        "; NTSC's pitches, then PAL's",
        bytes_("music_freq_lo", lo),
        bytes_("music_freq_hi", hi),
        bytes_("music_notes", flat),
        "; the second voice, indexed by note: pitch, hold at bit 7, rest before the next strike at 6",
        bytes_("music_harm", hflat),
        "",
    ]
    open(OUT, "w").write("\n".join(out))
    print("music: %d notes, %d pitches, %d bytes" % (len(notes), len(pitches), len(flat) + len(hflat) + 2 * len(lo) + 4))


if __name__ == "__main__":
    main()
