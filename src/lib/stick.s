; Wait for a key or a joystick in port 2.
;
; stick_key() returns what cgetc() would for a key, and for the joystick
; the cursor key of its direction, or STICK_FIRE for the button. A
; direction repeats while it is held: after a quarter of a second, then
; eight times a second. Only one is reported at a time (up, down, left,
; right, fire, in that order), so a diagonal counts as up or down, and fire
; with a direction as the direction. The button does not repeat: hold it
; and it still means one shot, or one stair.
;
; The stick is read once a jiffy, which is the debounce: a switch that
; bounces settles well inside 16 ms. stick_spins counts the times round the
; wait, for rng_stir() (see wait_key() in q4k.c).
;
; Port 2 because port 1 shares its lines with the keyboard's rows, and a
; stick there types phantom keys. $DC00 is the keyboard's column latch too,
; but the KERNAL's scan leaves it at $7F, so bits 0-4 read the stick alone.

        .export _stick_key, _stick_spins
        .import _kbhit, _cgetc

CIA1_PRA = $DC00
JIFFY    = $A2

FIRST    = 15                           ; jiffies before a held move repeats
AGAIN    = 7                            ; and between repeats
STICK_FIRE = $FE                        ; as in stick.h

        .segment "HICODE"

_stick_key:
        lda     #0
        sta     _stick_spins
        sta     _stick_spins+1
wait:   jsr     _kbhit
        beq     :+
        jmp     _cgetc
:       inc     _stick_spins
        bne     :+
        inc     _stick_spins+1
:       lda     JIFFY
        cmp     tick
        beq     wait
        sta     tick
        lda     CIA1_PRA
        eor     #$FF                    ; the lines are pulled low
        ldx     #0                      ; which of the five, or 5 for none
scan:   lsr     a
        bcs     got
        inx
        cpx     #5
        bne     scan
got:    lda     codes,x
        cmp     last
        sta     last
        bne     fresh
        cmp     #0                      ; nothing, as before
        beq     wait
        cmp     #STICK_FIRE             ; and the button does not repeat
        beq     wait
        lda     JIFFY                   ; held: is it time to repeat?
        sec
        sbc     due
        bmi     wait
        lda     #AGAIN
        bne     arm
fresh:  cmp     #0                      ; let go
        beq     wait
        lda     #FIRST
arm:    clc
        adc     JIFFY
        sta     due
        lda     last
        ldx     #0
        rts

        ; up, down, left, right as the cursor keys; fire; nothing
codes:  .byte   $91, $11, $9D, $1D, STICK_FIRE, 0

_stick_spins:
        .word   0
tick:   .byte   0
last:   .byte   0
due:    .byte   0
