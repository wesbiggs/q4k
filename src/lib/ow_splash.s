; Quest for the King -- the prototype's splash screen (include/q4k/ow.h).
;
; The picture and its text are the splash kit's (assets/splash-kit, made with
; vic2orious), shown with its libraries, v2i.inc and v2t.inc, as its README
; says to when the picture comes off the disk rather than out of the
; program: the bootstrap (src/openworld.c) loads SPLASH-P or SPLASH-N at
; OW_SPLASH_AT, the text and then the picture (tools/mksplash.py), and calls
; ow_splash(), which is the kit's splash_show() with only F1 and F3 for keys
; and no time limit.
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

        .segment "CODE"

; unsigned char ow_video(void) -- OW_PAL (0), OW_NTSC (1), or $FF for a
; VIC-II the pictures were not made for. With the KERNAL in, any time.
_ow_video:
        jsr v2i_detect
        lda v2i_system
        ldx #0
        rts

; unsigned char ow_splash(void) -- show the picture loaded at OW_SPLASH_AT
; until F1 or F3 is pressed and let go: OW_KEY_F1 or OW_KEY_F3, or 0 if what
; is there is not a picture for this machine. The text screen, the KERNAL
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
        lda #<TXT
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

; A = OW_KEY_F1 or OW_KEY_F3 if that key is down, else 0 (Z set). Both are
; in column 0 of the matrix, F1 on row 4 and F3 on row 5. (So is the button
; of a joystick in port 1, which reads as F1.)
scan:   lda #$fe
        sta $dc00
        lda $dc01
        and #$10
        beq @f1
        lda $dc01
        and #$20
        beq @f3
        lda #0
        rts
@f1:    lda #KEY_F1
        rts
@f3:    lda #KEY_F3
        rts

save02:           .byte 0
key:              .byte 0
_ow_splash_ticks: .byte 0

        .include "v2i.inc"
        .include "v2t.inc"
