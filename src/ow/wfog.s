; SPDX-License-Identifier: GPL-3.0-or-later
;
; Open world: what the hero sees of the land -- the row loop of world.c's
; look (wf_row()), in assembly for its speed and its size: in C it was
; 565 bytes, and a row took more raster lines than a frame has to spare.
;
; One row of one quarter of the look: dy = wf_dy, the quarter wf_q (bit 0
; west, bit 1 north), dx from 0 out to `last`. Each square is seen if the
; one before it on the way back to the hero (by the slope: across, down or
; the diagonal) was seen and is open (vis[] 2: seen and open, 1: seen and a
; mountain or forest, 0: not seen); the hero's own is seen through. A
; square seen that is still dark in WORLD is shown by world.c's wf_hit(),
; which takes the square from wf_ux/wf_uy (the world's), wf_at (WORLD's)
; and wf_gx/wf_gy (from the hero), and returns its tile.
;
; It keeps nothing in zero page across the call to wf_hit (C, which uses
; it all): ptr1 and ptr2 are set again after.

        .include "q4k/tiles.inc"

        .export _wf_row, _wf_hitv
        .export _wf_q, _wf_dy, _wf_hx, _wf_hy, _wf_wx, _wf_wy, _vis
        .export _wf_ux, _wf_uy, _wf_at, _wf_gx, _wf_gy
        .importzp ptr1, ptr2

WORLD   = $B000
T_ODARK = $94                   ; assets/overworld.txt's code dark
WF_ROW  = 13                    ; world.c's WF_X + 1
WORLD_W = 384

; Everything here but wf_draw() is in OW-DATA (COMMON: the file both
; engines load, and must be the same in both), off the world's overlay,
; which is full: so it uses only zero page, its own variables, here, and
; WORLD, and reaches world.c's wf_hit() through wf_hitv, which the overlay
; sets as it starts (w_enter()). OW-DATA is loaded afresh after any other
; program, as the engine is.
        .segment "OWDATA"
_wf_hitv: .word 0
_wf_q:  .byte 4                 ; the look's quarter (4: done) and row
_wf_dy: .byte 0
_wf_hx: .byte 0                 ; where it looks from: the engine's square
_wf_hy: .byte 0
_wf_wx: .word 0                 ; and the world's
_wf_wy: .byte 0
_wf_ux: .word 0                 ; a square seen for the first time, for
_wf_uy: .byte 0                 ; wf_hit(): the world's,
_wf_at: .word 0                 ; WORLD's,
_wf_gx: .byte 0                 ; and from where the look started
_wf_gy: .byte 0
_vis:   .res WF_ROW * 9, 0      ; world.c's WF_ROW by WF_Y + 1
last:   .byte 0
dx:     .byte 0
sx:     .byte 0                 ; 1 east, $FF west
sdy:    .byte 0                 ; dy, signed by the quarter
col:    .byte 0                 ; WORLD's column (& 63 when read)
ux:     .word 0                 ; the world's column, 0 to 383
rowp:   .word 0                 ; WORLD's row
curp:   .word 0                 ; vis + dy * WF_ROW - WF_ROW - 1

; void __fastcall__ wf_row(unsigned char last)
_wf_row:
        sta     last
        ldx     #1              ; the way across ...
        lda     _wf_q
        lsr     a
        bcc     :+
        ldx     #$FF
:       stx     sx
        lda     _wf_dy          ; ... and down
        ldx     _wf_q
        cpx     #2
        bcc     :+
        eor     #$FF
        clc
        adc     #1
:       sta     sdy
        clc                     ; WORLD's row: ((hy + sdy) & 63) * 64
        adc     _wf_hy
        and     #63
        tax
        lsr     a
        lsr     a
        clc
        adc     #>WORLD
        sta     rowp+1
        txa
        and     #3
        lsr     a
        ror     a
        ror     a
        sta     rowp
        lda     _wf_wy          ; the world's row
        clc
        adc     sdy
        sta     _wf_uy
        lda     _wf_dy          ; vis + dy * 13 - 14 (so that Y = dx + 14 is
        asl     a               ; this row's dx, dx + 13 the one before it,
        asl     a               ; dx + 1 the row above, dx the diagonal)
        sta     curp
        asl     a
        clc
        adc     curp
        adc     _wf_dy          ; dy * 13
        clc
        adc     #<(_vis - WF_ROW - 1)
        sta     curp
        lda     #>(_vis - WF_ROW - 1)
        adc     #0
        sta     curp+1
        lda     _wf_hx
        sta     col
        lda     _wf_wx
        sta     ux
        lda     _wf_wx+1
        sta     ux+1
        lda     #0
        sta     dx

@loop:  lda     curp
        sta     ptr2
        lda     curp+1
        sta     ptr2+1
        lda     dx
        ora     _wf_dy
        bne     @pred
        lda     #2              ; the hero's own: seen through
        bne     @store
@pred:  lda     _wf_dy          ; which square is before it
        asl     a
        cmp     dx
        bcs     @n1             ; (2dy >= dx: not across)
        lda     dx
        adc     #WF_ROW         ; (carry clear) the one before, this row
        bne     @get
@n1:    lda     dx
        asl     a
        cmp     _wf_dy
        lda     dx
        bcs     @get            ; (2dx >= dy: the diagonal, Y = dx)
        adc     #1              ; (carry clear) the one above
@get:   tay
        lda     (ptr2),y
        cmp     #2
        beq     @seen
        lda     #0
        beq     @store
@seen:  lda     rowp
        sta     ptr1
        lda     rowp+1
        sta     ptr1+1
        lda     col
        and     #63
        tay
        lda     (ptr1),y
        cmp     #T_ODARK
        bne     @what
        tya                     ; never seen before: wf_hit() shows it
        clc
        adc     ptr1
        sta     _wf_at
        lda     ptr1+1
        adc     #0
        sta     _wf_at+1
        lda     ux
        sta     _wf_ux
        lda     ux+1
        sta     _wf_ux+1
        lda     dx
        ldx     sx
        bpl     :+
        eor     #$FF
        clc
        adc     #1
:       sta     _wf_gx
        lda     sdy
        sta     _wf_gy
        jsr     hit             ; (A: its tile)
@what:  ldx     #1
        cmp     #T_MOUNTAIN
        beq     :+
        cmp     #T_FOREST
        beq     :+
        inx
:       txa
@store: pha
        lda     curp
        sta     ptr2
        lda     curp+1
        sta     ptr2+1
        lda     dx
        clc
        adc     #WF_ROW + 1
        tay
        pla
        sta     (ptr2),y

        lda     col             ; on along the row
        clc
        adc     sx
        sta     col
        lda     sx
        bmi     @west
        inc     ux
        bne     :+
        inc     ux+1
:       lda     ux
        cmp     #<WORLD_W
        bne     @next
        lda     ux+1
        cmp     #>WORLD_W
        bne     @next
        lda     #0
        sta     ux
        sta     ux+1
        beq     @next
@west:  lda     ux
        ora     ux+1
        bne     :+
        lda     #<WORLD_W
        sta     ux
        lda     #>WORLD_W
        sta     ux+1
:       lda     ux
        bne     :+
        dec     ux+1
:       dec     ux

@next:  lda     dx
        cmp     last
        inc     dx              ; (keeps the carry)
        bcs     :+
        jmp     @loop
:       rts

; A square seen for the first time. In the look (wf_lok), from wf_line,
; which world.c's wf_fill() (through wf_fillv) fills at the row's first:
; into WORLD, its bit set, and the boxes grown round it (the edges' but for
; the deep sea, whose edges are none and which is to its neighbours what the
; dark was; and what to draw, if it is on the screen). Else (the near look,
; round the hero) world.c's wf_hit(), through wf_hitv. A: its tile.
        .export _wf_line, _wf_lok, _wf_fillv, _wf_box, _wf_pbox
T_DEEP  = $95
_wf_fillv: .word 0
_wf_line: .res WF_ROW, 0
_wf_lok: .byte 0                ; 1 in the look, 2 once wf_line is filled
_wf_box: .byte 1, 0, 0, 0       ; what changed (x0 x1 y0 y1, from where the
_wf_pbox: .byte 1, 0, 0, 0      ; look started: x0 > x1 none), what to draw
tile:   .byte 0

hit:    lda     _wf_lok
        bne     @line
        jmp     (_wf_hitv)
@line:  cmp     #1
        bne     @have
        jsr     @fill
        lda     #2
        sta     _wf_lok
@have:  lda     _wf_q           ; dx from where wf_line starts: west of the
        lsr     a               ; hero, it starts WF_X squares away
        lda     _wf_gx
        bcc     :+
        clc
        adc     #WF_ROW - 1
:       tay
        lda     _wf_line,y
        sta     tile
        ldy     _wf_at
        sty     ptr1
        ldy     _wf_at+1
        sty     ptr1+1
        ldy     #0
        sta     (ptr1),y
        jsr     _wf_byte_
        lda     _wf_bp+1
        beq     @grow           ; (no bit: open sea)
        sta     ptr1+1
        lda     _wf_bp
        sta     ptr1
        ldy     #0
        lda     (ptr1),y
        ora     _wf_mask
        sta     (ptr1),y
        tax
        lda     _wf_far
        sta     ptr1
        lda     _wf_far+1
        sta     ptr1+1
        txa
        sta     (ptr1),y        ; (a write under the ROM reaches the RAM)
@grow:  lda     _wf_gx
        sta     _gx
        lda     _wf_gy
        sta     _gy
        lda     tile
        cmp     #T_DEEP
        beq     :+
        lda     #<_wf_box
        ldx     #>_wf_box
        jsr     _box_grow
:       lda     _gx             ; on the screen (10 across, 7 down)?
        clc
        adc     #10
        cmp     #21
        bcs     @done
        lda     _gy
        clc
        adc     #7
        cmp     #15
        bcs     @done
        lda     #<_wf_pbox
        ldx     #>_wf_pbox
        jsr     _box_grow
@done:  lda     tile
        rts
@fill:  jmp     (_wf_fillv)

; void wf_dark(void) -- the run load_run() has just copied into the window,
; wf_n squares from wf_dp, along the row (wf_down 0) or down the column:
; each whose bit (wf_mask, in the byte at wf_bp of world.c's copy of the
; bits) is clear, never seen, is dark. A row's bits run on through the
; byte and into the next; a column's are a row's bytes (8) apart.
        .export _wf_dark, _wf_dp, _wf_bp, _wf_n, _wf_down, _wf_mask
_wf_dp:   .word 0
_wf_bp:   .word 0
_wf_n:    .byte 0
_wf_down: .byte 0
_wf_mask: .byte 0

_wf_dark:
        lda     _wf_dp
        sta     ptr1
        lda     _wf_dp+1
        sta     ptr1+1
        lda     _wf_bp
        sta     ptr2
        lda     _wf_bp+1
        sta     ptr2+1
        ldx     _wf_n
        ldy     #0
@sq:    lda     (ptr2),y
        and     _wf_mask
        bne     @seen
        lda     #T_ODARK
        sta     (ptr1),y
@seen:  lda     _wf_down
        bne     @col
        inc     ptr1            ; along the row
        bne     :+
        inc     ptr1+1
:       asl     _wf_mask
        bne     @next
        rol     _wf_mask        ; (the bit carried out: 1, the next byte's)
        inc     ptr2
        bne     @next
        inc     ptr2+1
        bne     @next           ; (always)
@col:   lda     ptr1            ; down the column
        clc
        adc     #64
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       lda     ptr2
        clc
        adc     #8
        sta     ptr2
        bcc     @next
        inc     ptr2+1
@next:  dex
        bne     @sq
        rts

; void __fastcall__ box_grow(signed char *b) -- box b (x0, x1, y0, y1,
; signed; x0 > x1 is none) grown round (gx, gy), world.c's.
        .export _box_grow, _gx, _gy
        .importzp tmp1, tmp2
_gx:    .byte 0
_gy:    .byte 0

_box_grow:
        sta     ptr1
        stx     ptr1+1
        ldy     #1              ; none (x0 > x1)?
        lda     (ptr1),y
        eor     #$80
        sta     tmp1
        dey
        lda     (ptr1),y
        eor     #$80
        cmp     tmp1
        beq     @grow
        bcc     @grow
        lda     _gx             ; none: just the square
        sta     (ptr1),y
        iny
        sta     (ptr1),y
        lda     _gy
        iny
        sta     (ptr1),y
        iny
        sta     (ptr1),y
        rts
@grow:  lda     _gx
        ldy     #0
        jsr     @lo
        iny
        jsr     @hi
        lda     _gy
        iny
        jsr     @lo
        iny
@hi:    sta     tmp2            ; b[y] = A if A > b[y]
        eor     #$80
        sta     tmp1
        lda     (ptr1),y
        eor     #$80
        cmp     tmp1
        bcs     @keep
        bcc     @set
@lo:    sta     tmp2            ; b[y] = A if A < b[y]
        eor     #$80
        sta     tmp1
        lda     (ptr1),y
        eor     #$80
        cmp     tmp1
        bcc     @keep
        beq     @keep
@set:   lda     tmp2
        sta     (ptr1),y
@keep:  lda     tmp2
        rts

; void wf_draw(void) -- the characters sm_box_c0-c1 by sm_box_r0-r1 of the
; front screen drawn there and then from the map, with their colour (as
; the dungeon's fog draws while the hero stands still: foes.s's
; paint_row): what the look has just shown. The caller forgets any shift
; made for the next step (pre_dir), which starts again from this screen.
        .export _wf_draw
        .import _sm_box_c0, _sm_box_c1, _sm_box_r0, _sm_box_r1
        .import _sm_front, _sm_ox, _sm_oy, sm_char_at, sm_cx, sm_cy, ow_chcol
        .importzp ptr3

SCRA    = $4000
SCRB    = $4400

        .segment "BSS"
pr:     .res 1
pc:     .res 1

        .segment "OVWCODE"      ; (it uses the core's variables)
_wf_draw:
        lda     _sm_box_r0
        sta     pr
@row:   lda     #0              ; pr * 40
        sta     ptr1+1
        lda     pr
        asl     a
        asl     a
        asl     a
        sta     tmp1
        asl     a
        rol     ptr1+1
        asl     a
        rol     ptr1+1
        clc
        adc     tmp1
        sta     ptr1
        sta     ptr3
        lda     ptr1+1
        adc     #0
        sta     tmp2            ; (pr * 40's high byte)
        adc     #>$D800         ; (carry clear from here on)
        sta     ptr3+1
        lda     tmp2
        adc     #>SCRA
        ldx     _sm_front
        beq     :+
        adc     #>(SCRB - SCRA)
:       sta     ptr1+1
        ldy     _sm_box_c0
@col:   sty     pc
        tya
        clc
        adc     _sm_ox
        sta     sm_cx
        lda     pr
        clc
        adc     _sm_oy
        sta     sm_cy
        jsr     sm_char_at      ; (keeps ptr1 and ptr3)
        ldy     pc
        sta     (ptr1),y
        tax
        lda     ow_chcol,x
        sta     (ptr3),y
        cpy     _sm_box_c1      ; (carry set at the last)
        iny
        bcc     @col
        lda     pr
        inc     pr
        cmp     _sm_box_r1
        bcc     @row
        rts

; --- What has been seen of the land: the bits ---------------------------------
;
; world.c's wf_ram (wf_ramp: in its BSS, which differs between the
; engines): home's bits (512 bytes), then the kept continent's (248), copied
; out from under the KERNAL (OWG_WFOG) by its w_enter() and wf_take(). wf_isle is the continent's in it, and wf_isle_far where its
; bits are under the KERNAL.

        .export _wf_ramp, _wf_isle, _wf_isle_far, _wf_byte_, _wf_far
        .import _world_grid

OWG_WFOG = $F600
CELL_HOME = 5
ISLE_TOP = 16
ISLE_H  = 31

        .segment "OWDATA"
_wf_ramp:  .word 0              ; world.c's wf_ram (its BSS), set as it starts
_wf_isle:  .byte $FF
_wf_isle_far: .word 0
_wf_far:   .word 0
wbase:     .word 0
wfar:      .word 0
ly:        .byte 0
bits:      .byte 1, 2, 4, 8, 16, 32, 64, 128

; unsigned char *wf_byte(void) -- where the byte with world square (wf_ux,
; wf_uy)'s bit is, in wf_ram (and in wf_far under the KERNAL), and its bit
; in wf_mask; or 0 out on the open sea, where everything is seen, and on a
; continent not kept (never near enough to be streamed or looked at).
_wf_byte_:
        lda     _wf_ux+1        ; the cell: ux / 64 across ...
        lsr     a
        lda     _wf_ux
        ror     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     tmp1
        lda     _wf_uy          ; ... and uy / 64 down, 6 a row
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        tax
        asl     a
        sta     tmp2
        asl     a
        adc     tmp2            ; (carry clear) * 6
        adc     tmp1
        tay
        lda     _world_grid,y
        bne     :+
        jmp     @none
:       ldx     _wf_ramp        ; home: its rows are the cell's
        stx     wbase
        ldx     _wf_ramp+1
        stx     wbase+1
        ldx     #<OWG_WFOG
        stx     wfar
        ldx     #>OWG_WFOG
        stx     wfar+1
        ldx     _wf_uy
        cmp     #CELL_HOME
        beq     @row
        sec                     ; a continent: the kept one's, rows 16-46
        sbc     #1
        cmp     _wf_isle
        bne     @none2
        lda     _wf_uy
        and     #63
        sec
        sbc     #ISLE_TOP
        cmp     #ISLE_H
        bcs     @none2
        tax
        lda     _wf_ramp
        sta     wbase
        lda     _wf_ramp+1
        clc
        adc     #>512
        sta     wbase+1
        lda     _wf_isle_far
        sta     wfar
        lda     _wf_isle_far+1
        sta     wfar+1
        bcc     @row            ; (always)
@none2: jmp     @none
@row:   txa                     ; (row & 63) * 8 + (ux & 63) / 8
        and     #63
        sta     tmp2
        lda     #0
        sta     tmp1
        lda     tmp2
        asl     a
        asl     a
        asl     a
        rol     tmp1            ; (rows 32-63: past a page)
        sta     tmp2
        lda     _wf_ux
        and     #63
        tax
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     tmp2
        sta     tmp2            ; tmp1:tmp2, the byte's offset
        bcc     :+
        inc     tmp1
:       txa
        and     #7
        tax
        lda     bits,x
        sta     _wf_mask
        lda     wfar
        clc
        adc     tmp2
        sta     _wf_far
        lda     wfar+1
        adc     tmp1
        sta     _wf_far+1
        lda     wbase
        clc
        adc     tmp2
        sta     _wf_bp
        tay
        lda     wbase+1
        adc     tmp1
        sta     _wf_bp+1
        tax
        tya
        rts
@none:  lda     #0
        sta     _wf_bp
        sta     _wf_bp+1
        tax
        rts
