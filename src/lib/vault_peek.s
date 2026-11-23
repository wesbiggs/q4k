; SPDX-License-Identifier: GPL-3.0-or-later
; unsigned char __fastcall__ vault_peek(unsigned int addr): one byte of the
; RAM under the ROMs (vault.s: home at $D000, the continent kept at $E000),
; read with the machine in all-RAM for that long (vault_bank.s). A module of
; its own, for the open world's treasure maps (src/lib/ow_tmap.c), so that
; nothing else that links the vault carries it.

        .include "zeropage.inc"
        .export _vault_peek
        .import vault_enter, vault_leave

        .segment "HICODE"

_vault_peek:
        sta ptr1
        stx ptr1+1
        jsr vault_enter
        ldy #0
        lda (ptr1),y
        sta tmp1
        jsr vault_leave
        lda tmp1
        ldx #0
        rts
