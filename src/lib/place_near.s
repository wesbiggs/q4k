; The place of the land you are on that is nearest a square: for a treasure
; map, which measures from it.
;
; Set near_x and near_y, call place_near(), and A is the place's number (the
; one place_at() would give it) while near_fx and near_fy are its square.
; Nearest is by steps, |dx| + |dy|, the first of equals. Home has
; NUM_HOME_PLACES of them (its own copy of the list, because building a
; continent overwrites isle_fx) and a continent has two. In assembly because
; the C of it came to 300 bytes of a HIGH with none to spare.

        .export _place_near, _near_x, _near_y, _near_fx, _near_fy
        .import _land, _home_fx, _home_fy, _isle_fx, _isle_fy
        .importzp tmp1, tmp2, tmp3, tmp4, ptr1, ptr2

ISLE_HOME       = 4
NUM_HOME_PLACES = 5

        .segment "BSS"
_near_x:  .res 1
_near_y:  .res 1
_near_fx: .res 1
_near_fy: .res 1

        .segment "HICODE"

; |A - m|
.macro absdiff m
        .local ok
        sec
        sbc     m
        bcs     ok
        eor     #$FF
        adc     #1
ok:
.endmacro

_place_near:
        lda     #<_isle_fx
        sta     ptr1
        lda     #>_isle_fx
        sta     ptr1+1
        lda     #<_isle_fy
        sta     ptr2
        lda     #>_isle_fy
        sta     ptr2+1
        ldx     #2                      ; how many places
        lda     _land
        cmp     #ISLE_HOME
        bne     :+
        lda     #<_home_fx
        sta     ptr1
        lda     #>_home_fx
        sta     ptr1+1
        lda     #<_home_fy
        sta     ptr2
        lda     #>_home_fy
        sta     ptr2+1
        ldx     #NUM_HOME_PLACES
:       stx     tmp4
        lda     #$FF
        sta     tmp1                    ; the best distance so far
        ldy     #0
:       lda     (ptr1),y
        absdiff _near_x
        sta     tmp3
        lda     (ptr2),y
        absdiff _near_y
        clc
        adc     tmp3
        cmp     tmp1
        bcs     :+
        sta     tmp1
        sty     tmp2                    ; and which place gave it
        lda     (ptr1),y
        sta     _near_fx
        lda     (ptr2),y
        sta     _near_fy
:       iny
        cpy     tmp4
        bne     :--
        lda     tmp2
        ldx     #0
        rts
