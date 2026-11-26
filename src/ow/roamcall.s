; SPDX-License-Identifier: GPL-3.0-or-later
;
; Open world: the world's overlay's way to the roaming monsters (roam.s),
; which are under the KERNAL, in a file of their own over the dungeon's fog.
;
; unsigned char __fastcall__ roam(unsigned char op) -- roam.s's R_* op, run
; with the machine in all-RAM (and the split seen to first). If its files
; are not there (the first time out; and the .rl after a dungeon, whose way
; in clears the fog it is over), R_ENTER loads them (engine.c's fetch(), the
; name the .tl's was: ow-cpu.rl and .rh, ow-reu.rl and .rh) and any other
; op does nothing. Then what it
; says: the people placed again (sm_npcs), a fight (ow_fight_with(), which
; never returns), or its answer. While it says there are no monsters, R_IDLE
; and R_SHOW do nothing and R_CAN says yes, without going under the KERNAL.

        .export _roam
        .import rm_sig, rm_sig2, rm_run
        .import _sm_guard, vault_enter, vault_leave, _sm_npcs
        .import _ow_fight_with, _fetch, _hi_file, pusha

R_ENTER = 0                     ; (roam.s)
R_SHOW  = 4
R_IDLE  = 5
R_CAN   = 8

        .segment "OVWCODE"

_roam:  sta     rop
        ldx     none
        beq     @go0
        cmp     #R_IDLE
        beq     @z
        cmp     #R_SHOW
        beq     @z
        cmp     #R_CAN
        bcc     @go0
        lda     #1
        ldx     #0
        rts
@z:     lda     #0
        tax
        rts
@go0:   lda     #0
        sta     tried
@go:    jsr     _sm_guard
        jsr     vault_enter
        lda     rm_sig
        cmp     #$A5
        bne     @gone
        lda     rm_sig+1
        cmp     #$5A
        bne     @gone
        lda     rm_sig+2
        cmp     #$C3
        bne     @gone
        lda     rm_sig2
        cmp     #$C3
        bne     @gone2
        lda     rm_sig2+1
        cmp     #$5A
        bne     @gone2
        lda     rm_sig2+2
        cmp     #$A5
        bne     @gone2
        lda     rop
        jsr     rm_run
        jsr     vault_leave
        stx     rfl
        sty     rarg
        txa
        and     #$40
        sta     none
        lda     rfl
        bpl     :+
        jsr     _sm_npcs
:       lda     rfl
        and     #$3F
        beq     @ans
        sec                     ; ow_fight_with(kind, arg)
        sbc     #1
        jsr     pusha
        lda     rarg
        jmp     _ow_fight_with
@ans:   lda     rarg
        ldx     #0
        rts
@gone:  ldy     #$4C            ; r, l (PETSCII, as cc65 has C's strings)
        bne     @load           ; (always)
@gone2: ldy     #$48            ; r, h
@load:  jsr     vault_leave
        lda     rop
        bne     @zero
        lda     tried
        cmp     #2              ; (each file once, at most)
        bcs     @zero
        inc     tried
        sty     _hi_file+8
        lda     #$52
        sta     _hi_file+7
        lda     #<_hi_file
        ldx     #>_hi_file
        jsr     _fetch
        jmp     @go
@zero:  lda     #0
        tax
        rts

rop:    .byte   0
rfl:    .byte   0
rarg:   .byte   0
tried:  .byte   0
none:   .byte   0
