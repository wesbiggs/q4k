; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the prototype's splash screen (include/q4k/ow.h).
;
; The picture and its text are the splash kit's (assets/splash-kit, made with
; vic2orious), shown with its libraries, v2i.inc and v2t.inc, as its README
; says to when the picture comes off the disk rather than out of the
; program: the bootstrap (src/openworld.c) loads SPLASH-P or SPLASH-N at
; OW_SPLASH_AT, the text and then the picture (tools/mksplash.py), and calls
; ow_splash(), which is the kit's splash_show() with only F1 (and SHIFT+F1)
; and F3 for keys and no time limit; with no saved game, its text is cut to
; "F1 Start New Game", centred, and F3 does nothing.
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

        .export _ow_video, _ow_splash, _ow_splash_ticks

TXT       = $4000               ; OW_SPLASH_AT: the .v2t, in a page
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

; unsigned char __fastcall__ ow_splash(unsigned char saved) -- show the
; picture loaded at OW_SPLASH_AT until F1, SHIFT+F1 or F3 is pressed and let
; go: OW_KEY_F1, OW_KEY_F2 or OW_KEY_F3, or 0 if what is there is not a
; picture for this machine. `saved` 0 (no saved game) cuts the text's line
; at its first run of two spaces ("F1 Start New Game"), centres what is
; left, and leaves F3 unread. The text screen, the KERNAL
; and the interrupts are back as they were. ow_splash_ticks is how many
; frames it took the player, less whole 256s: the one number at this point
; that is not the same on every run.
;
; For the tests, which type into the KERNAL's buffer and cannot press a key
; in the matrix: while it waits, OW->hook is OW_HOOK_WAIT, and anything
; else written there is taken as the key (tools/screendump.py --splash).
; Then ow_splash_ticks is 0, so that a test's new game is always the same.
_ow_splash:
        sta saved
        lda $02                 ; the picture's display code uses $02, which
        sta save02              ; is half of cc65's stack pointer
        lda #0
        sta key
        jsr v2i_init            ; KERNAL out, its own interrupt
        lda #<PIC
        ldy #>PIC
        jsr v2i_show
        bcs @done               ; not a .v2i, or not this machine's: key 0
        lda saved
        bne @all
        jsr lone                ; just F1, centred
        bcc @text
@all:   lda #<TXT
        ldy #>TXT
        jsr v2t_draw            ; (no text if that is not a .v2t)
@text:
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
; either SHIFT down (the left one column 1 row 7, as SHIFT LOCK is; the
; right one column 6 row 4) is F2, as the KERNAL has it. F3 is not read
; when there is no saved game.
scan:   lda #$fe
        sta $dc00
        lda $dc01
        and #$10
        beq @f1
        lda saved
        beq @none
        lda $dc01
        and #$20
        beq @f3
@none:  lda #0
        rts
@f1:    lda #$fd                ; the left SHIFT
        sta $dc00
        lda $dc01
        and #$80
        beq @f2
        lda #$bf                ; the right one
        sta $dc00
        lda $dc01
        and #$10
        beq @f2
        lda #KEY_F1
        rts
@f2:    lda #KEY_F2
        rts
@f3:    lda #KEY_F3
        rts

; With no saved game: the text's one line cut at its first run of two
; spaces (TXT+10 is its colour, then its glyph count and the glyphs: the
; kit's docs/v2t-format.md), and drawn in the middle of the screen instead
; of at its header's column, on the line's colour across the rows it
; touches (the picture leaves them as they were). C=1: not a .v2t.
lone:   ldx #0
@find:  cpx TXT+11
        bcs @cut
        lda TXT+12,x
        cmp #$20
        bne @next
        lda TXT+13,x            ; (past the last glyph: the page's padding)
        cmp #$20
        beq @cut
@next:  inx
        bne @find
@cut:   stx TXT+11
        lda #0                  ; its width, in pixels
        sta width
        sta width+1
        tax
@sum:   cpx TXT+11
        beq @wide
        lda TXT+12,x
        and #$7f                ; (as v2t.inc draws 128-255)
        sec
        sbc #$20
        bcs :+
        lda #0                  ; (and below 32, a space)
:       tay
        lda v2t_font_w,y
        clc
        adc width
        sta width
        bcc :+
        inc width+1
:       inx
        bne @sum
@wide:  sec                     ; x = (320 - width) / 2
        lda #<320
        sbc width
        sta v2t_x
        lda #>320
        sbc width+1
        lsr
        sta v2t_x+1
        ror v2t_x
        lda TXT+5               ; y: the header's row
        sta v2t_row
        asl
        asl
        asl
        sta v2t_y
        lda TXT+9               ; its leading
        sta v2t_lead
        lda #0                  ; the rows from there down, every column,
        sta v2t_col             ; cleared to the line's colour
        lda #40
        sta v2t_cols
        lda #25
        sec
        sbc v2t_row
        sta v2t_rows
        lda TXT+10
        jsr v2t_clear
        lda #<TXT
        ldy #>TXT
        jmp v2t_draw_at

save02:           .byte 0
key:              .byte 0
saved:            .byte 0
width:            .word 0
_ow_splash_ticks: .byte 0

        .include "v2i.inc"
        .include "v2t.inc"
