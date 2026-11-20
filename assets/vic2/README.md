# vic2orious's libraries, version 6

`lib/v2i.inc` and `lib/v2t.inc` are vic2orious's image and text libraries
(Apache-2.0, `LICENSE`), as its author gave them: version 6 of the `.v2i`
format, which can show a picture in VIC bank 1, 2 or 3 (`v2i_bank`, read by
`v2i_init`) and relocates the picture's kernel for it. Do not edit them;
replace them with a newer copy.

They are used by the open world's talk (`src/ow/picshow.s`), which shows a
portrait of the speaker in bank 1 (see "The talk's portraits" in
CLAUDE.md). The font they draw with is not vic2orious's: it is the C64's
own lowercase set, `src/ow/vic2/font.inc`, made by `tools/mkv2tfont.py`.

The portraits are `assets/portraits/NAME-n.v2i` (NTSC) and `NAME-p.v2i`
(PAL), hires, 128 lines high at most, at the top left of the screen;
`PORTRAITS` in the Makefile says whose they are.

## To do

- **Regenerate the splash screen as version 6.** The splash
  (`src/lib/ow_splash.s`) still uses the splash kit's version 5 libraries
  and pictures (`assets/splash-kit`, bank 3). Make `splash-p` and `splash-n`
  again with vic2orious 6, move `ow_splash.s` onto `assets/vic2/lib`, and
  then the kit's `lib/` and `docs/` can go (keep what `tools/mksplash.py`
  reads). The text there is drawn with the kit's NLQ 12 font, which would
  move with it.
