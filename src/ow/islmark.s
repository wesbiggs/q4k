; SPDX-License-Identifier: GPL-3.0-or-later
;
; Quest for the King -- the engine's mark on an island's place
; (q4k/isles.h's ISL_STATE, under the KERNAL)
;
; unsigned char __fastcall__ isl_mark(unsigned char c)
;
; Cell c's byte of ISL_STATE. If its level (the low nibble) is still 0, the
; party's level goes in first: the place is made for the party as it was on
; its first visit. (A write under the KERNAL reaches the RAM; only the read
; needs the vault.) In OWTOP: HICODE is full.

        .export _isl_mark
        .import vault_enter, vault_leave

ISL_STATE   = $FBE0
ISL_LEVEL   = $0F
PARTY_LEVEL = $CE37 + 87        ; G->party_level (engine.c checks the 87)

        .segment "OWTOP"

_isl_mark:
        tax
        jsr     vault_enter     ; (keeps X)
        lda     ISL_STATE,x
        sta     got
        jsr     vault_leave
        lda     got
        and     #ISL_LEVEL
        bne     @has
        lda     PARTY_LEVEL
        ora     got
        sta     got
        sta     ISL_STATE,x
@has:   lda     got
        ldx     #0
        rts

        .segment "BSS"

got:    .res 1
