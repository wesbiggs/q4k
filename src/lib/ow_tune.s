; Quest for the King -- the smooth-scrolling prototype's music: the game's
; tunes (SID files under the KERNAL, a header at $E7C0 and the tune at
; $E800-$FFF9: music.s and tools/mksid.py have the layout), played from an
; engine's raster interrupt instead of the KERNAL's timer.
;
; That interrupt comes once a frame, 50 times a second on PAL, where the
; game's comes 60 times a second everywhere. So the cadence is the other way
; round from music.s: a tune made for the frame rate (PSID "VBI") is called
; every time, and one made for 60 Hz ("CIA") gets a sixth call every fifth
; frame on PAL. The sound effects (sfx.s, voice 3) tick here too, every
; frame.
;
; ow_tune_irq is called by the interrupt (src/ow/smooth.s), after it has
; rewritten colour RAM, which races the beam and cannot wait: so on the
; frames the screen flips a note can come some 12 ms late with the CPU
; scrolling. It uses A, X and Y and no zero page, as that interrupt promises.

        .export _ow_tune_play, _ow_tune_stop, _ow_tune_on, ow_tune_irq
        .import vault_nmi, sfx_tick

SID_CTRL = $D404                ; voice 1's control register
SID_V2C  = $D40B                ; voice 2's
SID_VOL  = $D418
PALFLAG  = $02A6                ; the KERNAL's own: 0 NTSC, 1 PAL

TUNE_HDR = $E7C0
HDR_INIT = TUNE_HDR
HDR_PLAY = TUNE_HDR + 2
HDR_CIA  = TUNE_HDR + 6         ; a byte a song: 1 if it is called at 60 Hz
NO_KERNAL = $35                 ; RAM under the KERNAL, I/O in

        .segment "BSS"
_ow_tune_on: .res 1             ; 1 while a tune plays
t_extra: .res 1                 ; nonzero: a sixth call every five frames
t_tick:  .res 1

        .segment "CODE"

; void __fastcall__ ow_tune_play(unsigned char song): start that song (from
; 0) of the tune under the KERNAL, from the top.
_ow_tune_play:
        tax
        lda     #0
        sta     _ow_tune_on     ; the interrupt leaves it alone from here
        php
        sei
        lda     $01
        pha
        lda     #NO_KERNAL
        sta     $01
        lda     #<vault_nmi     ; an NMI while the KERNAL is out goes to the
        sta     $FFFA           ; KERNAL's handler all the same (vault.s)
        lda     #>vault_nmi
        sta     $FFFB
        lda     HDR_INIT
        sta     @init+1
        lda     HDR_INIT+1
        sta     @init+2
        lda     HDR_PLAY
        sta     call+1
        lda     HDR_PLAY+1
        sta     call+2
        lda     HDR_CIA,x
        and     PALFLAG         ; a 60 Hz tune at 50 frames a second
        sta     t_extra
        lda     #5
        sta     t_tick
        txa
        cld
@init:  jsr     $FFFF           ; the tune's init, song in A
        pla
        sta     $01
        plp
        inc     _ow_tune_on
        rts

; void ow_tune_stop(void): no more calls, and silence.
_ow_tune_stop:
        lda     #0
        sta     _ow_tune_on
        sta     SID_CTRL
        sta     SID_V2C
        sta     SID_VOL         ; (a tune's init turns it up again)
        rts

ow_tune_irq:
        jsr     sfx_tick
        lda     _ow_tune_on
        beq     t_out
        jsr     t_play
        lda     t_extra
        beq     t_out
        dec     t_tick
        bne     t_out
        lda     #5
        sta     t_tick
t_play: lda     $01
        pha
        lda     #NO_KERNAL
        sta     $01
        cld
call:   jsr     $FFFF           ; the tune's play
        pla
        sta     $01
t_out:   rts
