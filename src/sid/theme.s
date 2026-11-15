; The game's theme as a SID file: the player that used to be in
; src/lib/music.s, on its own, with the score (theme_data.s, generated from
; assets/theme.txt by tools/mkmusic.py) beside it. cfg/sid.cfg links the two
; at $E800, where the game keeps a tune (src/lib/music.s), and
; tools/mksid.py wraps the result in a PSID header: build/sid/theme.sid,
; which plays in any SID player as well as in the game.
;
; The usual two entry points: init, with the song in A (there is only one,
; so A is ignored), and play, called 60 times a second (the header's speed
; flag says CIA, the PSID convention for that). It uses voices 1 and 2 only,
; since the game's sound effects have voice 3, and no zero page. Everything
; it keeps is in its own BSS, after the code, which init sets up.
;
; The score is two bytes a note: pitch index, slur flag at bit 6, vibrato flag
; at bit 7, then the length in eighths. The voice is a triangle with a soft
; attack, the nearest SID gets to a pipe. Voice 2 is a quiet triangle a fourth
; below it that moves only on the long notes, holding between them; the
; generator picks its pitches and says which notes hold (bit 7 of music_harm)
; and which are followed by a rest in this voice (bit 6): it lets go an eighth
; before the note ends. A slurred note changes pitch without taking the gate
; down, so there is no new attack; any other note lets go three frames early
; so that the next one speaks afresh. Vibrato waits a third of a second, then
; swings the pitch about 20 cents either way at 5 Hz.

        .export init, play
        .import music_notes, music_harm, music_count, music_npitch, music_tempo
        .import music_freq_lo, music_freq_hi

SID_FLO  = $D400                ; voice 1 frequency, low then high
SID_CTRL = $D404
SID_AD   = $D405
SID_SR   = $D406
SID_V2F  = $D407                ; voice 2, the same registers seven on
SID_V2C  = $D40B
SID_V2AD = $D40C
SID_V2SR = $D40D
SID_VOL  = $D418
PALFLAG  = $02A6                ; the KERNAL's own: 0 NTSC, 1 PAL

TRI_ON   = $11                  ; triangle, gate up
TRI_OFF  = $10
VIB_WAIT = 20                   ; frames before the vibrato starts
VIB_LEN  = 12                   ; frames to a cycle of it
LET_GO   = 3                    ; frames of release before an unslurred note ends

        .segment "BSS"
m_idx:   .res 1                 ; the next note to start
m_left:  .res 1                 ; frames until the current one ends
m_age:   .res 1                 ; frames since it began
m_cur:   .res 1                 ; its flags byte
m_flo:   .res 1                 ; its pitch, before vibrato
m_fhi:   .res 1
m_dlt:   .res 1
m_vph:   .res 1                 ; where it is in the vibrato cycle
m_hflg:  .res 1                 ; the second voice's flags for the current note
m_poff:  .res 1                 ; where this machine's half of the pitch table starts

        .segment "RODATA"
; The vibrato, in multiples of the frequency's high byte: about 1.2% at the
; peak, a fifth of a semitone.
vibtab: .byte 0, 2, 3, 3, 3, 2, 0, <-2, <-3, <-3, <-3, <-2

        .segment "CODE"

; init: from the top, on this machine's clock.
init:   lda #0
        ldx PALFLAG
        beq @off
        lda music_npitch        ; PAL: the second half of the pitch table
@off:   sta m_poff
        ldx #SID_V2SR - SID_FLO ; voices 1 and 2, so nothing is left over
        lda #0
@wipe:  sta SID_FLO,x
        dex
        bpl @wipe
        lda #$43                ; attack 38 ms, decay 72 ms
        sta SID_AD
        lda #$C3                ; sustain 12/15, release 72 ms
        sta SID_SR
        lda #$88                ; attack 100 ms, decay 300 ms: the envelope always
        sta SID_V2AD            ; climbs to full before settling to a quiet sustain,
                                ; so a quick attack is a click and this is a swell
        lda #$23                ; sustain 2/15: far under the lead
        sta SID_V2SR
        lda #$0F
        sta SID_VOL
        lda #0
        sta m_idx
        sta m_cur               ; no slur into the first note
        sta m_hflg
        lda #1
        sta m_left              ; the first call starts the first note
        rts

play:   dec m_left
        bne @mid
        jmp @next               ; (out of a branch's reach)
@mid:   inc m_age
        lda m_hflg              ; the second voice rests for the bar's last eighth
        and #$40
        beq @mel
        lda m_left
        cmp music_tempo
        bne @mel
        lda #TRI_OFF
        sta SID_V2C
@mel:   lda m_cur
        and #$40                ; slurred: keep the gate up to the end
        bne @vib
        lda m_left
        cmp #LET_GO
        bne @vib
        lda #TRI_OFF
        sta SID_CTRL
@vib:   lda m_cur
        bpl @out
        lda m_age
        cmp #VIB_WAIT
        bcc @out
        ldx m_vph
        inx
        cpx #VIB_LEN
        bcc @step
        ldx #0
@step:  stx m_vph
        lda vibtab,x
        tay                     ; Y keeps the sign
        bpl @mag
        eor #$FF
        clc
        adc #1
@mag:   tax
        lda #0
        cpx #0
        beq @got
@mul:   clc
        adc m_fhi
        dex
        bne @mul
@got:   sta m_dlt
        tya
        bmi @down
        lda m_flo
        clc
        adc m_dlt
        sta SID_FLO
        lda m_fhi
        adc #0
        sta SID_FLO+1
        rts
@down:  lda m_flo
        sec
        sbc m_dlt
        sta SID_FLO
        lda m_fhi
        sbc #0
        sta SID_FLO+1
@out:   rts

@next:  lda m_idx
        asl
        tay
        lda music_notes,y
        sta m_cur
        lda music_notes+1,y     ; its length, in eighths, times the frames in one
        tax
        lda #0
@len:   clc
        adc music_tempo
        dex
        bne @len
        sta m_left
        lda m_cur
        and #$1F
        clc
        adc m_poff
        tax
        lda music_freq_lo,x
        sta m_flo
        sta SID_FLO
        lda music_freq_hi,x
        sta m_fhi
        sta SID_FLO+1
        ldx m_idx               ; and the voice below it
        lda music_harm,x
        sta m_hflg
        bmi @held               ; bit 7: a short note holds what is sounding
        and #$1F
        clc
        adc m_poff
        tax
        lda music_freq_lo,x
        sta SID_V2F
        lda music_freq_hi,x
        sta SID_V2F+1
        lda #TRI_ON
        sta SID_V2C
@held:  lda #TRI_ON             ; already up after a slur, which is the point
        sta SID_CTRL
        lda #0
        sta m_age
        sta m_vph
        ldx m_idx
        inx
        cpx music_count
        bcc @keep
        ldx #0                  ; and round again
@keep:  stx m_idx
        rts
