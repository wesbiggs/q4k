; q4k -- smooth scrolling over a multicolour tile map, double buffered.
;
; The open world (src/ow/engine.c) walks Keldmarch and sails the sea with the
; camera moving a pixel or two a frame instead of a square at a time. This
; is its engine, assembled twice: OW_REU = 1 for OW-REU, which scrolls
; through the REU, and 0 for OW-CPU, which does it all with the CPU.
;
; VIC bank 1 ($4000-$7FFF), which has no character ROM image in it:
;
;   $4000  screen A          $4800  the character set (multicolour)
;   $4400  screen B          $5000  sprite blocks 64-73 (the hero as each
;                                   class, the villager, the ship)
;
; The map is a square's tile code per byte, 64 by 64, at WORLD (the
; generator's home, copied out of the vault). Every square is a metatile of
; 2x2 characters (src/lib/ow_data.s, from assets/overworld.txt), so the
; screen's 40x25 characters are 20x12.5 squares. World coordinates in
; characters are biased by 64 -- the map's 128 characters sit at 64-191 --
; so that the sea around it is still a byte, and anything outside is sea.
;
; Edges, where one terrain meets another. SHORE, beside WORLD, has a byte
; for each square: a bit for each of its eight neighbours that gives it an
; edge (sm_shores works it out when the land is built: bit 0 north, then
; clockwise). Water's edges are beaches, where land is; grass's only a
; headland, a quarter with sea beside and above (or below) it rounding its
; corner off with sand; and sand, forest, mountain and swamp fray where they
; meet open ground (assets/overworld.txt says which). Each quarter of a
; water square looks only at the three neighbours on its own corner -- the
; one beside it, the one above or below, and the diagonal -- and, through
; ow_qvar, picks an edge: a strip, an inside corner where both sides are
; edges, or a small outside corner where only the diagonal is; ow_blend
; has the character for each metatile, quarter and edge. So an edge of any
; shape is a few characters, and costs a lookup more.
;
; How a scroll works. The VIC's fine scroll ($D016/$D011 bits 0-2) moves the
; picture up to seven pixels; past that, every character has to move one
; place. That is a thousand bytes of screen and a thousand of colour RAM,
; more than a frame can rewrite without the beam catching it half done. So:
;
;  - The screen is double buffered. While the front one is shown, the back
;    one is built as the front moved a character in the direction of travel
;    (sm_build: a straight copy, then the one new row or column from the map).
;    The C side does that in the frames between, well ahead of need.
;  - On the frame the fine scroll wraps, the raster interrupt (line 250, in
;    the bottom border) sets the new fine scroll, points $D018 at the other
;    screen, and rewrites colour RAM top down from the new screen. Colour RAM
;    cannot be double buffered -- there is one, at $D800 -- but it does not
;    have to be: each character has its own colour (ow_chcol), so colour RAM
;    is a function of the screen, and the copy, started in the border, stays
;    ahead of the beam all the way down (about 570 cycles a row against the
;    beam's 504; it starts some twelve rows ahead).
;  - Nothing is ever drawn into the screen being shown.
;
; Movement is a square at a time, as in the game, so the direction of each
; character shift is known when the step begins. At rest the camera sits four
; pixels into a character (fine scroll 3, the VIC's default), so that a step
; either way reaches its first wrap two frames in at two pixels a frame: time
; enough to build the back screen first.
;
; The interrupt chains to the KERNAL's ($EA31) for the keyboard and the clock,
; with the CIA's timer interrupt turned off: one interrupt a frame, at a
; known line. It uses no zero page.

        .export _sm_init, _sm_draw_all, _sm_shores, _sm_build, _sm_shift, _sm_edge, _sm_edge_row, _sm_edge_col, _sm_commit
        .export _sm_sprite, _sm_animate, _sm_show
        .export _sm_redraw_box, _sm_shores_rect, _sm_torus
        .export _sm_vcopy, _vc_src, _vc_dst, _vc_n, _vc_sstep, _vc_dstep
        .import vault_enter, vault_leave
        .import ow_tune_irq, gen_on, gen_preempt
        .export _sm_sh_x0, _sm_sh_y0, _sm_sh_x1, _sm_sh_y1
        .export _sm_box_c0, _sm_box_c1, _sm_box_r0, _sm_box_r1
        .export _sm_ox, _sm_oy, _sm_fx, _sm_fy, _sm_front, _sm_frame
        .export _sm_colend, _sm_irqline, _sm_irqend, _sm_splitline
        .export _sm_say, _sm_ink, _sm_text_clear, _sm_guard, sm_guard
        .export _sm_quit
.if OW_REU
        .export _sm_reu_keep
.endif

        .import ow_qvar, ow_rnd, ow_sprites, ow_nsprites
        .import _ow_spr1, _ow_spr2
        .export _sm_tileset, _sm_blank, _sm_offtile
        .export _sm_npcs, _sm_npc_n, _sm_npc_x, _sm_npc_y, _sm_tunic
        .export _sm_npc_ox, _sm_npc_oy
        .importzp ptr1, ptr2, ptr3, ptr4, sp
        .export _sm_stack

SCRA     = $4000
SCRB     = $4400
CHARS    = $4800
SPRITES  = $5000
SPRBLOCK = (SPRITES & $3FFF) / 64
WORLD    = $B000                ; 64x64, page aligned
SHORE    = $A000                ; the same, for water: land round it (below)
D018_A   = ((SCRA & $3FFF) / $400) * 16 + ((CHARS & $3FFF) / $800) * 2
D018_B   = ((SCRB & $3FFF) / $400) * 16 + ((CHARS & $3FFF) / $800) * 2
IRQLINE  = 250
; The text window (sm_say): the map is rows 0-21 of the screens, and rows
; 22 and 23 are two lines of text that stay put while the map scrolls. See
; "The text window", below the interrupt.
MAPROWS  = 22
MAPLEN   = MAPROWS * 40
TEXTROW  = 22
SPLITLINE = $DC                 ; ECM on, fine scroll 7: the map ends
TEXTLINE  = $E5                 ; the line before the line before the text
FONT     = $A0                  ; the text's characters: screen codes 0-$5F

; The REU (a 1700/1750, or one emulated: VICE's, a TeensyROM's), at $DF00.
REU_STATUS = $DF00
REU_CMD    = $DF01              ; $90 + type: execute now, no $FF00 trigger
REU_C64    = $DF02              ; and $DF03
REU_ADDR   = $DF04              ; and $DF05, $DF06 the bank
REU_LEN    = $DF07              ; and $DF08
REU_IMASK  = $DF09
REU_ACTL   = $DF0A
STASH      = $90                ; C64 to REU
FETCH      = $91                ; REU to C64
REU_SCREEN = $0000              ; where a front screen is stashed to shift
REU_COLOUR = $0400              ; and its colour RAM
TBASE    = $60
        .assert (TBASE & 63) = 32, error, "loadrow's quick way needs it"
BIAS     = 64                   ; world characters: the map is 64-191

; Each character's colour RAM value, which the flip's colour copy looks up a
; thousand times: on a page of its own, or most of those cross one, which
; is a cycle each and more than NTSC has to spare. (Not cleared: the tile
; set writes it.)
        .segment "ALIGNBSS"
        .align  256
ow_chcol:   .res 256

        .segment "BSS"

; The tile set in use: what sm_tileset() unpacks from a set's blob (see
; tools/mkoverworld.py). Everything that draws reads these.
ts_bg:      .res 1              ; the three shared colours
ts_mc1:     .res 1
ts_mc2:     .res 1
ts_nchars:  .res 1              ; 0 is 256
ow_nwater:  .res 1              ; water character pairs to animate
_sm_offtile: .res 1             ; what is beyond the map
ts_nmt:     .res 1              ; metatiles
ow_water:   .res 16             ; those pairs
ow_mt0:     .res 64             ; each tile code's metatile ($60 + index) ...
ow_mt1:     .res 64             ; ... and its variant's
        .export ow_nflag, ow_bsel, ow_binv   ; (for tests)
ow_nflag:   .res 64             ; what it is to its neighbours
ow_bsel:    .res 64             ; which of their flags give it an edge ...
ow_binv:    .res 64             ; ... set or clear
ow_chq:     .res 128            ; each metatile's characters, 32 a quarter
ow_blend:   .res 512            ; and its edges', 16 a metatile
den:        .res 1              ; $10, or 0 while the screen is blanked

; People: sprites 1-7 over the map, at squares (biased, like the hero's),
; placed from the camera the next frame shows (sm_npcs) and written by the
; interrupt with the fine scroll, so they move with the map.
_sm_tunic:  .res 1              ; the hero's colour: the leader's (G->tunic)
_sm_npc_n:  .res 1
_sm_npc_x:  .res 7
_sm_npc_y:  .res 7
_sm_npc_ox: .res 7              ; and pixels from there, signed (a dungeon's
_sm_npc_oy: .res 7              ; monsters gliding from square to square)
spr_xy:     .res 14             ; sprites 1-7: x, y, x, y, ...
spr_msb:    .res 1              ; $D010
spr_en:     .res 1              ; $D015
spr_ready:  .res 1              ; the interrupt copies them in, once
camx:       .res 2              ; the camera, in pixels
camy:       .res 2
cnlo:       .res 1              ; copyn's count
cnhi:       .res 1

_sm_ox:     .res 1              ; the front screen's top left, in characters
_sm_oy:     .res 1
_sm_fx:     .res 1              ; how far into it the camera is, 0-7
_sm_fy:     .res 1
_sm_front:  .res 1              ; 0: screen A is shown, 1: B
_sm_frame:  .res 1              ; counts the interrupts
_sm_colend: .res 1              ; the raster line the last colour copy ended on
_sm_irqline: .res 1             ; the line the interrupt began on
_sm_irqend: .res 1              ; and ended on
_sm_splitline: .res 1           ; the latest line a split's write came in
irqstep:    .res 1              ; 0 the flip's next, 1 the split's, 2 the text's
_sm_ink:    .res 1              ; sm_say's colour

built_dir:  .res 1              ; the way the back screen was built
.if OW_REU
built_reu:  .res 1              ; the back screen was built through it
r_c64:      .res 2              ; reu_go's arguments
r_addr:     .res 2
r_len:      .res 2
pendreu:    .res 1              ; the flip's colour comes from the REU
.endif
pend16:     .res 1              ; what the interrupt writes next
pend11:     .res 1
pend18:     .res 1
pendswap:   .res 1

cx:         .res 1              ; char_at's arguments and workings
cy:         .res 1
tx:         .res 1
ty:         .res 1
qofs:       .res 1
vtmp:       .res 1
bmask:      .res 1
mtv:        .res 1
sy:         .res 1              ; sm_shores' workings
dmask:      .res 1
inv:        .res 1
sel:        .res 1
_sm_sh_x0:  .res 1              ; the squares sm_shores_rect does
_sm_sh_y0:  .res 1
_sm_sh_x1:  .res 1
_sm_sh_y1:  .res 1
offflag:    .res 1              ; the flags of what is beyond the map
rowlen:     .res 1              ; fill_n's count
xend:       .res 1              ; sm_shores_rect's last flag index
_sm_torus:  .res 1              ; WORLD is the overworld's wrapping window
_vc_src:    .res 2              ; sm_vcopy's arguments
_vc_dst:    .res 2
_vc_n:      .res 1
_vc_sstep:  .res 1
_vc_dstep:  .res 1
_sm_box_c0: .res 1              ; sm_redraw_box's characters, on the screen
_sm_box_c1: .res 1
_sm_box_r0: .res 1
_sm_box_r1: .res 1
above:      .res 66             ; sm_shores' rows of flags
cur:        .res 66
below:      .res 66
cnt:        .res 1
dir:        .res 1
sc_front:   .res 1              ; what shiftcopy is patched for, with ...
backhi:     .res 1

        .segment "CODE"

; ---------------------------------------------------------------------------
; The character at world position (cx, cy), in A. Clobbers X, Y, ptr2.

char_at:
        lda     cy
        lsr     a
        sta     ty
        lda     #0
        rol     a               ; the row within the square
        sta     qofs
        lda     cx
        lsr     a
        sta     tx
        lda     qofs
        rol     a               ; quadrant: row * 2 + column
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     qofs            ; * 32, the quadrant's table in ow_chq

        lda     #0
        sta     bmask
        lda     _sm_torus       ; the overworld's window wraps: no edge
        beq     @bound
        lda     ty
        and     #63
        tay
        lda     tx
        and     #63
        jmp     @inmap
@bound: lda     ty
        sec
        sbc     #BIAS / 2
        cmp     #64
        bcs     @sea
        tay
        lda     tx
        sec
        sbc     #BIAS / 2
        cmp     #64
        bcs     @sea
@inmap: ora     rowlo,y
        sta     ptr2
        lda     rowhi,y
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        tax
        lda     ptr2+1          ; and what is round it, from SHORE
        sec
        sbc     #>(WORLD - SHORE)
        sta     ptr2+1
        lda     (ptr2),y
        sta     bmask
        txa
        jmp     @code           ; (not a BIT skip: the load below is 3 bytes)
@sea:   lda     _sm_offtile
@code:  sec
        sbc     #TBASE
        and     #63
        tax

        ldy     ty              ; the variant, from a hash of the square
        lda     ow_rnd,y
        clc
        adc     tx
        tay
        lda     ow_rnd,y
        lsr     a
        bcs     @v1
        lda     ow_mt0,x
        bcc     @mt             ; always
@v1:    lda     ow_mt1,x
@mt:    sta     mtv

        ldy     bmask           ; an edge on this quarter's corner?
        beq     @plain
        lda     qofs
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     #>ow_qvar
        sta     ptr2+1
        lda     #<ow_qvar
        sta     ptr2
        lda     (ptr2),y        ; which edge, 1-4, from ow_qvar[q][mask]
        beq     @plain
        sec
        sbc     #1
        sta     vtmp
        lda     qofs
        lsr     a
        lsr     a
        lsr     a               ; quarter * 4
        ora     vtmp
        tay
        lda     mtv             ; ow_blend + metatile * 16
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     vtmp            ; (its high byte's part)
        lda     mtv
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     #<ow_blend
        sta     ptr2
        lda     vtmp
        adc     #>ow_blend
        sta     ptr2+1
        lda     (ptr2),y
        rts

@plain: lda     mtv
        ora     qofs
        tax
        lda     ow_chq,x
        rts

; ---------------------------------------------------------------------------
; void sm_shores(void) -- SHORE from WORLD (see the top): for each square
; that takes edges (ow_bsel), a bit for each neighbour that makes one,
; north first and then clockwise. Three rows of neighbour flags (ow_nflag)
; roll down the map, so every neighbour is a load, a mask and a rotate. In a
; place, beyond the map is the set's offmap tile (sea, or grass round the
; castle); on the overworld (sm_torus) WORLD is a window that wraps both
; ways, row 63 beside row 0 and column 63 beside column 0, so the flags
; wrap too. Only the columns asked for, and one either side, are read.

; Row A of the map as flags into `below`, columns sm_sh_x0 - 1 to sm_sh_x1
; + 1 (at index column + 1). A is $FF for the row above row 0 and 64 for
; the one below row 63: on the torus those are rows 63 and 0, and in a
; place they are off the map.
loadrow:
        ldx     _sm_torus
        beq     :+
        and     #63
        bpl     @have           ; (always)
:       cmp     #64
        bcc     @have
        lda     offflag         ; a row off the map
        ldx     _sm_sh_x0
:       sta     below,x
        cpx     xend
        beq     @done
        inx
        bne     :-
@done:  rts
@have:  tay
        lda     rowlo,y
        sta     ptr3
        lda     rowhi,y
        sta     ptr3+1
        ldy     _sm_sh_x0       ; no column off the map or wrapped: the
        beq     @slow           ; quick way, with the column in Y and
        lda     xend            ; the index (the column + 1) from it
        cmp     #65
        bcs     @slow
        dey
@fast:  lda     (ptr3),y
        eor     #TBASE & $3F    ; = (code - TBASE) & 63, since TBASE is
        and     #63             ; 32 more than a multiple of 64
        tax
        lda     ow_nflag,x
        sta     below+1,y
        iny
        cpy     xend
        bne     @fast
        rts
@slow:  ldx     _sm_sh_x0       ; the index: the column + 1
@col:   txa
        sec
        sbc     #1              ; the column: -1 ($FF) to 64
        ldy     _sm_torus
        beq     :+
        and     #63             ; wrapped
        bpl     @in             ; (always)
:       cmp     #64
        bcc     @in
        lda     offflag         ; off the map's side
        jmp     @st
@in:    tay
        lda     (ptr3),y
        sec
        sbc     #TBASE
        and     #63
        tay
        lda     ow_nflag,y
@st:    sta     below,x
        cpx     xend
        beq     @done
        inx
        jmp     @col

; above = cur, cur = below, for the columns in hand.
roll:   ldx     _sm_sh_x0
:       lda     cur,x
        sta     above,x
        lda     below,x
        sta     cur,x
        cpx     xend
        beq     :+
        inx
        bne     :-
:       rts

_sm_shores:
        lda     #0              ; all of it
        sta     _sm_sh_x0
        sta     _sm_sh_y0
        lda     #63
        sta     _sm_sh_x1
        sta     _sm_sh_y1
; void sm_shores_rect(void) -- the same for the squares sm_sh_x0-x1 by
; sm_sh_y0-y1 only (x0 <= x1, y0 <= y1): where a roof came off or went on,
; or a column or row the overworld's window has just taken in, and a square
; round it. The rest is left as it was.
_sm_shores_rect:
        lda     _sm_offtile     ; beyond the map: what the set says is there
        sec
        sbc     #TBASE
        and     #63
        tax
        lda     ow_nflag,x
        sta     offflag
        lda     _sm_sh_x1
        clc
        adc     #2
        sta     xend
        lda     _sm_sh_y0       ; the row above the first ...
        sec
        sbc     #1
        jsr     loadrow
        jsr     roll            ; ... into `cur`, to move up into `above`
        lda     _sm_sh_y0       ; and the first, to move up into `cur`
        sta     sy
        jsr     loadrow

@row:   jsr     roll
        lda     sy              ; the row below, into `below`
        clc
        adc     #1
        jsr     loadrow
        ldy     sy              ; this row's: WORLD and SHORE + y * 64
        lda     rowlo,y
        sta     ptr1
        sta     ptr2
        lda     rowhi,y
        sta     ptr1+1
        sec
        sbc     #>(WORLD - SHORE)
        sta     ptr2+1

        ldy     _sm_sh_x1
@col:   lda     (ptr1),y        ; what makes an edge on this square
        eor     #TBASE & $3F    ; (code - TBASE) & 63
        and     #63
        tax
        lda     ow_bsel,x
        bne     :+
        jmp     @store          ; nothing does (A is 0)
:       sta     sel             ; which of a neighbour's flags ...
        lda     ow_binv,x
        sta     inv             ; ... and whether set or clear
        tya
        tax
        inx                     ; the flags' index for this square
        lda     inv             ; most squares have no edge: all eight
        bne     @all            ; neighbours at once first
        lda     above-1,x       ; (an edge where a flag is set: none is?)
        ora     above,x
        ora     above+1,x
        ora     cur-1,x
        ora     cur+1,x
        ora     below-1,x
        ora     below,x
        ora     below+1,x
        and     sel
        bne     @each
        jmp     @store          ; (A is 0)
@all:   lda     above-1,x       ; (an edge where one is clear: all set?)
        and     above,x
        and     above+1,x
        and     cur-1,x
        and     cur+1,x
        and     below-1,x
        and     below,x
        and     below+1,x
        and     sel
        beq     @each
        lda     #0
        jmp     @store
@each:  lda     above,x         ; north
        and     sel
        cmp     #1
        ror     dmask
        lda     above+1,x       ; north east
        and     sel
        cmp     #1
        ror     dmask
        lda     cur+1,x         ; east
        and     sel
        cmp     #1
        ror     dmask
        lda     below+1,x       ; south east
        and     sel
        cmp     #1
        ror     dmask
        lda     below,x         ; south
        and     sel
        cmp     #1
        ror     dmask
        lda     below-1,x       ; south west
        and     sel
        cmp     #1
        ror     dmask
        lda     cur-1,x         ; west
        and     sel
        cmp     #1
        ror     dmask
        lda     above-1,x       ; north west
        and     sel
        cmp     #1
        ror     dmask
        lda     dmask
        eor     inv
@store: sta     (ptr2),y
        cpy     _sm_sh_x0
        beq     :+
        dey
        jmp     @col
:       lda     sy              ; the last row?
        cmp     _sm_sh_y1
        bne     :+
        rts
:       inc     sy
        jmp     @row

; void sm_stack(void) -- the C stack to the tape buffer, down from $0400:
; MAIN, at whose top cc65 starts it, is full. Called first thing.
_sm_stack:
        lda     #<$0400
        sta     sp
        lda     #>$0400
        sta     sp+1
        rts

; A row of 40 from (cx, cy) into ptr1; or of rowlen (fill_n).
fill_row:
        lda     #40
        sta     rowlen
fill_n: lda     #0
        sta     cnt
@l:     jsr     char_at
        ldy     cnt
        sta     (ptr1),y
        inc     cx
        iny
        sty     cnt
        cpy     rowlen
        bne     @l
        rts

; A column of the map's rows from (cx, cy) down into ptr1.
fill_col:
        lda     #MAPROWS
        sta     cnt
@l:     jsr     char_at
        ldy     #0
        sta     (ptr1),y
        inc     cy
        lda     ptr1
        clc
        adc     #40
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       dec     cnt
        bne     @l
        rts

; ---------------------------------------------------------------------------
; Colour RAM from a screen: four bytes a pass, ten passes a row, row by row
; from the top so the beam never overtakes it. About 570 cycles a row. The
; map's rows only: the text's colours are its own (sm_say).

.macro COLCOPY scr
        .local row
        .repeat MAPROWS, R
        ldx     #9
:
        .repeat 4, K
        ldy     scr + R*40 + K*10, x
        lda     ow_chcol, y
        sta     $D800 + R*40 + K*10, x
        .endrep
        dex
        bpl     :-
        .endrep
        rts
.endmacro

.if !OW_REU
colour_a:
        COLCOPY SCRA
colour_b:
        COLCOPY SCRB
.else
; OW-REU colours a flip from the REU, and needs a copy from the screen only
; for a redraw (sm_draw_all, sm_redraw_box): one copy will do, made to read
; whichever screen that is by colour_for, in the main loop, beforehand.
colour_a:
        COLCOPY SCRA
CC_ROW = 2 + 4 * 9 + 3          ; a row's code: ldx, four ldy-lda-sta, dex, bpl
        .assert * - colour_a = MAPROWS * CC_ROW + 1, error, "colour_for's layout"

; colour_a, made to read the screen whose page is in A: the high byte of
; each of its hundred ldy's, flipped if it reads the other one. About 2K
; cycles, when it does. Clobbers X, Y and ptr1.
colour_for:
        cmp     cc_page
        beq     @done
        sta     cc_page
        lda     #<(colour_a + 4)
        sta     ptr1
        lda     #>(colour_a + 4)
        sta     ptr1+1
        ldx     #MAPROWS
@row:   ldy     #0
@ldy:   lda     (ptr1),y
        eor     #(>SCRA) ^ (>SCRB)
        sta     (ptr1),y
        tya
        clc
        adc     #9
        tay
        cpy     #36
        bne     @ldy
        lda     ptr1
        clc
        adc     #CC_ROW
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       dex
        bne     @row
@done:  rts

        .segment "DATA"
cc_page: .byte  >SCRA           ; the screen colour_a reads
        .segment "CODE"
.endif

; ---------------------------------------------------------------------------
.if !OW_REU
; The back screen as the front moved one character. Column by column, each
; column all the map's rows unrolled: nine cycles a byte. One copy for every
; direction and screen, 2.2K less than sixteen copies: sm_shift patches its
; fifty addresses for a new direction (about 2K cycles), or for the same one
; from the other screen only flips their high bytes (about 900), which is
; every shift of a straight walk. The first pair is the map's last row: a
; direction that moves rows copies it and starts past it (it is the new edge,
; or it would land on the text).
        .segment "HICODE"
shiftcopy:
sc_ldx: ldx     #38
sc_go:  jmp     sc_body
sc_body:
        .repeat MAPROWS, R
        lda     $FFFF, x
        sta     $FFFF, x
        .endrep
        dex
        bmi     :+
sc_loop: jmp    sc_body
:       rts

; The camera's directions are the game's (sprite.h): down, up, left, right;
; then the diagonals, down-left, down-right, up-left, up-right. Down shows
; the next row below: everything moves up a row.
        .segment "CODE"
sh_soff: .byte 40, 0, 0, 1, 40, 41, 0, 1
sh_doff: .byte 0, 40, 1, 0, 1, 0, 41, 40
sh_skip: .byte 6, 6, 0, 0, 6, 6, 6, 6   ; past the last row if it moves rows
sh_cnt:  .byte 39, 39, 38, 38, 38, 38, 38, 38

        .segment "DATA"
sc_dir:  .byte   $FF            ; ... this direction: none yet
        .segment "CODE"

.endif

; Which way each direction moves the camera, a character at a time.
dirdx:   .byte 0, 0, $FF, 1, $FF, 1, $FF, 1
dirdy:   .byte 1, $FF, 0, 0, 1, 1, $FF, $FF

; void __fastcall__ sm_build(unsigned char dir)
; The back screen, as the front would be with the camera a character further
; in `dir`. Never call it while a flip is pending: the back screen is still
; on show until the interrupt has made it. It is the three parts below,
; about 10K, 6K and 4K cycles, which can go in different frames (NTSC has no
; room for all of them in the frame after a flip): sm_shift, then sm_edge_row
; and sm_edge_col, all with the same `dir`.
_sm_build:
        pha
        jsr     _sm_shift
        pla
; void __fastcall__ sm_edge(unsigned char dir) -- both edges.
_sm_edge:
        pha
        jsr     _sm_edge_row
        pla
        jmp     _sm_edge_col

; void __fastcall__ sm_shift(unsigned char dir) -- the copy.
_sm_shift:
        and     #7
        sta     dir
        sta     built_dir
.if OW_REU
        lda     #1
        sta     built_reu
        jmp     reu_shift
.else
        jsr     patch
        jmp     shiftcopy
.endif

.if !OW_REU
; The copy, made ready for direction A from the screen in front.
patch:  cmp     sc_dir
        bne     @new
        lda     _sm_front       ; the same way: from the other screen?
        cmp     sc_front
        beq     @go
        sta     sc_front
        ldy     #0              ; flip every address's high byte
@flip:  lda     sc_body+2,y
        eor     #(>SCRA) ^ (>SCRB)
        sta     sc_body+2,y
        lda     sc_body+5,y
        eor     #(>SCRA) ^ (>SCRB)
        sta     sc_body+5,y
        tya
        clc
        adc     #6
        tay
        cpy     #MAPROWS*6
        bne     @flip
@go:    rts

@new:   sta     sc_dir
        tax
        lda     _sm_front
        sta     sc_front
        lda     sh_cnt,x
        sta     sc_ldx+1
        lda     sh_skip,x
        clc
        adc     #<sc_body
        sta     sc_go+1
        sta     sc_loop+1
        lda     #>sc_body
        adc     #0
        sta     sc_go+2
        sta     sc_loop+2
        lda     sh_soff,x
        sta     ptr1
        lda     sh_doff,x
        sta     ptr2
        lda     #>SCRA
        ldy     #>SCRB
        ldx     _sm_front
        beq     :+
        lda     #>SCRB
        ldy     #>SCRA
:       sta     ptr1+1
        sty     ptr2+1
        ldy     #7              ; rows 0-20 into pairs 1-21 ...
        ldx     #MAPROWS
@pair:  dex
        bne     :+
        ldy     #1              ; ... and row 21 into pair 0
:       lda     ptr1
        sta     sc_body,y
        lda     ptr1+1
        sta     sc_body+1,y
        lda     ptr2
        sta     sc_body+3,y
        lda     ptr2+1
        sta     sc_body+4,y
        txa
        beq     @go
        tya
        clc
        adc     #6
        tay
        lda     ptr1
        clc
        adc     #40
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       lda     ptr2
        clc
        adc     #40
        sta     ptr2
        bcc     @pair
        inc     ptr2+1
        bne     @pair           ; (always)

.endif

; The back screen's page into backhi, and dir from A.
backpage:
        and     #7
        sta     dir
        lda     _sm_front
        beq     :+
        lda     #>SCRA
        .byte   $2C
:       lda     #>SCRB
        sta     backhi
        rts

; void __fastcall__ sm_edge_row(unsigned char dir) -- the new row, at the
; top or the bottom, if the camera moves up or down.
_sm_edge_row:
        jsr     backpage
        ldx     dir
        lda     dirdy,x
        beq     @none
        bmi     @top
        lda     #<((MAPROWS-1)*40)      ; the bottom row, from the new
        sta     ptr1                    ; origin's
        lda     backhi
        clc
        adc     #>((MAPROWS-1)*40)
        sta     ptr1+1
        lda     _sm_oy
        clc
        adc     #MAPROWS        ; (oy + 1) + the last row
        bne     @go             ; (never 0 on the map)
@top:   lda     #0
        sta     ptr1
        lda     backhi
        sta     ptr1+1
        lda     _sm_oy
        sec
        sbc     #1
@go:    sta     cy
        lda     _sm_ox
        clc
        adc     dirdx,x         ; the new origin's x
        sta     cx
        jmp     fill_row
@none:  rts

; void __fastcall__ sm_edge_col(unsigned char dir) -- the new column, at the
; left or the right, if the camera moves across.
_sm_edge_col:
        jsr     backpage
        ldx     dir
        lda     backhi
        sta     ptr1+1
        lda     dirdx,x
        beq     @none
        bmi     @left
        lda     #39
        sta     ptr1
        lda     _sm_ox
        clc
        adc     #40             ; (ox + 1) + 39
        bne     @go
@left:  lda     #0
        sta     ptr1
        lda     _sm_ox
        sec
        sbc     #1
@go:    sta     cx
        lda     _sm_oy
        clc
        adc     dirdy,x         ; the new origin's y
        sta     cy
        jmp     fill_col
@none:  rts

; void sm_draw_all(void) -- the front screen and colour RAM from scratch.
_sm_draw_all:
        lda     _sm_front
        beq     :+
        lda     #>SCRB
        .byte   $2C
:       lda     #>SCRA
        jsr     draw_page
.if OW_REU
        lda     _sm_front
        beq     :+
        lda     #>SCRB
        .byte   $2C
:       lda     #>SCRA
        jsr     colour_for
        jmp     colour_a
.else
        lda     _sm_front
        bne     :+
        jmp     colour_a
:       jmp     colour_b
.endif

; void sm_redraw_box(void) -- the back screen as the front, but for the
; characters sm_box_c0-c1 by sm_box_r0-r1, drawn from the map: for a flip
; that changes a little of what is on the map without moving. The colour of
; that flip comes from the screen, as sm_redraw's.
_sm_redraw_box:
        lda     _sm_front       ; the front copied, sprite pointers and all
        beq     :+
        lda     #>SCRB
        .byte   $2C
:       lda     #>SCRA
        sta     ptr3+1
        eor     #(>SCRA) ^ (>SCRB)
        sta     ptr2+1
        ldy     #0
        sty     ptr3
        sty     ptr2
        ldx     #4
:       lda     (ptr3),y
        sta     (ptr2),y
        iny
        bne     :-
        inc     ptr3+1
        inc     ptr2+1
        dex
        bne     :-
        lda     _sm_box_c1      ; how many a row
        sec
        sbc     _sm_box_c0
        clc
        adc     #1
        sta     rowlen
        lda     _sm_box_r0
        sta     dir             ; the row, here
@row:   ldy     dir             ; the back screen's row, from c0
        lda     rowlo40,y
        clc
        adc     _sm_box_c0
        sta     ptr1
        lda     _sm_front
        beq     :+
        lda     #>SCRA
        .byte   $2C
:       lda     #>SCRB
        adc     rowhi40,y
        sta     ptr1+1
        lda     _sm_ox
        clc
        adc     _sm_box_c0
        sta     cx
        lda     _sm_oy
        clc
        adc     dir
        sta     cy
        jsr     fill_n
        lda     dir
        cmp     _sm_box_r1
        beq     :+
        inc     dir
        bne     @row
:
.if OW_REU
        lda     #0
        sta     built_reu
.endif
        rts

; The map's rows from the map into the screen whose page is in A.
draw_page:
        sta     ptr1+1
        lda     #0
        sta     ptr1
        lda     _sm_oy
        sta     cy
        lda     #MAPROWS
        sta     dir             ; a row count, here
@row:   lda     _sm_ox
        sta     cx
        jsr     fill_row
        lda     ptr1
        clc
        adc     #40
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       inc     cy
        dec     dir
        bne     @row
        rts

; ---------------------------------------------------------------------------
; void __fastcall__ sm_commit(unsigned char flip)
; What the next interrupt shows: the fine scroll from sm_fx and sm_fy and, if
; flip, the other screen. All of it at once, so that a frame never gets the
; new fine scroll with the old screen.
_sm_commit:
.if OW_REU
        pha                     ; a flip coloured from the screen: the copy
        tax                     ; made to read the one that will be in front
        beq     @nocol
        lda     built_reu
        bne     @nocol
        lda     _sm_front
        beq     @tob
        lda     #>SCRA
        bne     @pat
@tob:   lda     #>SCRB
@pat:   jsr     colour_for
@nocol: pla
.endif
        sei
        tay
        lda     #7
        sec
        sbc     _sm_fx
        ora     #$10            ; multicolour, 38 columns
        sta     pend16
        lda     #7
        sec
        sbc     _sm_fy
        ora     den             ; display on (unless blanked), 24 rows
        sta     pend11
        tya
        beq     @done
        lda     _sm_front
        eor     #1
        sta     _sm_front
        beq     :+
        lda     #D018_B
        .byte   $2C
:       lda     #D018_A
        sta     pend18
.if OW_REU
        lda     built_reu
        sta     pendreu
        lda     #0
        sta     built_reu       ; until the next build says otherwise
.endif
        lda     #1
        sta     pendswap
@done:  cli
        rts

; void __fastcall__ sm_sprite(unsigned char frame) -- the hero's frame, in
; both screens' sprite pointers, so a flip does not change it.
_sm_sprite:
        clc
        adc     #SPRBLOCK
        sta     SCRA + $3F8
        sta     SCRB + $3F8
        rts

; void sm_animate(void) -- water flows a pixel east: each row of each water
; metatile, sixteen pixels across two characters, rotated one double-width
; pixel.
_sm_animate:
        ldx     #0
@pair:  cpx     ow_nwater
        bcs     @done
        txa
        pha
        asl     a
        tax
        lda     ow_water,x
        jsr     @addr
        sta     ptr1
        sty     ptr1+1
        lda     ow_water+1,x
        jsr     @addr
        sta     ptr2
        sty     ptr2+1
        ldy     #7
@row:   ldx     #2
@bit:   lda     (ptr2),y
        lsr     a
        lda     (ptr1),y
        ror     a
        sta     (ptr1),y
        lda     (ptr2),y
        ror     a
        sta     (ptr2),y
        dex
        bne     @bit
        dey
        bpl     @row
        pla
        tax
        inx
        bne     @pair
@done:  rts

@addr:  ldy     #0              ; CHARS + A * 8, in A (low) and Y (high)
        sty     cnt
        asl     a
        rol     cnt
        asl     a
        rol     cnt
        asl     a
        rol     cnt
        pha
        lda     cnt
        clc
        adc     #>CHARS
        tay
        pla
        rts

.if OW_REU
; ---------------------------------------------------------------------------
; The REU. Its transfers run at a byte a cycle with the CPU stopped, about
; five times the fastest copy the CPU can do, and it reaches colour RAM like
; any other address, so:
;
;  - the shift is two transfers: the front screen into the REU, and back
;    out into the back screen `off` bytes along (+-1 a column, +-40 a row,
;    both for a diagonal). What wraps round a row's end lands in the edge
;    column, which sm_edge_col then draws over. About 2K cycles, not 10K.
;  - colour RAM goes into the REU with it (it changes only at a flip, so
;    it is the front's), and at the flip the interrupt brings it back the
;    same `off` along -- a thousand cycles, not fifteen thousand -- and then
;    colours only the new edge from the screen.
;
; Only OW-REU has this: the bootstrap (src/openworld.c) loads it when it
; finds an REU whose transfers land in colour RAM (ow_reu_detect).

; Where each direction takes a byte from: the shift as a signed offset.
offlo:  .byte <40, <-40, <-1, <1, <39, <41, <-41, <-39
offhi:  .byte >40, >-40, >-1, >1, >39, >41, >-41, >-39

; One transfer: r_c64, r_addr (bank 0), r_len, the command in A.
reu_go: ldx     r_c64
        stx     REU_C64
        ldx     r_c64+1
        stx     REU_C64+1
        ldx     r_addr
        stx     REU_ADDR
        ldx     r_addr+1
        stx     REU_ADDR+1
        ldx     #0
        stx     REU_ADDR+2
        stx     REU_ACTL
        ldx     r_len
        stx     REU_LEN
        ldx     r_len+1
        stx     REU_LEN+1
        sta     REU_CMD
        rts

; r_addr = base + max(off, 0), r_c64 = c64 + max(-off, 0), r_len = MAPLEN -
; |off|, for the direction in X: what a shifted transfer needs. The bases
; come in r_addr and r_c64.
shifted:
        lda     offhi,x
        bmi     @neg
        lda     r_addr
        clc
        adc     offlo,x
        sta     r_addr
        lda     r_addr+1
        adc     #0
        sta     r_addr+1
        lda     #<MAPLEN
        sec
        sbc     offlo,x
        sta     r_len
        lda     #>MAPLEN
        sbc     #0
        sta     r_len+1
        rts
@neg:   lda     r_c64           ; c64 + -off
        sec
        sbc     offlo,x
        sta     r_c64
        lda     r_c64+1
        sbc     #$FF
        sta     r_c64+1
        lda     #<MAPLEN        ; MAPLEN + off
        clc
        adc     offlo,x
        sta     r_len
        lda     #>MAPLEN
        adc     #$FF
        sta     r_len+1
        rts

; The page of the front screen into A.
frontpage:
        lda     _sm_front
        beq     :+
        lda     #>SCRB
        .byte   $2C
:       lda     #>SCRA
        rts

; Each transfer's setting up and running is one stretch with interrupts off
; (the interrupt uses the REU and these variables too), and no longer: a
; thousand cycles at most, so a flip is never held up past the border.
reu_shift:
        jsr     sm_guard
        sei
        lda     #0              ; the front screen out ...
        sta     r_c64
        sta     r_addr
        sta     r_addr+1
        jsr     frontpage
        sta     r_c64+1
        jsr     len1000
        lda     #STASH
        jsr     reu_go
        cli

        jsr     sm_guard
        sei
        lda     #<$D800         ; ... and its colour, for the flip ...
        sta     r_c64
        lda     #>$D800
        sta     r_c64+1
        lda     #<REU_COLOUR
        sta     r_addr
        lda     #>REU_COLOUR
        sta     r_addr+1
        jsr     len1000
        lda     #STASH
        jsr     reu_go
        cli

        jsr     sm_guard
        sei
        lda     #0              ; ... and into the back screen, shifted
        sta     r_c64
        sta     r_addr
        sta     r_addr+1
        jsr     frontpage
        eor     #(>SCRA) ^ (>SCRB)
        sta     r_c64+1
        ldx     dir
        jsr     shifted
        lda     #FETCH
        jsr     reu_go
        cli
        rts

len1000:                        ; (the map's rows: not the text's)
        lda     #<MAPLEN
        sta     r_len
        lda     #>MAPLEN
        sta     r_len+1
        rts

; In the interrupt, at a flip built through the REU: colour RAM from the
; REU, shifted, then the new edge's colours from the new front screen. The
; edge loops modify their own operands: the interrupt has no zero page.
reu_colour:
        lda     #<$D800
        sta     r_c64
        lda     #>$D800
        sta     r_c64+1
        lda     #<REU_COLOUR
        sta     r_addr
        lda     #>REU_COLOUR
        sta     r_addr+1
        ldx     built_dir
        jsr     shifted
        lda     #FETCH
        jsr     reu_go

        ldx     built_dir
        lda     dirdy,x
        beq     @col
        ldy     #0              ; the top row ...
        lda     dirdy,x
        bmi     :+
        ldy     #MAPROWS-1      ; ... or the bottom
:       lda     rowlo40,y
        sta     @rs+1
        sta     @rc+1
        jsr     frontpage
        clc
        adc     rowhi40,y
        sta     @rs+2
        lda     #>$D800
        clc
        adc     rowhi40,y
        sta     @rc+2
        ldx     #39
@rs:    ldy     $FFFF,x
        lda     ow_chcol,y
@rc:    sta     $FFFF,x
        dex
        bpl     @rs

@col:   ldx     built_dir
        lda     dirdx,x
        beq     @done
        ldy     #0              ; the left column ...
        lda     dirdx,x
        bmi     :+
        ldy     #39             ; ... or the right
:       sty     @cs+1
        sty     @cc+1
        jsr     frontpage
        sta     @cs+2
        lda     #>$D800
        sta     @cc+2
        ldx     #MAPROWS
@cs:    ldy     $FFFF
        lda     ow_chcol,y
@cc:    sta     $FFFF
        lda     @cs+1
        clc
        adc     #40
        sta     @cs+1
        sta     @cc+1
        bcc     :+
        inc     @cs+2
        inc     @cc+2
:       dex
        bne     @cs
@done:  rts

.endif

; void sm_vcopy(void) -- vc_n bytes from vc_src, in the RAM under the ROMs
; (the vault, where the lands are kept), to vc_dst, the source stepping
; vc_sstep a byte and the destination vc_dstep (1 along a row, 64 down a
; column). Interrupts are off for as long as it takes (vault.s). A source
; below $100 is none: its low byte fills the run instead (open sea).
_sm_vcopy:
        lda     _vc_dst
        sta     ptr2
        lda     _vc_dst+1
        sta     ptr2+1
        ldy     #0
        ldx     _vc_n
        lda     _vc_src+1       ; no source: fill with its low byte
        bne     @vault
@fill:  lda     _vc_src
        sta     (ptr2),y
        lda     ptr2
        clc
        adc     _vc_dstep
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       dex
        bne     @fill
        rts
@vault: sta     ptr1+1
        lda     _vc_src
        sta     ptr1
@chunk: jsr     sm_guard        ; sixteen at a time, clear of the split
        jsr     vault_enter
        lda     #16
        sta     cnt
@l:     lda     (ptr1),y
        sta     (ptr2),y
        lda     ptr1
        clc
        adc     _vc_sstep
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       lda     ptr2
        clc
        adc     _vc_dstep
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       dex
        beq     @end
        dec     cnt
        bne     @l
        jsr     vault_leave
        jmp     @chunk
@end:   jmp     vault_leave

.if OW_REU
; void __fastcall__ sm_reu_keep(unsigned char out) -- the overworld's map
; and edges (SHORE and WORLD, 8K) into the REU (out = 1) or back from it
; (0), so that coming out of a town or a dungeon builds nothing again.
REU_KEEP = $2000                ; past what the scroll uses
_sm_reu_keep:
        tax
        sei
        lda     #<SHORE
        sta     r_c64
        lda     #>SHORE
        sta     r_c64+1
        lda     #<REU_KEEP
        sta     r_addr
        lda     #>REU_KEEP
        sta     r_addr+1
        lda     #<$2000
        sta     r_len
        lda     #>$2000
        sta     r_len+1
        lda     #FETCH
        cpx     #0
        beq     :+
        lda     #STASH
:       jsr     reu_go
        cli
        rts

.endif

; ---------------------------------------------------------------------------
; void sm_npcs(void) -- the people's sprites from the camera just committed
; (sm_ox/sm_fx, sm_oy/sm_fy), for the next interrupt to write. Off the
; screen, a sprite is switched off. On the screen: x = square * 16 - camera
; + 31 and y = square * 16 - camera + 54, as the hero's (engine.c).
_sm_npcs:
        lda     _sm_ox          ; camx = ox * 8 + fx
        ldx     #0
        stx     camx+1
        asl     a
        rol     camx+1
        asl     a
        rol     camx+1
        asl     a
        rol     camx+1
        ora     _sm_fx
        sta     camx
        lda     _sm_oy
        stx     camy+1
        asl     a
        rol     camy+1
        asl     a
        rol     camy+1
        asl     a
        rol     camy+1
        ora     _sm_fy
        sta     camy
        lda     #0
        sta     spr_msb
        lda     #1              ; the hero is always on
        sta     spr_en
        ldy     #0
@one:   cpy     _sm_npc_n
        bcc     :+
        jmp     @done
:
        lda     #0              ; x = square * 16 + 31 - camx, 16 bits
        sta     cnhi
        lda     _sm_npc_x,y
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        clc
        adc     #31
        bcc     :+
        inc     cnhi
:       clc                     ; + the glide, signed
        adc     _sm_npc_ox,y
        bcc     :+
        inc     cnhi
:       ldx     _sm_npc_ox,y
        bpl     :+
        dec     cnhi
:       sec
        sbc     camx
        sta     cnlo
        lda     cnhi
        sbc     camx+1
        and     #7              ; on a plane 2048 pixels round (the
        cmp     #2              ; overworld's 128 squares, wrapping)
        bcs     @off            ; off either side
        sta     cnhi
        lda     #0              ; y = square * 16 + 54 - camy, 8 bits or off
        sta     vtmp
        lda     _sm_npc_y,y
        asl     a
        rol     vtmp
        asl     a
        rol     vtmp
        asl     a
        rol     vtmp
        asl     a
        rol     vtmp
        clc
        adc     #54
        bcc     :+
        inc     vtmp
:       clc
        adc     _sm_npc_oy,y
        bcc     :+
        inc     vtmp
:       ldx     _sm_npc_oy,y
        bpl     :+
        dec     vtmp
:       sec
        sbc     camy
        tax
        lda     vtmp
        sbc     camy+1
        and     #7
        bne     @off            ; above the screen, or below it
        cpx     #NPC_YMAX + 1
        bcs     @off            ; over the text window
        tya                     ; on: x, y and the bits
        asl     a
        sta     dmask           ; (two bytes a sprite)
        txa
        ldx     dmask
        sta     spr_xy+1,x
        lda     cnlo
        sta     spr_xy,x
        lda     bit_of+1,y
        ora     spr_en
        sta     spr_en
        lda     cnhi
        beq     @off2
        lda     bit_of+1,y
        ora     spr_msb
        sta     spr_msb
        bne     @off2
@off:
@off2:  iny
        jmp     @one
@done:  lda     #1
        sta     spr_ready
        rts

bit_of: .byte   1, 2, 4, 8, 16, 32, 64, 128

; The lowest a person's sprite may stand: its sixteen lines end above the
; band (from $DD) and the text.
NPC_YMAX = SPLITLINE - 15

; ---------------------------------------------------------------------------
; void __fastcall__ sm_tileset(const unsigned char *set) -- a tile set in:
; its blob (src/lib/ow_data.s; the order is mkoverworld.py's) unpacked into
; the tables above and its characters into the character set, and its
; colours. Whatever is on screen is drawn in the new set until it is
; redrawn, so blank the screen first (sm_blank) if one is showing.
_sm_tileset:
        sta     ptr1
        stx     ptr1+1
        ldy     #6              ; the header: colours, counts, off the map
:       lda     (ptr1),y
        sta     ts_bg,y
        dey
        bpl     :-
        lda     #7
        jsr     skip
        lda     ts_bg
        sta     $D021
        lda     ts_mc1
        sta     $D022
        lda     ts_mc2
        sta     $D023

        lda     ow_nwater       ; the water pairs
        asl     a
        ldx     #0
        ldy     #<ow_water
        jsr     copyto
        lda     ts_nchars       ; each character's colour
        ldx     #0
        cmp     #0
        bne     :+
        inx                     ; 0 is 256
:       ldy     #<ow_chcol
        sty     ptr2
        ldy     #>ow_chcol
        sty     ptr2+1
        jsr     copyn
        lda     #<(64 * 5)      ; the five tables by tile code, end to end
        ldx     #>(64 * 5)
        ldy     #<ow_mt0
        sty     ptr2
        ldy     #>ow_mt0
        sty     ptr2+1
        jsr     copyn
        ldy     #0              ; each quarter's characters, 32 apart
@quart: tya
        pha
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     #<ow_chq
        sta     ptr2
        lda     #>ow_chq
        adc     #0
        sta     ptr2+1
        lda     ts_nmt
        ldx     #0
        jsr     copyn
        pla
        tay
        iny
        cpy     #4
        bne     @quart
        lda     ts_nmt          ; the edges, 16 a metatile
        ldx     #0
        stx     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        ldx     cnhi
        ldy     #<ow_blend
        sty     ptr2
        ldy     #>ow_blend
        sty     ptr2+1
        jsr     copyn
        lda     ts_nchars       ; and the characters, 8 bytes each
        ldx     #0
        stx     cnhi
        cmp     #0
        bne     :+
        inc     cnhi            ; 256 of them
:       asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        asl     a
        rol     cnhi
        ldx     cnhi
        ldy     #<CHARS
        sty     ptr2
        ldy     #>CHARS
        sty     ptr2+1
        jmp     copyn

; A bytes on from ptr1.
skip:   clc
        adc     ptr1
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       rts

; A bytes (under 256) from ptr1 to the page of ow_water at Y: for the
; little tables. (ow_water is the only one: it does not cross a page.)
copyto: sty     ptr2
        ldy     #>ow_water
        sty     ptr2+1
        ldx     #0
; X:A bytes from ptr1, which ends past them, to ptr2.
copyn:  sta     cnlo
        stx     cnhi
        ldy     #0
@l:     lda     cnlo
        ora     cnhi
        beq     @done
        lda     (ptr1),y
        sta     (ptr2),y
        inc     ptr1
        bne     :+
        inc     ptr1+1
:       inc     ptr2
        bne     :+
        inc     ptr2+1
:       lda     cnlo
        bne     :+
        dec     cnhi
:       dec     cnlo
        jmp     @l
@done:  rts

; void __fastcall__ sm_blank(unsigned char on) -- the screen off (1) or on
; (0) from the next frame: for while a tile set and a map change.
_sm_blank:
        tax
        lda     #$10
        cpx     #0
        beq     :+
        lda     #0
:       sta     den
        sei
        lda     pend11
        and     #$EF
        ora     den
        sta     pend11
        cli
        rts

; ---------------------------------------------------------------------------
; The raster interrupt, from the KERNAL's vector at $0314.

irq:
        lda     irqstep         ; the text window's two (below)?
        beq     :+
        jmp     irq_text
:       lda     $D012
        sta     _sm_irqline
        lda     #$01
        sta     $D019
        lda     pend16
        sta     $D016
        lda     pend11
        sta     $D011
        lda     ts_bg           ; (the text window made it black)
        sta     $D021
        lda     den             ; and its split, unless the screen is off
        beq     :+
        lda     #SPLITLINE
        sta     $D012
        inc     irqstep
:       lda     pendswap
        beq     @tail
        lda     #0
        sta     pendswap
        lda     pend18
        sta     $D018
.if OW_REU
        lda     pendreu
        beq     :+
        jsr     reu_colour
        jmp     :++
:       jsr     colour_a        ; (colour_for chose the screen)
.else
        lda     _sm_front
        bne     :+
        jsr     colour_a
        jmp     :++
:       jsr     colour_b
.endif
:       lda     $D012
        sta     _sm_colend
@tail:  lda     spr_ready       ; the people, with the scroll
        beq     @nospr
        ldx     #13
:       lda     spr_xy,x
        sta     $D002,x
        dex
        bpl     :-
        lda     spr_msb
        sta     $D010
        lda     spr_en
        sta     $D015
        lda     #0
        sta     spr_ready
@nospr: inc     _sm_frame
        jsr     ow_tune_irq     ; the music (ow_tune.s)
        lda     $D012           ; (how late a split could be, for --peek)
        sta     _sm_irqend
        lda     gen_on          ; a continent being built is put down until
        beq     :+              ; the main loop has had its frame (ow_gen.s)
        jmp     gen_preempt
:       jmp     $EA31

; ---------------------------------------------------------------------------
; The text window: two lines that stay put under a map that scrolls.
;
; The map is rows 0-21 of the screens and the text rows 22 and 23. Across,
; that is only the fine scroll, which can change anywhere on the screen. Down,
; the VIC starts a character row on each line whose low three bits are the
; fine scroll (a "bad line"), so with the map's fine scroll (0-7) row 22 would
; start anywhere in eight lines, and the text would bob as the map moves. So
; round line $DD, once row 20 has begun whatever the fine scroll, this sets
; it to 7: row 22 starts at $E7 every frame, and 22 and 23 are $E7-$F6, just
; above the bottom border. (Row 21 starts where it would have, or, if the
; write came before its bad line, at $DF; or starts again at $DF; in every
; case the VIC has counted it once by $E7.) What it shows between the map
; and the text depends on all that, so the same write turns on extended
; colour, which with multicolour is a mode with no colours at all: black,
; whatever the characters, from here to line $E6, where the second interrupt
; turns it off again just before the text. Black on black: a band below the
; map, always the same.
;
; The first write must come after row 20's last possible bad line ($D7) and
; before cycle 58 of line $DF: later than that, with the map's fine scroll
; at 0, the VIC would count row 21 twice and show row 23 in row 22's place.
; The interrupt at $DC writes in $DD (--peek sm_splitline is the latest
; line seen just after the write), so that holds unless something keeps
; interrupts off for longer than a couple of lines round there: see
; sm_guard. Later is not much worse: row 22 then starts at $E0 and is drawn
; again from $E7, so the text is right and a few lines of the band show it
; early, for a frame. Measured over openworld-sail's 59 steps, with a
; continent built on the way: $E1 on PAL, $E2 on NTSC and with the REU. The second write has to land at the end of line $E6, in the
; right border, after the last of its pixels and before $E7's bad line.
;
; The flip's interrupt at 250 puts the map's own scroll back for the next
; frame. Each of these is a few dozen cycles and leaves by $EA81: only the
; flip's goes on to the KERNAL's keyboard scan.
irq_text:
        lda     #$01
        sta     $D019
        lda     irqstep
        cmp     #2
        beq     @text
        lda     den             ; extended colour, multicolour: black; and
        ora     #$40 | $07      ; the fine scroll 7, 24 rows
        sta     $D011
        lda     #0
        sta     $D021
        lda     #$18            ; text: no fine scroll, 40 columns
        sta     $D016
        lda     $D012           ; (the latest yet, for --peek)
        cmp     _sm_splitline
        bcc     :+
        sta     _sm_splitline
:       lda     #TEXTLINE
        sta     $D012
        inc     irqstep
        jmp     $EA81
@text:  lda     den             ; the text's, ready
        ora     #$07
        tax
        lda     #TEXTLINE
@line:  cmp     $D012           ; to the start of the line after
        beq     @line
        ldy     #10             ; and into its right border: about 55 cycles
@wait:  dey
        bne     @wait
        nop
        stx     $D011
        lda     #IRQLINE
        sta     $D012
        lda     #0
        sta     irqstep
        jmp     $EA81

; void sm_guard(void) -- wait while the split is near: from about twenty
; lines before it until it is done. For whatever is about to keep interrupts
; off for up to sixteen lines (a row copied out of the vault, an REU
; transfer), which would hold the split back past where it works. Clobbers A.
GUARD0 = SPLITLINE - 17
sm_guard:
_sm_guard:
:       lda     $D012
        cmp     #GUARD0
        bcc     @ok
        cmp     #TEXTLINE + 3
        bcc     :-
@ok:    rts

        .segment "HICODE"       ; (the rest of the window: MAIN is full)

; void __fastcall__ sm_say(const char *s) -- a line of text: the window's
; bottom line moves up into the top one, and s (PETSCII, up to 40) is the
; new bottom line, in the colour sm_ink (0-7), in both screens.
_sm_say:
        sta     ptr1
        stx     ptr1+1
        ldx     #39
@up:    lda     SCRA + (TEXTROW+1)*40,x
        sta     SCRA + TEXTROW*40,x
        sta     SCRB + TEXTROW*40,x
        lda     $D800 + (TEXTROW+1)*40,x
        sta     $D800 + TEXTROW*40,x
        dex
        bpl     @up
        ldy     #0
@ch:    lda     (ptr1),y
        beq     @pad
        jsr     font_of
        sta     SCRA + (TEXTROW+1)*40,y
        sta     SCRB + (TEXTROW+1)*40,y
        lda     _sm_ink
        and     #7              ; 8-15 would be a multicolour character
        sta     $D800 + (TEXTROW+1)*40,y
        iny
        cpy     #40
        bne     @ch
        rts
@pad:   lda     #FONT + $20
        sta     SCRA + (TEXTROW+1)*40,y
        sta     SCRB + (TEXTROW+1)*40,y
        iny
        cpy     #40
        bne     @pad
        rts

; PETSCII in A to the text's character: its screen code in the lowercase
; set, from FONT. What has none is a space.
font_of:
        cmp     #$20
        bcc     @sp
        cmp     #$40
        bcc     @add            ; $20-$3F: the same
        cmp     #$60
        bcc     @low            ; $40-$5F: a-z and @[£]^<-
        cmp     #$C0
        bcc     @sp
        cmp     #$E0
        bcs     @sp
        and     #$7F            ; $C0-$DF: A-Z
        bne     @add            ; (always)
@low:   and     #$1F
        .byte   $2C             ; (skips the two-byte lda)
@sp:    lda     #$20
@add:   clc
        adc     #FONT
        rts

; void sm_text_clear(void) -- both lines blank (and row 24, which is under
; the border, with them).
_sm_text_clear:
        ldx     #3*40 - 1
        lda     #FONT + $20
:       sta     SCRA + TEXTROW*40,x
        sta     SCRB + TEXTROW*40,x
        dex
        bpl     :-
        rts

; The font: the character ROM's lowercase set, its first 96 (screen codes
; 0-$5F), into the character set from FONT. Above anything a tile set has
; (mkoverworld.py keeps them under it), so it is done once.
font_copy:
        sei
        lda     $01
        pha
        lda     #$33            ; the character ROM in, at $D000
        sta     $01
        ldx     #0
:       lda     $D800,x
        sta     CHARS + FONT*8,x
        lda     $D900,x
        sta     CHARS + FONT*8 + $100,x
        lda     $DA00,x
        sta     CHARS + FONT*8 + $200,x
        inx
        bne     :-
        pla
        sta     $01
        cli
        rts

        .segment "CODE"

; void sm_init(void) -- the hero into the VIC's bank. The display stays as
; it is until sm_show(); a tile set goes in with sm_tileset().
_sm_init:
        lda     #$10
        sta     den
        lda     #<ow_sprites
        sta     ptr1
        lda     #>ow_sprites
        sta     ptr1+1
        lda     #<SPRITES
        sta     ptr2
        lda     #>SPRITES
        sta     ptr2+1
        lda     ow_nsprites     ; 64 bytes a frame: eight eights
        asl     a
        asl     a
        asl     a
        sta     cnt
        jsr     copy8s

        lda     #7 - 4
        ora     #$10
        sta     pend16
        sta     pend11
        lda     #0
        sta     pendswap
        sta     irqstep
        lda     #COLOR_WHITE
        sta     _sm_ink
        jsr     font_copy
        jmp     _sm_text_clear

COLOR_WHITE = 1

; Copy cnt lots of 8 bytes from ptr1 to ptr2.
copy8s:
        ldy     #0
@l:     lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     :+
        inc     ptr1+1
        inc     ptr2+1
:       tya
        and     #7
        bne     @l
        dec     cnt
        bne     @l
        rts

; void sm_show(void) -- switch the VIC over to the scrolling screen, with the
; hero in the middle, and start the interrupt.
_sm_show:
        sei
        lda     $DD00           ; VIC bank 1
        and     #$FC
        ora     #$02
        sta     $DD00
        lda     _sm_front
        beq     :+
        lda     #D018_B
        .byte   $2C
:       lda     #D018_A
        sta     $D018
        lda     pend16
        sta     $D016
        lda     pend11
        sta     $D011
        lda     #0
        sta     $D020

        lda     _ow_spr1        ; the hero: sprite 0, multicolour
        sta     $D025
        lda     _ow_spr2
        sta     $D026
        lda     _sm_tunic       ; the leader's colour
        sta     $D027
        lda     #1
        sta     $D01C
        sta     $D015
        lda     #0
        sta     $D010
        sta     $D017
        sta     $D01D
        sta     $D01B
        lda     #179            ; see engine.c: the square at 148, 84
        sta     $D000
        lda     #138
        sta     $D001

        lda     #$7F            ; the CIA's timer interrupt off ...
        sta     $DC0D
        lda     $DC0D
        lda     #<irq           ; ... and a raster one instead
        sta     $0314
        lda     #>irq
        sta     $0315
        lda     #IRQLINE
        sta     $D012
        lda     $D011
        and     #$7F
        sta     $D011
        lda     #$01
        sta     $D01A
        sta     $D019
        cli
        rts

; void sm_quit(void) -- the machine as the KERNAL had it: its interrupt,
; VIC bank 0 and the text screen, no sprites, so that a program can be
; loaded over this one (ow_run).
_sm_quit:
        sei
        lda     #0
        sta     $D01A           ; no raster interrupt
        sta     $D015           ; no sprites
        lda     #$FF
        sta     $D019
        lda     #<$EA31
        sta     $0314
        lda     #>$EA31
        sta     $0315
        lda     #$81            ; the CIA's timer interrupt again
        sta     $DC0D
        lda     $DD00           ; VIC bank 0
        ora     #$03
        sta     $DD00
        lda     #$17            ; the screen at $0400, the lowercase set
        sta     $D018
        lda     #$1B
        sta     $D011
        lda     #$C8
        sta     $D016
        cli
        rts

        .segment "RODATA"

; A screen row's offset, row * 40, for the rows the edges use.
rowlo40:
        .repeat 25, N
        .byte   <(N * 40)
        .endrep
rowhi40:
        .repeat 25, N
        .byte   >(N * 40)
        .endrep

; The map's rows: WORLD + y * 64.
rowlo:
        .repeat 64, N
        .byte   <(WORLD + N * 64)
        .endrep
rowhi:
        .repeat 64, N
        .byte   >(WORLD + N * 64)
        .endrep
