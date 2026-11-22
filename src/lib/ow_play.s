; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the open world's music on its screens of text.
;
; An engine pauses the tune where it is when it leaves for owtown (the talk,
; the shops, the inns, the party, the help: src/owtown.c), and says which
; song in the hand-off block (OWB->song, 1 + the song, q4k/ow.h). The tune
; keeps its place in its own memory, under the KERNAL, so this carries on
; with it -- the same play routine, called from the KERNAL's interrupt -- and
; pauses it again, where it is, before the engine is loaded back, which then
; carries on in turn. So the theme plays on through a talk instead of
; stopping on a held note and starting again from the top.
;
; The KERNAL's interrupt is 60 Hz on PAL and NTSC alike (music.s has the
; arithmetic): a tune made for 60 Hz is called every time, one made for the
; frame rate five times in six on PAL. The play routine runs with the KERNAL
; out ($01 = $35), so an NMI then reads its vector from the RAM under it:
; that points at owt_nmi, which runs the KERNAL's handler with the KERNAL in
; (vault.s's vault_nmi, again, here in CODE: owtown's HIGH is where a fight
; loads its module, which plays the tune itself, from its own interrupt).
;
; Pausing waits twelve jiffies for the silence to reach VICE's sound buffer
; before the drive works (music.s's music_pause: otherwise VICE holds the
; last note for as long as the disk takes).

        .export _owt_music_on, _owt_music_pause, _owt_music_resume
        .export _owt_music_off, _owt_music_frame, _owt_nowait

SID_VOL  = $D418
PALFLAG  = $02A6
TUNE_HDR = $E7C0
HDR_PLAY = TUNE_HDR + 2
HDR_VOL  = TUNE_HDR + 5
HDR_CIA  = TUNE_HDR + 6
NO_KERNAL = $35
OWB_SONG = $02F0                ; OWB->song (ow.h: OWB is at $02ED)

        .segment "BSS"
state:  .res 1                  ; 0 off, 1 playing, 2 paused
skip:   .res 1                  ; nonzero: one call in six is skipped
tick:   .res 1
vol:    .res 1
xtra:   .res 1                  ; nonzero: owt_music_frame plays six in five
tick5:  .res 1
_owt_nowait: .res 1             ; nonzero: no wait (the REU, not the drive)

        .segment "CODE"

; void owt_music_on(void): carry on with the song OWB->song names, if any.
_owt_music_on:
        ldx     OWB_SONG
        beq     @no
        dex                     ; the song, from 0
        sei
        lda     $01
        pha
        lda     #NO_KERNAL
        sta     $01
        lda     #<owt_nmi
        sta     $FFFA
        lda     #>owt_nmi
        sta     $FFFB
        lda     HDR_PLAY
        sta     call+1
        sta     play+1
        lda     HDR_PLAY+1
        sta     call+2
        sta     play+2
        lda     HDR_VOL
        sta     vol
        ldy     #0              ; 60 Hz: every call; the frame rate on PAL:
        sty     xtra            ; five in six (and by the frame, a 60 Hz
        lda     HDR_CIA,x       ; one six times in five frames on PAL)
        beq     @vbi
        lda     PALFLAG
        sta     xtra
        jmp     :+
@vbi:   lda     PALFLAG
        beq     :+
        iny
:       sty     skip
        lda     #5
        sta     tick5
        pla
        sta     $01
        lda     #6
        sta     tick
        lda     $0314           ; in front of whatever the interrupt was
        sta     chain+1
        lda     $0315
        sta     chain+2
        lda     #<irq
        sta     $0314
        lda     #>irq
        sta     $0315
        lda     vol
        sta     SID_VOL
        lda     #1
        sta     state
        cli
@no:    rts

; void owt_music_pause(void): silence, by the volume, and the place kept.
_owt_music_pause:
        lda     state
        cmp     #1
        bne     @no
        lda     #2
        sta     state
        lda     #0
        sta     SID_VOL
        ldx     _owt_nowait
        bne     @no
        ldx     #12
@wait:  lda     $A2
@same:  cmp     $A2
        beq     @same
        dex
        bne     @wait
@no:    rts

; void owt_music_resume(void)
_owt_music_resume:
        lda     state
        cmp     #2
        bne     @no
        lda     #1
        sta     state
        lda     vol
        sta     SID_VOL
@no:    rts

; void owt_music_off(void): paused where it is, for the engine to carry on
; with (OWB->song is left as it is), and the interrupt as it was.
_owt_music_off:
        jsr     _owt_music_pause
        lda     state
        beq     @no
        sei
        lda     chain+1
        sta     $0314
        lda     chain+2
        sta     $0315
        lda     #0
        sta     state
        cli
@no:    rts

irq:    lda     state
        cmp     #1
        bne     chain
        lda     skip
        beq     @play
        dec     tick
        bne     @play
        lda     #6              ; the sixth: skipped
        sta     tick
        bne     chain
@play:  lda     $01
        pha
        lda     #NO_KERNAL
        sta     $01
        cld
call:   jsr     $FFFF           ; the tune's play
        pla
        sta     $01
chain:  jmp     $EA31           ; (what the interrupt was)

; void owt_music_frame(void): the tune's turn, once a frame, from an
; interrupt that is not the KERNAL's and runs with it out ($01 = $35): the
; talk's portrait (src/ow/picshow.s), which turns the KERNAL's off. Every
; frame, and on PAL a tune made for 60 Hz twice on every fifth.
_owt_music_frame:
        lda     state
        cmp     #1
        bne     fr_no
        cld
        lda     xtra
        beq     @one
        dec     tick5
        bne     @one
        lda     #5
        sta     tick5
        jsr     play
@one:
play:   jmp     $FFFF           ; the tune's play (its rts returns from here)
fr_no:  rts

; An NMI while the KERNAL is out: vault.s's vault_nmi.
owt_nmi:
        pha
        lda     $01
        pha
        and     #$F8
        ora     #$06
        sta     $01
        lda     #>@back
        pha
        lda     #<@back
        pha
        php
        jmp     ($0318)
@back:  pla
        sta     $01
        pla
        rti
