; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the multicolour fight: the module's front door.
;
; The file's load address, the jump at $5800 that fight_call() goes through
; (include/q4k/fight.h), and what a program's startup would have done for
; it: the BSS cleared, every time, since nothing is kept between fights.
; The block the caller fills in follows the jump (block.c).

        .import         _fight_run
        .import         __BSS_RUN__, __BSS_SIZE__
        .import         _party, _fq_end
        .importzp       ptr1

        .segment "LOADADDR"
        .addr   * + 2

        .segment "FIGHTHDR"
        jmp     start

; The block is where fight.h says it is, and as long.
        .assert _party = $5803, lderror, "the fight's block has moved"
        .assert _fq_end - _party = 155, lderror, "the fight's block: see FIGHT_BLK_SIZE"

        .segment "CODE"

start:  lda     #<__BSS_RUN__
        sta     ptr1
        lda     #>__BSS_RUN__
        sta     ptr1+1
        lda     #0
        tay
        ldx     #>__BSS_SIZE__
        beq     @rest
@page:  sta     (ptr1),y
        iny
        bne     @page
        inc     ptr1+1
        dex
        bne     @page
@rest:  cpy     #<__BSS_SIZE__
        beq     @done
        sta     (ptr1),y
        iny
        bne     @rest
@done:  jmp     _fight_run
