; Quest for the King -- the open world's continents, built when they are
; wanted: the thread itself (src/lib/ow_gen.s has the rest, which is the
; core's: starting it, running it in a frame's spare time, and the
; interrupt taking the machine back from it).
;
; This half is the world's overlay's (OVWCODE), with the land generator
; (the Makefile's ow_isle.o, isle_gen.s assembled into the overlay): only
; the overworld builds a continent, and a build is dropped on the way into a
; place (world.c's w_leave()). The thread starts at ovl.s's ov_gen, the
; jump table's way into whichever overlay is in, so the core never calls
; into the world's code by name (tools/owslots.py).

        .export _w_gen
        .import _isle_begin, _isle_work, _local
        .import vault_enter, vault_leave, sm_guard
        .import gen_d, gen_leave, _gen_state
        .importzp ptr1, ptr2

SLOT    = $E000                 ; the one continent kept (vault.s's slot 0)
ROWS    = 31                    ; ISLE_H

        .segment "BSS"
rows:   .res 1

        .segment "OVWCODE"

        .assert <_local = 0, lderror, "genbuild: local[] must start on a page"

; The thread: the continent gen_d into local[], then a row at a time into
; the slot under the KERNAL.
_w_gen: lda     gen_d
        jsr     _isle_begin
:       jsr     _isle_work
        bne     :-
        lda     #3              ; the slot is neither land from here on
        sta     _gen_state
        lda     #<_local
        sta     ptr1
        lda     #>_local
        sta     ptr1+1
        lda     #<SLOT
        sta     ptr2
        lda     #>SLOT
        sta     ptr2+1
        lda     #ROWS
        sta     rows
@row:   jsr     sm_guard        ; not over the text window's split (smooth.s)
        jsr     vault_enter     ; interrupts off for a row: half a millisecond
        ldy     #63
:       lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     :-
        jsr     vault_leave
        lda     ptr1
        clc
        adc     #64
        sta     ptr1
        sta     ptr2            ; (both start on a page)
        bcc     :+
        inc     ptr1+1
        inc     ptr2+1
:       dec     rows
        bne     @row
        sei
        lda     #2
        sta     _gen_state
        jmp     gen_leave
