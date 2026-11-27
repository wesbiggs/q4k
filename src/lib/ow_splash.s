; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the prototype's splash screen (include/q4k/ow.h).
;
; The picture and its text are the splash kit's (assets/splash-kit, made with
; vic2orious), shown with its libraries, v2i.inc and v2t.inc, as its README
; says to when the picture comes off the disk rather than out of the
; program: the bootstrap (src/openworld.c) loads SPLASH-P or SPLASH-N at
; OW_SPLASH_AT, the text and then the picture (tools/mksplash.py), and calls
; ow_splash(), which is the kit's splash_show() with only F1 (and SHIFT+F1)
; and F3 for keys and no time limit; with no saved game, its text is only
; "F1 Start New Game", centred.
;
; While the picture is up the KERNAL is out, the picture's raster interrupt
; has every sprite and most of the frame, and VIC bank 3, $C000-$FFFF, is
; the picture: so nothing can be loaded meanwhile, and the lands, which are
; built into that same RAM, are built afterwards. The bootstrap keeps its
; BSS out of there (cfg/c64-owboot.cfg).
;
; Assembled with -t none (the Makefile has a rule for it): the libraries
; compare the files' magic with ASCII strings, which -t c64 would turn into
; PETSCII.

        .export _ow_video, _ow_splash, _ow_splash_ticks, _ow_saved

TXT       = $4000               ; OW_SPLASH_AT: the .v2t's line's first
                                ; part alone, centred, for no saved game,
                                ; then at TXT + $80 the .v2t, each in half
                                ; a page (tools/mksplash.py)
PIC       = TXT + $100          ; and the .v2i
HOOK      = $02A7 + 62          ; OW->hook: openworld.c checks the offset
HOOK_WAIT = $A5                 ; OW_HOOK_WAIT
KEY_F1    = $85
KEY_F3    = $86
KEY_F2    = $89                 ; SHIFT+F1

        .segment "CODE"

; unsigned char ow_video(void) -- OW_PAL (0), OW_NTSC (1), or $FF for a
; VIC-II the pictures were not made for. With the KERNAL in, any time.
_ow_video:
        jsr v2i_detect
        lda v2i_system
        ldx #0
        rts

; unsigned char ow_splash(void) -- show the picture loaded at OW_SPLASH_AT
; until F1, SHIFT+F1 or F3 is pressed and let go: OW_KEY_F1, OW_KEY_F2 or
; OW_KEY_F3, or 0 if what is there is not a picture for this machine.
; ow_saved 0 (no saved game) shows the text that mksplash.py puts first,
; "F1 Start New Game" alone and centred; ow_saved $80, which load_save()
; answers when there is one, is the whole text's offset. The text screen, the KERNAL
; and the interrupts are back as they were. ow_splash_ticks is how many
; frames it took the player, less whole 256s: the one number at this point
; that is not the same on every run.
;
; For the tests, which type into the KERNAL's buffer and cannot press a key
; in the matrix: while it waits, OW->hook is OW_HOOK_WAIT, and anything
; else written there is taken as the key (tools/screendump.py --splash).
; Then ow_splash_ticks is 0, so that a test's new game is always the same.
_ow_splash:
        lda $02                 ; the picture's display code uses $02, which
        sta save02              ; is half of cc65's stack pointer
        lda #0
        sta key
        jsr v2i_init            ; KERNAL out, its own interrupt
        lda #<PIC
        ldy #>PIC
        jsr v2i_show
        bcs @done               ; not a .v2i, or not this machine's: key 0
        lda _ow_saved           ; $80: all of it; 0: just F1, centred
        ldy #>TXT
        jsr v2t_draw            ; (no text if that is not a .v2t)
        jsr v2i_start
        lda #HOOK_WAIT
        sta HOOK
:       jsr scan                ; a key held from before does not count
        bne :-
@frame: lda v2i_ticks
:       cmp v2i_ticks           ; once a frame
        beq :-
        lda HOOK
        cmp #HOOK_WAIT
        bne @hook
        jsr scan
        beq @frame
        sta key
        lda v2i_ticks
        sta _ow_splash_ticks
:       jsr scan                ; until it is let go, so that nothing after
        bne :-                  ; the splash sees it
        beq @done
@hook:  sta key
        lda #0
        sta _ow_splash_ticks
@done:  jsr v2i_exit
        lda save02
        sta $02
        lda #0
        sta HOOK
        tax
        lda key
        rts

; A = OW_KEY_F1, OW_KEY_F2 or OW_KEY_F3 if that key is down, else 0 (Z
; set). F1 and F3 are in column 0 of the matrix, F1 on row 4 and F3 on row
; 5 (so is the button of a joystick in port 1, which reads as F1); F1 with
; the left SHIFT or SHIFT LOCK down (column 1, row 7) is F2, as the KERNAL
; has it. (Not the right SHIFT: row 4, as the joystick's button is, which
; would make it F2.)
scan:   lda #$fe
        sta $dc00
        lda $dc01
        and #$10
        beq @f1
        lda $dc01
        and #$20
        eor #$20                ; 0, Z set: not F3 either
        beq @no
        lda #KEY_F3
@no:    rts
@f1:    lda #$fd                ; the left SHIFT
        sta $dc00
        lda $dc01
        bpl @f2
        lda #KEY_F1
        rts
@f2:    lda #KEY_F2
        rts

; The bootstrap's MAIN ends in OWDISK's load image, which disk_resident()
; copies out only after cc65's startup has pushed main()'s argc and argv on
; the C stack, the four bytes below $4000: it has to stop short of them, as
; an engine does (src/ow/smooth.s). Here because only the bootstrap links
; this. (The linker says so.)
        .import __MAIN_LAST__
        .assert __MAIN_LAST__ < $4000 - 4, lderror, "the bootstrap's MAIN runs into the four bytes cc65's startup pushes at its top"

save02:           .byte 0
key:              .byte 0
_ow_saved:        .byte 0
_ow_splash_ticks: .byte 0

        .include "v2i.inc"
        .include "v2t.inc"
