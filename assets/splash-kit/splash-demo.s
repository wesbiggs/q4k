; splash-demo: a host program that starts with the splash screen, then carries on with
; the KERNAL as usual (here: prints a line and returns to BASIC).
;
;   cl65 -t none -C c64.cfg --asm-include-dir lib --bin-include-dir data \
;        -o splash-demo.prg splash-demo.s
;
; Defines (cl65 --asm-define NAME=VALUE): SPLASH_SECS, SPLASH_PAL, SPLASH_NTSC,
; SPLASH_KEY (see lib/splash.inc).

CHROUT  = $ffd2

        .segment "LOADADDR"
        .word $0801
        .segment "EXEHDR"
        .word @next, 10
        .byte $9e, "2061", 0            ; 10 SYS 2061
@next:  .word 0

        .code
start:  jsr splash_show                 ; picture and text until a key or time is up
        bcs @none                       ; C=1: no splash for this machine
        sta key                         ; A: the key (PETSCII), 0 if time was up
        ldx #0                          ; back to normal: KERNAL, text screen
        jsr print
        lda key
        beq @time
        ldx #msg_key - msg_back
        jsr print
        lda key
        cmp #$85
        bcc @other
        sec                             ; F1-F7 ($85-$88): "F" and 1, 3, 5, 7
        sbc #$85
        asl
        adc #'1'
        pha
        lda #'F'
        jsr CHROUT
        pla
        jsr CHROUT
        jmp @nl
@other: cmp #$20
        bne :+
        ldx #msg_space - msg_back
        jsr print
        jmp @nl
:       ldx #msg_return - msg_back
        jsr print
@nl:    lda #13
        jmp CHROUT                      ; and back to BASIC
@time:  ldx #msg_time - msg_back
        jmp print
@none:  ldx #msg_none - msg_back        ; and on into print

print:  lda msg_back,x                  ; the message at msg_back + X, up to a 0
        beq :+
        jsr CHROUT
        inx
        bne print
:       rts

key:        .byte 0
msg_back:   .byte "BACK IN THE PROGRAM", 13, 0
msg_key:    .byte "KEY: ", 0
msg_space:  .byte "SPACE", 0
msg_return: .byte "RETURN", 0
msg_time:   .byte "TIME WAS UP", 13, 0
msg_none:   .byte "NO SPLASH FOR THIS MACHINE", 13, 0

        .include "splash.inc"
