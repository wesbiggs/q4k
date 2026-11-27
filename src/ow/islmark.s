; SPDX-License-Identifier: GPL-3.0-or-later
;
; Quest for the King -- the engine's marks on the islands' places
; (q4k/isles.h's ISL_STATE, under the KERNAL), and where a sea cave's
; chests keep their looted[] bits.
;
; In OW-DATA (COMMON), which both engines load and must be the same in
; both: so this uses only fixed addresses, its own variables and engine.c's
; looted[] (which both have at the same place, in BSS: the Makefile's cmp
; of the two engines' OW-DATA fails if they ever do not), and banks the RAM
; in itself rather than through vault_enter (whose address is not the same
; in both). A write under the KERNAL reaches the RAM; only a read
; needs the KERNAL out.
;
; unsigned char __fastcall__ isl_mark(unsigned char c)
;     Cell c's byte. If its level (the low nibble) is still 0, the party's
;     level goes in first: the place is made for the party as it was on its
;     first visit. The cell is kept for isl_gone().
; void isl_gone(void)
;     The last cell isl_mark() named fallen in (ISL_GONE), and a sea cave's
;     chests' looted[] bits cleared for the next cave; isl_fell set, for
;     the world to say so when it is back.
;
; A sea cave's chests share looted[] bits that no dungeon uses (the Hollow
; Crown's and the Drowned Stair's fourth levels, which they do not have,
; and the two past the towns'): one cave at a time is ever open, since it
; falls in as the party leaves it, and the bits are cleared as it does.
; Each level's first bit is stlow.s's isl_base; level 3's first is the
; chest at the end (ISL_END_BIT, q4k/isles.h).

        .export _isl_mark, _isl_gone, _isl_fell
        .import _looted         ; (engine.c's: the game block's is a copy)

ISL_STATE   = $FBE0
ISL_LEVEL   = $0F
ISL_GONE    = $80
OWG         = $CE37             ; the game block (owgame.h)
PARTY_LEVEL = OWG + 87          ; (engine.c checks the 87)

        .segment "OWDATA"

isl_c:  .byte   0               ; the cell isl_mark() last named
_isl_fell: .byte 0              ; (world.c's to say and clear)

_isl_mark:
        sta     isl_c
        jsr     peek
        and     #ISL_LEVEL
        beq     @new
        tya
        ldx     #0
        rts
@new:   tya
        ora     PARTY_LEVEL
        bne     store           ; (always: the level is never 0)

_isl_gone:
        lda     _looted + 1
        and     #$F1            ; 9-11
        sta     _looted + 1
        lda     _looted + 2
        and     #$9F            ; 21-22
        sta     _looted + 2
        lda     _looted + 4
        and     #$3F            ; 38-39
        sta     _looted + 4
        jsr     peek
        ora     #ISL_GONE
        sta     _isl_fell       ; (nonzero: for the world to say)
store:  sta     ISL_STATE,x     ; (through to the RAM)
        ldx     #0
        rts

; A and Y = the byte of cell isl_c, X = isl_c. An NMI while the
; KERNAL is out finds an RTI: the vector in the RAM under it may be
; another program's (vault_enter sets its own each time).
peek:   ldx     isl_c
        lda     #<nmi
        sta     $FFFA
        lda     #>nmi
        sta     $FFFB
        sei
        lda     $01
        pha
        lda     #$34
        sta     $01
        lda     ISL_STATE,x
        tay
        pla
        sta     $01
        cli
        tya
        rts
nmi:    rti
