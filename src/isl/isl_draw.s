; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the open sea's small islands, drawn into the
; engine's window (q4k/isles.h). This runs under the KERNAL, at ISL_CODE:
; a file of its own, ow-isles (cfg/isl.cfg), which the bootstrap loads there
; beside the table it makes (src/lib/ow_isles.c); the engine's world calls it with the machine in all-RAM
; (vault_enter()), interrupts off, for a run of the sea it has just filled
; with the deep sea (load_run()). It knows nothing of either program but
; the addresses isles.h fixes, and keeps zero page as it found it.
;
; A square is an island's if its block (8 by 8 squares) is the island's:
; land where the island's 4 by 4 at (2, 2) of it has a square, shallow
; water round those, the rest of the block left as it is.

        .export isl_draw

        .segment "LOADADDR"
        .word   isl_draw

ISL_TABLE  = $F000
ISL_ENTRY  = 46
P_CELL     = $F450
P_LX       = $F451
P_LY       = $F452
P_N        = $F453
P_DOWN     = $F454
P_DST      = $F455
ISL_STATE  = $FBE0              ; a byte a cell: bit 7 a sea cave fallen in

T_WATER    = $64

ent        = $FB                ; the cell's entry (saved and put back)
dst        = $FD                ; where the square goes

        .segment "ISLCODE"

isl_draw:
        ldx     #3
:       lda     ent,x
        sta     zsave,x
        dex
        bpl     :-
        lda     #<ISL_TABLE     ; the cell's entry: 46 bytes a cell
        sta     ent
        lda     #>ISL_TABLE
        sta     ent+1
        ldx     P_CELL
        beq     @got
:       lda     ent
        clc
        adc     #ISL_ENTRY
        sta     ent
        bcc     @nc
        inc     ent+1
@nc:    dex
        bne     :-
@got:   ldy     #0
        lda     (ent),y
        beq     @done           ; (a cell with none)
        sta     cnt
        lda     P_DST
        sta     dst
        lda     P_DST+1
        sta     dst+1
        lda     P_LX
        sta     lx
        lda     P_LY
        sta     ly
        lda     P_N
        sta     n
        beq     @done
@sq:    jsr     one
        lda     P_DOWN
        beq     @across
        inc     ly
        lda     dst
        clc
        adc     #64
        sta     dst
        bcc     @next
        inc     dst+1
        bne     @next           ; (always)
@across:
        inc     lx
        inc     dst
        bne     @next
        inc     dst+1
@next:  dec     n
        bne     @sq
@done:  ldx     #3
:       lda     zsave,x
        sta     ent,x
        dex
        bpl     :-
        rts

; The square (lx, ly), if an island's block is where it is.
one:    lda     lx
        lsr     a
        lsr     a
        lsr     a
        sta     blk
        lda     ly
        and     #$38
        ora     blk
        sta     blk
        ldx     #0
        ldy     #4
@find:  lda     (ent),y
        cmp     blk
        beq     @found
        tya
        clc
        adc     #7
        tay
        inx
        cpx     cnt
        bne     @find
        rts
@found: stx     isl
        iny
        lda     (ent),y
        sta     mlo
        iny
        lda     (ent),y
        sta     mhi
        iny
        lda     (ent),y
        sta     base
        iny
        lda     (ent),y
        sta     alt
        iny
        lda     (ent),y
        sta     alo
        iny
        lda     (ent),y
        sta     ahi
        lda     lx              ; where in the island's 4 by 4: -2 to 5
        and     #7
        sec
        sbc     #2
        sta     mx
        lda     ly
        and     #7
        sec
        sbc     #2
        sta     my
        tax
        lda     mx
        jsr     at
        bcc     @sea
        lda     isl             ; land: the place, on island 0's top row
        bne     @gr
        ldy     #2
        lda     (ent),y
        cmp     k
        bne     @gr
        ldx     P_CELL          ; unless it is a sea cave fallen in (its
        lda     ISL_STATE,x     ; chest taken: ISL_GONE), drawn as the
        bmi     @gr             ; ground round it
        dey
        lda     (ent),y
        bne     @put            ; (always: T_TOWN or T_DUNGEON)
@gr:    ldx     k               ; or its ground, or the second
        cpx     #8
        bcs     @ahi
        lda     bits,x
        and     alo
        jmp     @which
@ahi:   lda     bits-8,x
        and     ahi
@which: beq     @base
        lda     alt
        bne     @put            ; (always)
@base:  lda     base
@put:   ldy     #0
        sta     (dst),y
        rts
@sea:   lda     #$FF            ; shallow if any of the eight round it is
        sta     dy              ; land
@row:   lda     #$FF
        sta     dx
@col:   lda     my
        clc
        adc     dy
        tax
        lda     mx
        clc
        adc     dx
        jsr     at
        bcs     @shoal
        inc     dx
        lda     dx
        cmp     #2
        bne     @col
        inc     dy
        lda     dy
        cmp     #2
        bne     @row
        rts
@shoal: lda     #T_WATER
        ldy     #0
        sta     (dst),y
        rts

; Carry set if the island has square (A, X) of its 4 by 4 (each -2 to 6);
; its number, y * 4 + x, in k.
at:     cmp     #4
        bcs     @no
        cpx     #4
        bcs     @no
        sta     tx
        txa
        asl     a
        asl     a
        ora     tx
        sta     k
        tax
        cpx     #8
        bcs     @hi
        lda     bits,x
        and     mlo
        jmp     @t
@hi:    lda     bits-8,x
        and     mhi
@t:     beq     @no
        sec
        rts
@no:    clc
        rts

; The scratch: in the table, in the entry of a cell that has no islands
; of its own (cell 8, home's), which the bootstrap fills with zeros.
SCR     = ISL_TABLE + 8 * ISL_ENTRY
lx      = SCR + 0
ly      = SCR + 1
n       = SCR + 2
cnt     = SCR + 3
blk     = SCR + 4
isl     = SCR + 5
mlo     = SCR + 6
mhi     = SCR + 7
base    = SCR + 8
alt     = SCR + 9
alo     = SCR + 10
ahi     = SCR + 11
mx      = SCR + 12
my      = SCR + 13
k       = SCR + 14
tx      = SCR + 15
dx      = SCR + 16
dy      = SCR + 17
zsave   = SCR + 18             ; (4)
bits:   .byte 1, 2, 4, 8, 16, 32, 64, 128
