; Quest for the King -- the multicolour fight's rounded corners: the four
; sprite blocks at $5700-$57FF (SPR_CORNER in include/q4k/fight.h), hi-res,
; for sprites 0 and 1 doubled both ways (src/fight/mux.s puts them up). The
; same quarter circle as the open world's (src/ow/smooth.s, mx_w, which
; draws them at run time): in each row of the top left one, the first W of
; its 24 pixels are set, those whose middles are more than 48 pixels, three
; squares, from the corner's centre.

        .segment "FIGHTCNR"

W0  = 19
W1  = 16
W2  = 13
W3  = 12
W4  = 10
W5  = 9
W6  = 8
W7  = 7
W8  = 6
W9  = 5
W10 = 4
W11 = 4
W12 = 3
W13 = 2
W14 = 2
W15 = 2
W16 = 1
W17 = 1
W18 = 1
W19 = 0
W20 = 0

.macro  ROW     w, right
        .local  bits
.if right
        bits = (1 << w) - 1
.else
        bits = ((1 << w) - 1) << (24 - w)
.endif
        .byte   <(bits >> 16), <(bits >> 8), <bits
.endmacro

        .repeat 21, R           ; top left
        ROW     .ident(.sprintf("W%d", R)), 0
        .endrepeat
        .byte   0
        .repeat 21, R           ; top right
        ROW     .ident(.sprintf("W%d", R)), 1
        .endrepeat
        .byte   0
        .repeat 21, R           ; bottom left
        ROW     .ident(.sprintf("W%d", 20 - R)), 0
        .endrepeat
        .byte   0
        .repeat 21, R           ; bottom right
        ROW     .ident(.sprintf("W%d", 20 - R)), 1
        .endrepeat
        .byte   0
