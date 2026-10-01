; The theme, played on SID voices 1 and 2 from the frame interrupt.
;
; The score is two bytes a note in music_data.s (generated from
; assets/theme.txt): pitch index, slur flag at bit 6, vibrato flag at bit 7,
; then the length in eighths. cc65's interruptor chain calls music_irq from
; the KERNAL's 50/60 Hz IRQ, so this is pure assembly that touches only A, X
; and Y, and none of the zero page the C code keeps its temporaries in.
;
; The voice is a triangle with a soft attack, the nearest SID gets to a
; pipe. Voice 2 is a quiet triangle a fourth below it that moves only on the
; long notes, holding between them; the generator picks its pitches and says
; which notes hold (bit 7 of music_harm) and which are followed by a rest in
; this voice (bit 6): it lets go an eighth before the note ends. A slurred note changes pitch without taking the gate down, so there
; is no new attack; any other note lets go three frames early so that the
; next one speaks afresh. Vibrato waits a third of a second, then swings the
; pitch about 20 cents either way at 5 Hz.
;
; State, so that the disk code can step out of the way: 0 stopped, 1
; playing, 2 paused. music_pause()/music_resume() do nothing in any other
; state, which lets savegame.c call them whether or not the music has
; started. The serial bus is timing-sensitive and every IRQ here is cycles it
; does not have.

        .export _music_start, _music_stop, _music_pause, _music_resume
        .export _music_toggle, _music_state, _music_idx
        .import music_notes, music_harm, music_count, music_npitch, music_tempo
        .import music_freq_lo, music_freq_hi
        .interruptor music_irq

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
HARM_ON  = $11                  ; triangle, gate up
HARM_OFF = $10
VIB_WAIT = 20                   ; frames before the vibrato starts
VIB_LEN  = 12                   ; frames to a cycle of it
LET_GO   = 3                    ; frames of release before an unslurred note ends

        .segment "BSS"
_music_state: .res 1
_music_idx:   .res 1                 ; the next note to start
m_left:  .res 1                 ; frames until the current one ends
m_age:   .res 1                 ; frames since it began
m_cur:   .res 1                 ; its flags byte
m_flo:   .res 1                 ; its pitch, before vibrato
m_fhi:   .res 1
m_dlt:   .res 1
m_vph:   .res 1                 ; where it is in the vibrato cycle
m_fpe:   .res 1                 ; frames to an eighth, for this machine's clock
m_hflg:  .res 1                 ; the second voice's flags for the current note
m_force: .res 1                 ; strike it even if it is held: after a pause
m_poff:  .res 1                 ; where this machine's half of the pitch table starts

        .segment "HIDATA"
; The vibrato, in multiples of the frequency's high byte: about 1.2% at the
; peak, a fifth of a semitone.
vibtab: .byte 0, 2, 3, 3, 3, 2, 0, <-2, <-3, <-3, <-3, <-2

        .segment "HICODE"

; void music_start(void): from the top, on this machine's clock.
_music_start:
        lda #0
        sta _music_state
        ldx #0
        lda PALFLAG
        beq @clock
        ldx #1
@clock: lda music_tempo,x
        sta m_fpe
        lda #0
        cpx #0
        beq @off
        lda music_npitch
@off:   sta m_poff
        ldx #$18                ; every register, so nothing is left from BASIC
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
        lda #0
        sta _music_idx
        lda #2
        sta _music_state
        ; fall into resume, which starts a paused tune on its next note

; void music_resume(void)
_music_resume:
        lda _music_state
        cmp #2
        bne @no
        lda #0
        sta m_cur               ; no slur from before the pause
        lda #1
        sta m_left              ; the next frame starts the next note
        sta _music_state
        lda #$0F
        sta SID_VOL
@no:    rts

; void music_pause(void)
_music_pause:
        lda _music_state
        cmp #1
        bne @no
        lda #2
        sta _music_state
        lda #0
        sta SID_VOL
        lda #TRI_OFF
        sta SID_CTRL
        lda #HARM_OFF
        sta SID_V2C
@no:    rts

; unsigned char music_toggle(void): the M key. Off if it was playing, from the
; top if it was not; returns 1 for on. A stopped tune stays stopped through
; the disk's pause and resume, which only touch one that is playing.
_music_toggle:
        lda _music_state
        beq @on
        jsr _music_stop
        lda #0
        tax
        rts
@on:    jsr _music_start
        lda #1
        ldx #0
        rts

; void music_stop(void)
_music_stop:
        lda #0
        sta _music_state
        sta SID_VOL
        lda #TRI_OFF
        sta SID_CTRL
        lda #HARM_OFF
        sta SID_V2C
        rts

music_irq:
        lda _music_state
        cmp #1
        beq @run
        clc
        rts
@run:   dec m_left
        bne @mid
        jmp @next               ; (out of a branch's reach, now)
@mid:   inc m_age
        lda m_hflg              ; the second voice rests for the bar's last eighth
        and #$40
        beq @mel
        lda m_left
        cmp m_fpe
        bne @mel
        lda #HARM_OFF
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
        bcc @out                ; (always taken: the sum cannot carry past $FF)
@down:  lda m_flo
        sec
        sbc m_dlt
        sta SID_FLO
        lda m_fhi
        sbc #0
        sta SID_FLO+1
@out:   clc                     ; not ours alone: let the KERNAL have it too
        rts

@next:  lda _music_idx
        asl
        tay
        lda music_notes,y
        sta m_cur
        lda music_notes+1,y     ; its length, in eighths, times the frames in one
        tax
        lda #0
@len:   clc
        adc m_fpe
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
        ldx _music_idx          ; and the voice below it
        lda music_harm,x
        sta m_hflg
        bpl @strike             ; bit 7: a short note holds what is sounding
        lda m_force
        beq @held
@strike:
        and #$1F
        clc
        adc m_poff
        tax
        lda music_freq_lo,x
        sta SID_V2F
        lda music_freq_hi,x
        sta SID_V2F+1
        lda #HARM_ON
        sta SID_V2C
@held:  lda #0
        sta m_force
        lda #TRI_ON             ; already up after a slur, which is the point
        sta SID_CTRL
        lda #0
        sta m_age
        sta m_vph
        ldx _music_idx
        inx
        cpx music_count
        bcc @keep
        ldx #0                  ; and round again
@keep:  stx _music_idx
        clc
        rts
