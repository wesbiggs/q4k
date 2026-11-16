; The RAM under the ROMs, which nothing else uses.
;
; $D000-$DFFF (under the I/O area) holds the home continent, 64 by 64, for
; the whole game; $E000-$E7BF (under the KERNAL) holds a 64 by 31 map: the
; continent you left to visit a town or a dungeon, which both want local[].
; The rest under the KERNAL is the music's: a tune's header at $E7C0 and the
; tune at $E800-$FFF9 (music.s).
;
; Reaching either takes the machine to all-RAM ($01 = $34), which takes the
; KERNAL and the I/O area away: so interrupts are off for as long as it lasts,
; and for that long only the code below, in ordinary RAM, may run. An NMI cannot
; be turned off, and its vector is read from RAM under the KERNAL too, so it
; is pointed at vault_nmi, which banks the KERNAL in for its handler and out
; again; the music, which plays with the KERNAL out, uses it too. Nothing
; here lasts longer than a copy of a screen's worth of map: about thirty
; milliseconds at the worst.
;
; (The generator builds home in place in the same mode. See isle_gen.s.)
;
; Getting there and back (vault_enter, vault_leave, vault_nmi) is in
; vault_bank.s, which is all some programs want of this.

        .include "zeropage.inc"

        .export _vault_row, _vault_tile, _vault_park, _vault_unpark, _lair_clear
        .import vault_enter, vault_leave        ; vault_bank.s
        .import _local, popa
        .import _quest_stage, _home_fx, _home_fy
        .include "q4k/tiles.inc"

Q_VICTORY  = 9          ; game.h
PLACE_LAIR = 3          ; game.h: home's places, in isle.h's order

HOME_AT = $D000
SLOTS   = $E000
SLOT_SZ = 1984          ; 64 * 31: all of local[] but the row buffer

        .segment "HICODE"

; ptr1 = HOME_AT + A * 64, from a row number in A.
rowptr: ldx #0
        stx ptr1
        lsr
        ror ptr1
        lsr
        ror ptr1
        clc
        adc #>HOME_AT
        sta ptr1+1
        rts

; void vault_row(unsigned char y, unsigned char *dst): row y of home, 64
; squares, to dst.
_vault_row:
        sta ptr2
        stx ptr2+1
        jsr popa
        jsr rowptr
        jsr vault_enter
        ldy #63
@c:     lda (ptr1),y
        sta (ptr2),y
        dey
        bpl @c
        jmp vault_leave

; unsigned char vault_tile(unsigned char x, unsigned char y): one square of
; home.
_vault_tile:
        jsr rowptr
        jsr popa
        ora ptr1
        sta ptr1
        jsr vault_enter
        ldy #0
        lda (ptr1),y
        sta tmp1
        jsr vault_leave
        lda tmp1
        ldx #0
        rts

; void lair_clear(void): once Maldrath is banished (quest_stage >= Q_VICTORY),
; his lair on home is a pass. Home is made again from the seed on a restore,
; so home_ensure() calls this every time it builds it. In MAIN, where there
; is room; it calls nothing that is not in ordinary RAM.
        .segment "CODE"

_lair_clear:
        lda _quest_stage
        cmp #Q_VICTORY
        bcc @no
        lda _home_fy+PLACE_LAIR
        jsr rowptr
        lda _home_fx+PLACE_LAIR
        ora ptr1
        sta ptr1
        jsr vault_enter
        ldy #0
        lda #T_PASS
        sta (ptr1),y
        jmp vault_leave
@no:    rts

        .segment "HICODE"

; ptr2 = the slot A's address.
slotptr:
        asl
        asl
        asl
        clc
        adc #>SLOTS
        sta ptr2+1
        lda #0
        sta ptr2
        rts

; Copy SLOT_SZ bytes, ptr1 to ptr2.
copy:   jsr vault_enter
        ldx #7
@pg:    ldy #0
@b:     lda (ptr1),y
        sta (ptr2),y
        iny
        bne @b
        inc ptr1+1
        inc ptr2+1
        dex
        bne @pg
@t:     lda (ptr1),y            ; Y is 0 again: the last 192 bytes
        sta (ptr2),y
        iny
        cpy #<(SLOT_SZ - 7 * 256)
        bne @t
        jmp vault_leave

; void vault_park(unsigned char slot): local[] into a slot.
_vault_park:
        jsr slotptr
        lda #<_local
        sta ptr1
        lda #>_local
        sta ptr1+1
        jmp copy

; void vault_unpark(unsigned char slot): a slot back into local[].
_vault_unpark:
        jsr slotptr
        lda ptr2
        sta ptr1
        lda ptr2+1
        sta ptr1+1
        lda #<_local
        sta ptr2
        lda #>_local
        sta ptr2+1
        jmp copy
