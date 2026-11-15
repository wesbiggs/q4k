; The music, a SID file's play routine called from the frame interrupt, and
; the sound effects on voice 3.
;
; A tune is a standard SID file's player and data (tools/mksid.py), which
; src/lib/tune.c loads from the disk into the RAM under the KERNAL: a
; header at $E7C0, then the tune at $E800-$FFF9. Reaching it takes the KERNAL
; out ($01 = $35, I/O stays in), so every call into it -- init here, play
; from the interrupt -- banks it out and back, with interrupts off (the
; interrupt's own call is inside the interrupt). An NMI can still come while
; it is out, and its vector is read from RAM: vault_nmi (vault.s) passes it
; on to the KERNAL's handler with the KERNAL banked back in, which the modem's
; RS-232 needs.
;
; The KERNAL's interrupt comes 60 times a second on PAL as well as NTSC (its
; CIA timer is $4025 on PAL, $4295 on NTSC). A tune made to be called at the
; frame rate (the PSID speed flag "VBI") is called five times in six on PAL,
; 50 Hz; a "CIA" one, and any on NTSC, every time. cc65's interruptor chain
; calls music_irq, so this is pure assembly that touches only A, X and Y and
; none of the zero page the C code keeps its temporaries in; a tune may use
; $FB-$FE, which nothing else here does (mksid.py checks).
;
; State, so that the disk code can step out of the way: 0 stopped, 1
; playing, 2 paused. music_pause()/music_resume() do nothing in any other
; state, which lets savegame.c call them whether or not the music has
; started. Pausing silences by the volume alone, and keeps the place: on
; resuming, the notes carry on from where they were. It also waits a moment
; for the silence to be flushed out; see _music_pause. The serial bus is
; timing-sensitive and every IRQ here is cycles it does not have.

        .export _music_stop, _music_pause, _music_resume
        .export _music_state, _music_play
        .export _sfx_play, _sfx_step, _sfx_toggle, _sfx_delay
        .import vault_nmi
        .interruptor music_irq

SID_CTRL = $D404                ; voice 1's control register
SID_V2C  = $D40B                ; voice 2's
SID_V2SR = $D40D
SID_V3F  = $D40E                ; voice 3, the effects'
SID_V3PW = $D410
SID_V3C  = $D412
SID_V3AD = $D413
SID_V3SR = $D414
SID_VOL  = $D418
PALFLAG  = $02A6                ; the KERNAL's own: 0 NTSC, 1 PAL

TUNE_HDR = $E7C0                ; the header tools/mksid.py puts before the tune
HDR_INIT = TUNE_HDR
HDR_PLAY = TUNE_HDR + 2
HDR_VOL  = TUNE_HDR + 5
HDR_CIA  = TUNE_HDR + 6         ; a byte a song: 1 if it is called at 60 Hz
NO_KERNAL = $35                 ; RAM under the KERNAL, I/O in
PAUSE_FRAMES = 12               ; jiffies music_pause waits for silence to be flushed

        .segment "BSS"
_music_state: .res 1
m_skip:  .res 1                 ; nonzero: one call in six is skipped (50 Hz)
m_tick:  .res 1                 ; calls until the next skip
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

; void music_play(unsigned char song): start song (from 0) of the tune that
; is loaded, from the top. tune.c calls it only with one loaded.
_music_play:
        tax
        lda #0
        sta _music_state        ; the interrupt leaves it alone from here
        php
        sei
        lda $01
        pha
        lda #NO_KERNAL
        sta $01
        lda #<vault_nmi         ; an NMI while the KERNAL is out finds its way
        sta $FFFA               ; to the KERNAL's handler (vault.s)
        lda #>vault_nmi
        sta $FFFB
        lda HDR_INIT
        sta @init+1
        lda HDR_INIT+1
        sta @init+2
        lda HDR_PLAY
        sta call+1
        lda HDR_PLAY+1
        sta call+2
        lda HDR_VOL
        sta vol+1
        ldy #0                  ; 60 Hz: every call; at the frame rate on PAL,
        lda HDR_CIA,x           ; five in six
        bne @rate
        lda PALFLAG
        beq @rate
        iny
@rate:  sty m_skip
        lda #6
        sta m_tick
        txa
        cld
@init:  jsr $FFFF               ; the tune's init, song in A
        pla
        sta $01
        plp
        lda #1
        sta _music_state
        rts

; void music_resume(void)
_music_resume:
        lda _music_state
        cmp #2
        bne resumed
        lda #1
        sta _music_state
vol:    lda #$0F                ; the tune's volume, from its header
        sta SID_VOL
resumed:
        rts

; void music_pause(void)
_music_pause:
        lda _music_state
        cmp #1
        bne @no
        lda #2
        sta _music_state
        lda #0
        sta SID_VOL
        ldx #PAUSE_FRAMES       ; VICE, with the drive emulated, stops making
@wait:  lda $A2                 ; sound while the drive works and plays its last
@same:  cmp $A2                 ; buffer over and over: a note held for as long
        beq @same               ; as the disk takes. Let silence reach it first.
        dex
        bne @wait
@no:    rts

; void music_stop(void): no more calls, and the tune's voices let go. It has
; only voices 1 and 2 (mksid.py checks); the effects keep voice 3.
_music_stop:
        lda #0
        sta _music_state
        sta SID_CTRL
        sta SID_V2C
        rts

music_irq:
        jsr sfx_tick
        lda _music_state
        cmp #1
        bne irq_out
        lda m_skip
        beq @play
        dec m_tick
        bne @play
        lda #6                  ; the sixth: skipped
        sta m_tick
        bne irq_out
@play:  lda $01
        pha
        lda #NO_KERNAL
        sta $01
        cld
call:   jsr $FFFF               ; the tune's play
        pla
        sta $01
irq_out:
        clc                     ; not ours alone: let the KERNAL have it too
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
        ldy _music_state        ; the volume is the tune's while it plays (or
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
