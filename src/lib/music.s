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
        .import sfx_tick
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

