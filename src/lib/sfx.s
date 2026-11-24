; SPDX-License-Identifier: GPL-3.0-or-later
; The sound effects, on voice 3: a step by terrain, a blow, a shot.
;
; Played from whichever interrupt the program has: the game's music
; interrupt (music.s calls sfx_tick each jiffy), or the open world's raster
; interrupt (src/lib/ow_tune.s, once a frame). Assembled twice: into the
; library for the game, its code and table in HICODE/HIDATA, and with
; OW_SFX=1 for the open world's engines (build/ow/ow_sfx.o, in CODE and
; RODATA, linked ahead of the library), where the tune's flag is
; ow_tune_on rather than music_state. Either way: A, X and Y only, no zero
; page, from the interrupt.

        .export _sfx_play, _sfx_step, _sfx_toggle, _sfx_delay, sfx_tick
        .export _sfx_on

.ifdef OW_SFX
        .import _ow_tune_on
TUNE_ON = _ow_tune_on
.else
        .import _music_state
TUNE_ON = _music_state
.endif

SID_V3F  = $D40E                ; voice 3, the effects'
SID_V3PW = $D410
SID_V3C  = $D412
SID_V3AD = $D413
SID_V3SR = $D414
SID_VOL  = $D418

        .segment "BSS"
m_sx_on: .res 1                 ; an effect is playing
m_sx_pos: .res 1                ; the next step of it, in sfx_data
m_sx_left: .res 1               ; frames left in this one
m_sx_fhi: .res 1                ; its pitch, high byte
m_sx_dlt: .res 1                ; and how much that moves each frame
m_sx_bias: .res 1               ; how much sharper this one is: a step with the other foot
m_sx_age: .res 1                ; frames since the last effect began, to 255
m_fx_off: .res 1                ; the X key: no effects, and no waiting for them
m_foot:  .res 1                 ; 0 or 4: which foot the next step is

.ifdef OW_SFX
        .segment "RODATA"
.else
        .segment "HIDATA"
.endif
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
         .byte sfx_no - sfx_data, sfx_drink - sfx_data, sfx_level - sfx_data
         .byte sfx_die - sfx_data
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
; no way through: two low, dull buzzes (the open world's word for "not yet")
sfx_no:     .byte $00, $46,  5, $41, $05, 0,  3, $40, $05, 0,  5, $41, $04, 0,  3, $40, $04, 0,  0
; a draught: three rising gulps of triangle
sfx_drink:  .byte $00, $56,  4, $11, $0C, <1,  2, $10, $0C, 0,  4, $11, $10, <1,  2, $10, $10, 0,  4, $11, $14, <1,  3, $10, $14, 0,  0
; a level: a bright arpeggio of triangle, C E G and the C above, held
sfx_level:  .byte $00, $96,  5, $11, $22, 0,  5, $11, $2B, 0,  5, $11, $33, 0,  14, $11, $44, 0,  6, $10, $44, 0,  0
; a foe falls: a sawtooth groan sliding down, and a last thud of noise
sfx_die:    .byte $00, $86,  12, $21, $18, <-1,  6, $81, $07, 0,  6, $80, $07, 0,  0

.ifdef OW_SFX
        .segment "CODE"
.else
        .segment "HICODE"
.endif

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
        ldy TUNE_ON             ; the volume is the tune's while it plays (or
        bne @vol                ; is paused, when it is silence)
        lda #$0F
        sta SID_VOL
@vol:   lda sfx_off,x
        tay
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
; unsigned char sfx_on(void): 1 if the effects are on.
_sfx_on:
        lda m_fx_off
        eor #1
        ldx #0
        rts
