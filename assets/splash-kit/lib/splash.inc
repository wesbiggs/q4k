; splash.inc: show the splash picture and its text, then give the machine back.
;
;       jsr splash_show                 ; returns when a key is pressed or time is up
;
; C=0: shown; A = the key that ended it as PETSCII (space $20, RETURN $0d, F1 $85,
; F3 $86, F5 $87, F7 $88), or 0 when the time was up. C=1: not shown (A = V2I_ERR_SYSTEM or V2I_ERR_FORMAT: this machine's video
; system has no image here, or its VIC-II isn't supported). Either way the machine is as
; it was before (KERNAL, IRQ, VIC registers, $01), except for the memory listed in the
; README: $02, $1d-$29 (v2t zero page) and RAM at $c000-$ffff (the picture is unpacked
; there; its IRQ/NMI vectors at $fffa-$ffff).
;
; ca65 source. Include it anywhere in RAM below $c000, after the code that calls it:
; it assembles code and data into the current segment, and it includes v2i.inc, v2t.inc
; (which needs font.inc) and the data files (.incbin, on the --bin-include-dir path).
; Define before the include to change:

.ifndef SPLASH_SECS
SPLASH_SECS = 5                         ; seconds on screen (1-1000); 0: until space
.endif
.ifndef SPLASH_PAL
SPLASH_PAL = 1                          ; 0: leave the PAL image out (20K)
.endif
.ifndef SPLASH_NTSC
SPLASH_NTSC = 1                         ; 0: leave the NTSC image out (20K)
.endif
.ifndef SPLASH_KEY
SPLASH_KEY = 1                          ; 0: keys don't end the splash early
.endif

.if SPLASH_SECS = 0 && SPLASH_KEY = 0
  .error "SPLASH_SECS = 0 needs SPLASH_KEY = 1, or the splash never ends"
.endif

splash_show:
        jsr v2i_init                    ; KERNAL out, own IRQ; sets v2i_system
.if SPLASH_PAL
        lda v2i_system
        cmp #V2I_PAL
        bne :+
        lda #<splash_img_p
        ldy #>splash_img_p
        ldx #50
        bne @show
:
.endif
.if SPLASH_NTSC
        lda v2i_system
        cmp #V2I_NTSC
        bne :+
        lda #<splash_img_n
        ldy #>splash_img_n
        ldx #60
        bne @show
:
.endif
        lda #V2I_ERR_SYSTEM             ; no image for this machine
        bne @fail
@show:  stx splash_fps
        jsr v2i_show
        bcs @fail
.if SPLASH_PAL && SPLASH_NTSC
        lda #<splash_txt_p              ; the text that goes with the image
        ldy #>splash_txt_p
        ldx v2i_system
        beq :+
        lda #<splash_txt_n
        ldy #>splash_txt_n
:
.elseif SPLASH_PAL
        lda #<splash_txt_p
        ldy #>splash_txt_p
.else
        lda #<splash_txt_n
        ldy #>splash_txt_n
.endif
        jsr v2t_draw
        jsr v2i_start                   ; on screen from the next frame
        lda #0                          ; frames left: SPLASH_SECS * fps (16 bit)
        sta splash_left
        sta splash_left+1
        ldx #<SPLASH_SECS
        ldy #>SPLASH_SECS
@mul:   txa                             ; add fps SPLASH_SECS times
        bne :+
        tya
        beq @wait
        dey
:       dex
        clc
        lda splash_left
        adc splash_fps
        sta splash_left
        bcc @mul
        inc splash_left+1
        bne @mul                        ; always
@wait:  lda #0
        sta splash_key                  ; 0: the time was up
.if SPLASH_KEY
:       jsr splash_scan                 ; a key held from before doesn't count
        bne :-
.endif
@frame: lda v2i_ticks
:       cmp v2i_ticks                   ; next frame
        beq :-
.if SPLASH_KEY
        jsr splash_scan
        bne @key
.endif
.if SPLASH_SECS
        lda splash_left
        bne :+
        dec splash_left+1
:       dec splash_left
        lda splash_left
        ora splash_left+1
        beq @done
.endif
        jmp @frame
.if SPLASH_KEY
@key:   sta splash_key
:       jsr splash_scan                 ; wait for the release, so the program
        bne :-                          ; after the splash doesn't see the key
.endif
@done:  jsr v2i_exit
        lda splash_key
        clc
        rts
@fail:  pha
        jsr v2i_exit
        pla
        sec
        rts

.if SPLASH_KEY
; A = PETSCII of a pressed key (space, RETURN, F1, F3, F5, F7; the first found), else 0
; (Z set). Reads the CIA1 matrix: the KERNAL's keyboard scan is off.
splash_scan:
        lda #$fe                        ; column 0: RETURN bit 1, F7 3, F1 4, F3 5, F5 6
        sta $dc00
        ldx #5
@bit:   lda $dc01                       ; 0: pressed
        and splash_mask-1,x
        beq @hit
        dex
        bne @bit
        lda #$7f                        ; column 7: space bit 4
        sta $dc00
        lda $dc01
        and #$10
        bne @none
        lda #$20
        rts
@hit:   lda splash_bits-1,x
        rts
@none:  lda #0
        rts
splash_mask: .byte $02, $08, $10, $20, $40
splash_bits: .byte $0d, $88, $85, $86, $87
.endif

splash_fps:  .byte 0
splash_key:  .byte 0
splash_left: .word 0

        .include "v2i.inc"
        .include "v2t.inc"
.if SPLASH_PAL
splash_img_p: .incbin "splash-p.v2i", 2
splash_txt_p: .incbin "splash-p.v2t", 2
.endif
.if SPLASH_NTSC
splash_img_n: .incbin "splash-n.v2i", 2
splash_txt_n: .incbin "splash-n.v2t", 2
.endif
