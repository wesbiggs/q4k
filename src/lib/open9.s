; Is a square, and all eight round it, open ground?
;
; open9(w) takes a pointer into the fight's `wall` (battle.c), whose rows
; are RIM_W = LOCAL_W + 2 = 23 bytes wide, and returns 1 if the nine bytes
; centred on it are all 0 (not walls). The caller keeps it off the rim. In
; assembly because cc65 turned the nine tests in C into 300 bytes of HIGH.

        .export _open9
        .importzp ptr1

RIM_W = 23

        .segment "HICODE"

_open9:
        sta     ptr1
        stx     ptr1+1
        sec                             ; back to the top-left of the nine
        lda     ptr1
        sbc     #RIM_W + 1
        sta     ptr1
        bcs     :+
        dec     ptr1+1
:       ldx     #3
row:    ldy     #0
        lda     (ptr1),y
        iny
        ora     (ptr1),y
        iny
        ora     (ptr1),y
        bne     no
        clc
        lda     ptr1
        adc     #RIM_W
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       dex
        bne     row
        lda     #1
        ldx     #0
        rts
no:     lda     #0
        tax
        rts
