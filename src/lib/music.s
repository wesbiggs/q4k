; The theme, played on SID voices 1 and 2 from the frame interrupt, and the
; sound effects on voice 3.
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
; started. Pausing also waits a moment for the silence to be flushed out; see
; _music_pause. The serial bus is timing-sensitive and every IRQ here is cycles it
; does not have.

        .export _music_start, _music_stop, _music_pause, _music_resume
        .export _music_toggle, _music_state, _music_idx
        .export _sfx_play, _sfx_step, _sfx_toggle, _sfx_delay
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
SID_V3F  = $D40E                ; voice 3
SID_V3PW = $D410
SID_V3C  = $D412
SID_V3AD = $D413
SID_V3SR = $D414
SID_VOL  = $D418
PALFLAG  = $02A6                ; the KERNAL's own: 0 NTSC, 1 PAL

TRI_ON   = $11                  ; triangle, gate up
TRI_OFF  = $10
HARM_ON  = $11                  ; triangle, gate up
HARM_OFF = $10
VIB_WAIT = 20                   ; frames before the vibrato starts
VIB_LEN  = 12                   ; frames to a cycle of it
PAUSE_FRAMES = 12              ; jiffies music_pause waits for silence to be flushed
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
m_sx_on: .res 1                 ; an effect is playing
m_sx_pos: .res 1                ; the next step of it, in sfx_data
m_sx_left: .res 1               ; frames left in this one
m_sx_fhi: .res 1                ; its pitch, high byte
m_sx_dlt: .res 1                ; and how much that moves each frame
m_sx_bias: .res 1               ; how much sharper this one is: a step with the other foot
m_sx_age: .res 1                ; frames since the last effect began, to 255
m_fx_off: .res 1                ; the X key: no effects, and no waiting for them
m_foot:  .res 1                 ; 0 or 4: which foot the next step is

        .segment "HIDATA"
; The vibrato, in multiples of the frequency's high byte: about 1.2% at the
; peak, a fifth of a semitone.
vibtab: .byte 0, 2, 3, 3, 3, 2, 0, <-2, <-3, <-3, <-3, <-2

; The effects. A record is its attack/decay and sustain/release, then steps
; of four bytes -- frames, control (waveform and gate), pitch's high byte,
; and how that moves each frame -- and a zero. The envelope always climbs to
; full level when the gate goes up, so how loud an effect is comes down to
; its sustain level. (Closing the gate early to cut the climb short does not
; work: the attack's start is delayed by a varying amount, and the level
; wanders by 12 dB.)
; A record per sound, by id: the terrains first, then the blows. sfx_terrain
; says which a step onto each tile makes, by screen code from $60; the
; entry after the table, which anything beyond it gets, is the floor.
SFX_TERRAINS = 30
sfx_off: .byte sfx_floor - sfx_data, sfx_grass - sfx_data, sfx_forest - sfx_data
         .byte sfx_sand - sfx_data, sfx_swamp - sfx_data, sfx_rock - sfx_data
         .byte sfx_wood - sfx_data, sfx_melee - sfx_data, sfx_bow - sfx_data
sfx_terrain:
         .byte 1, 2, 0, 5, 0, 3, 4, 0, 0, 0, 6, 0, 0, 6, 0, 0   ; $60 grass...
         .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0       ; $70 ...the drawbridge
sfx_data:
; The steps. Each is quiet by its low sustain level, held for the length of the
; step; the two feet differ by a few steps of pitch, added when it plays.
; the floor, in town and in dungeons: a trudge, a breath of low noise
sfx_floor:  .byte $00, $26,  4, $81, $0A, 0,  3, $80, $0A, 0,  0
; grass: a soft rustle, higher and longer
sfx_grass:  .byte $00, $36,  6, $81, $20, <-2,  3, $80, $16, 0,  0
; forest: two crunches in a stride
sfx_forest: .byte $00, $36,  3, $81, $1A, <-2,  1, $80, $16, 0,  3, $81, $14, <-1,  2, $80, $10, 0,  0
; sand and shingle: a hiss
sfx_sand:   .byte $00, $46,  7, $81, $30, 0,  2, $80, $30, 0,  0
; swamp: a squelch that sinks
sfx_swamp:  .byte $00, $36,  3, $81, $12, <-2,  4, $81, $0C, <-1,  3, $80, $06, 0,  0
; a mountain pass: a hard tick and a short knock (a triangle, not a pulse,
; which at the same level is a good deal louder)
sfx_rock:   .byte $00, $16,  1, $81, $30, 0,  3, $11, $16, <-2,  2, $10, $0C, 0,  0
; boards: bridges and doors, a hollow tock
sfx_wood:   .byte $00, $16,  3, $11, $20, <-3,  3, $10, $10, 0,  0
; a blow: a crack of low noise that falls away. The gate stays up for the whole
; of it and the decay does the fading: the envelope can take up to 32 ms to
; start its attack when its rates change (the SID's ADSR delay bug), so a
; gate that closes early never reaches full level.
sfx_melee:  .byte $05, $00,  3, $81, $24, <-3,  8, $81, $12, <-1,  2, $80, $0C, 0,  0
; a bow hit: a tick of noise, then a woody thump falling in pitch
sfx_bow:    .byte $03, $04,  1, $81, $60, 0,  3, $41, $0C, <-2,  5, $40, $06, <-1,  0

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
        ldx #PAUSE_FRAMES       ; VICE, with the drive emulated, stops making
@wait:  lda $A2                 ; sound while the drive works and plays its last
@same:  cmp $A2                 ; buffer over and over: a note held for as long
        beq @same               ; as the disk takes. Let silence reach it first.
        dex
        bne @wait
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
        lda #TRI_OFF
        sta SID_CTRL
        lda #HARM_OFF
        sta SID_V2C
        rts

music_irq:
        jsr sfx_tick
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

SFX_GAP  = 15                   ; frames from one effect's start to the next's, in a fight

; void sfx_step(unsigned char tile): the sound of a step onto that tile. The
; feet take turns.
_sfx_step:
        sec
        sbc #$60
        cmp #SFX_TERRAINS
        bcc @in
        lda #SFX_TERRAINS
@in:    tax
        lda m_foot
        eor #4
        sta m_foot
        lda sfx_terrain,x
        ldx m_foot
        jmp play

; void sfx_play(unsigned char id): the id's record: SFX_MELEE or SFX_BOW. Works
; whether or not the music is on, but not with the effects off.
_sfx_play:
        ldx #0
play:   ldy m_fx_off
        bne @mute
        stx m_sx_bias
        tax
        lda #0
        sta m_sx_on
        sta m_sx_age
        sta SID_V3C             ; gate down, so a repeat is struck afresh
        lda sfx_off,x
        tay
        lda sfx_data,y
        sta SID_V3AD
        lda sfx_data+1,y
        sta SID_V3SR
        lda #0
        sta SID_V3PW
        lda #$08
        sta SID_V3PW+1          ; a square, for the records that are pulses
        lda #$0F
        sta SID_VOL
        iny
        iny
        sty m_sx_pos
        lda #1
        sta m_sx_left           ; the next frame starts the first step
        sta m_sx_on
@mute:  rts

; unsigned char sfx_toggle(void): the X key. Returns 1 if the effects are now on.
_sfx_toggle:
        lda m_fx_off
        eor #1
        sta m_fx_off
        beq @on
        lda #0
        sta m_sx_on
        sta SID_V3C             ; and quiet at once
        tax
        rts
@on:    lda #0                  ; a step, to show it
        jsr _sfx_play
        lda #1
        ldx #0
        rts

; void sfx_delay(void): wait until SFX_GAP frames after the last effect began,
; so that the next does not run into it. Nothing to wait for with them off.
_sfx_delay:
@wait:  lda m_fx_off
        bne @done
        lda m_sx_age
        cmp #SFX_GAP
        bcc @wait
@done:  rts

sfx_tick:
        lda m_sx_age
        cmp #$FF
        beq @aged
        inc m_sx_age
@aged:  lda m_sx_on
        beq @done
        dec m_sx_left
        bne @sweep
        ldy m_sx_pos
        lda sfx_data,y
        beq @end
        sta m_sx_left
        lda sfx_data+2,y
        clc
        adc m_sx_bias
        sta m_sx_fhi
        sta SID_V3F+1
        lda sfx_data+3,y
        sta m_sx_dlt
        lda #0
        sta SID_V3F
        lda sfx_data+1,y
        sta SID_V3C
        tya
        clc
        adc #4
        sta m_sx_pos
@done:  rts
@sweep: lda m_sx_fhi
        clc
        adc m_sx_dlt
        sta m_sx_fhi
        sta SID_V3F+1
        rts
@end:   lda #0
        sta SID_V3C
        sta m_sx_on
        rts
