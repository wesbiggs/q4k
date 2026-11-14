; q4k -- smooth scrolling over a multicolour tile map, double buffered.
;
; The prototype in src/openworld.c walks Keldmarch with the camera moving a
; pixel or two a frame instead of a square at a time. This is its engine.
;
; VIC bank 1 ($4000-$7FFF), which has no character ROM image in it:
;
;   $4000  screen A          $4800  the character set (multicolour)
;   $4400  screen B          $5000  sprite blocks 64-71 (the hero)
;
; The map is a square's tile code per byte, 64 by 64, at WORLD (the
; generator's home, copied out of the vault). Every square is a metatile of
; 2x2 characters (src/lib/ow_data.s, from assets/overworld.txt), so the
; screen's 40x25 characters are 20x12.5 squares. World coordinates in
; characters are biased by 64 -- the map's 128 characters sit at 64-191 --
; so that the sea around it is still a byte, and anything outside is sea.
;
; Coastlines. SHORE, beside WORLD, has a byte for each water square: a bit
; for each of its eight neighbours that is land (openworld.c works it out
; when the land is built: bit 0 north, then clockwise); and for each grass
; square, a bit for each that is sea, whose only use is a headland: a
; quarter of grass with sea beside and above (or below) it rounds its
; corner off with sand. Each quarter of a
; water square looks only at the three neighbours on its own corner -- the
; one beside it, the one above or below, and the diagonal -- and, through
; ow_qvar, picks a beach edge: a strip, an inside corner where both sides are
; land, or a small outside corner where only the diagonal is. So a coast is
; drawn in sixteen characters whatever its shape, and costs a lookup more.
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
        .export _sm_ox, _sm_oy, _sm_fx, _sm_fy, _sm_front, _sm_frame
        .export _sm_colend, _sm_rowend, _sm_irqline
        .export _sm_reu, _sm_reu_detect

        .import ow_chars, ow_nchars, ow_chcol, ow_mt0, ow_mt1, ow_chq
        .import ow_qvar, ow_shore
        .import ow_rnd, ow_water, ow_nwater, ow_sprites, ow_nsprites
        .import _ow_bg, _ow_mc1, _ow_mc2, _ow_spr1, _ow_spr2
        .importzp ptr1, ptr2, ptr3, ptr4

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
REU_TEST   = $0800              ; and sm_reu_detect's test bytes
T_WATER  = $64
T_GRASS  = $60
T_PORT   = $7E
T_BRIDGE = $6A
TBASE    = $60
BIAS     = 64                   ; world characters: the map is 64-191

        .segment "BSS"

_sm_ox:     .res 1              ; the front screen's top left, in characters
_sm_oy:     .res 1
_sm_fx:     .res 1              ; how far into it the camera is, 0-7
_sm_fy:     .res 1
_sm_front:  .res 1              ; 0: screen A is shown, 1: B
_sm_frame:  .res 1              ; counts the interrupts
_sm_colend: .res 1              ; the raster line the last colour copy ended on
_sm_irqline: .res 1             ; the line the interrupt began on
_sm_rowend: .res 25             ; ... and each of its rows (low 8 bits)

_sm_reu:    .res 1              ; 1: build and flip through the REU
built_reu:  .res 1              ; the back screen was built through it ...
built_dir:  .res 1              ; ... moving this way
r_c64:      .res 2              ; reu_go's arguments
r_addr:     .res 2
r_len:      .res 2
pendreu:    .res 1              ; the flip's colour comes from the REU
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
tcode:      .res 1
sy:         .res 1              ; sm_shores' workings
dmask:      .res 1
inv:        .res 1
probe_buf:  .res 8              ; sm_reu_detect's test bytes
above:      .res 66             ; sm_shores' rows of flags
cur:        .res 66
below:      .res 66
cnt:        .res 1
dir:        .res 1
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

        lda     ty
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
        ora     rowlo,y
        sta     ptr2
        lda     rowhi,y
        sta     ptr2+1
        ldy     #0
        lda     (ptr2),y
        cmp     #T_WATER
        beq     @coast
        cmp     #T_GRASS
        bne     @tile
@coast: sta     tcode
        lda     ptr2+1          ; is there a coast on this corner?
        sec
        sbc     #>(WORLD - SHORE)
        sta     ptr2+1
        lda     (ptr2),y        ; what is round it, a bit a compass point
        beq     @same
        tay
        lda     qofs            ; the quadrant's own view of those bits,
        lsr     a               ; from ow_qvar + quadrant * 256
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     #>ow_qvar
        sta     ptr2+1
        lda     #<ow_qvar
        sta     ptr2
        lda     (ptr2),y
        beq     @same           ; a coast, but not on this quarter's side
        ldx     tcode
        cpx     #T_WATER
        beq     :+
        cmp     #3              ; grass: only a headland, sea on both sides
        bne     @same
        lda     #7
:       sta     vtmp
        lda     qofs
        lsr     a
        lsr     a               ; quadrant * 8
        ora     vtmp
        tax
        lda     ow_shore,x
        rts
@same:  lda     tcode
        .byte   $2C             ; bit abs: skip the next
@sea:   lda     #T_WATER
@tile:  sec
        sbc     #TBASE
        and     #31
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
@mt:    ora     qofs
        tax
        lda     ow_chq,x
        rts

; ---------------------------------------------------------------------------
; void sm_shores(void) -- SHORE from WORLD (see the top): for each water
; square a bit for each neighbour that is land, and for each grass square a
; bit for each that is sea, north first and then clockwise. Off the map is
; sea. Three rows of land-or-sea flags roll down the map, each with a square
; of sea either side, so every neighbour is a load, a shift and a rotate,
; with no edge to test. Built once a land.

; Carry set if the square in A is land: anything but water, a pier or a
; bridge.
isdry:  cmp     #T_WATER
        beq     @no
        cmp     #T_PORT
        beq     @no
        cmp     #T_BRIDGE
        beq     @no
        sec
        rts
@no:    clc
        rts

; The row at ptr3 as flags, into `below` (1 land, 0 sea; index x + 1).
dryrow: ldy     #63
@l:     lda     (ptr3),y
        jsr     isdry
        lda     #0
        rol     a
        sta     below+1,y
        dey
        bpl     @l
        rts

_sm_shores:
        lda     #0
        ldx     #65
:       sta     above,x
        sta     cur,x
        sta     below,x
        dex
        bpl     :-
        lda     #<WORLD
        sta     ptr3
        sta     ptr1
        sta     ptr2
        lda     #>WORLD
        sta     ptr3+1
        sta     ptr1+1
        lda     #>SHORE
        sta     ptr2+1
        jsr     dryrow          ; row 0, to move up into `cur`
        lda     #0
        sta     sy

@row:   ldx     #65             ; roll the rows up one
:       lda     cur,x
        sta     above,x
        lda     below,x
        sta     cur,x
        dex
        bpl     :-
        lda     ptr3            ; and read the next, if there is one
        clc
        adc     #64
        sta     ptr3
        bcc     :+
        inc     ptr3+1
:       lda     sy
        cmp     #63
        bne     :+
        lda     #0              ; below the bottom row: sea
        ldx     #64
@sea:   sta     below+1,x
        dex
        bpl     @sea
        bmi     @cols
:       jsr     dryrow

@cols:  ldy     #63
@col:   lda     (ptr1),y
        ldx     #0              ; water: the land round it
        cmp     #T_WATER
        beq     @look
        dex                     ; grass: the sea round it
        cmp     #T_GRASS
        beq     @look
        lda     #0
        beq     @store
@look:  stx     inv
        tya
        tax
        inx                     ; the flags' index for this square
        lda     above,x         ; north
        lsr     a
        ror     dmask
        lda     above+1,x       ; north east
        lsr     a
        ror     dmask
        lda     cur+1,x         ; east
        lsr     a
        ror     dmask
        lda     below+1,x       ; south east
        lsr     a
        ror     dmask
        lda     below,x         ; south
        lsr     a
        ror     dmask
        lda     below-1,x       ; south west
        lsr     a
        ror     dmask
        lda     cur-1,x         ; west
        lsr     a
        ror     dmask
        lda     above-1,x       ; north west
        lsr     a
        ror     dmask
        lda     dmask
        eor     inv
@store: sta     (ptr2),y
        dey
        bpl     @col

        lda     ptr1
        clc
        adc     #64
        sta     ptr1
        sta     ptr2
        bcc     :+
        inc     ptr1+1
        inc     ptr2+1
:       inc     sy
        lda     sy
        cmp     #64
        beq     :+
        jmp     @row
:       rts

; A row of 40 from (cx, cy) into ptr1.
fill_row:
        lda     #0
        sta     cnt
@l:     jsr     char_at
        ldy     cnt
        sta     (ptr1),y
        inc     cx
        iny
        sty     cnt
        cpy     #40
        bne     @l
        rts

; A column of 25 from (cx, cy) down into ptr1.
fill_col:
        lda     #25
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
; from the top so the beam never overtakes it. About 570 cycles a row.

.macro COLCOPY scr
        .local row
        .repeat 25, R
        ldx     #9
:
        .repeat 4, K
        ldy     scr + R*40 + K*10, x
        lda     ow_chcol, y
        sta     $D800 + R*40 + K*10, x
        .endrep
        dex
        bpl     :-
        lda     $D012           ; when each row was done, for measuring
        sta     _sm_rowend + R
        .endrep
        rts
.endmacro

colour_a:
        COLCOPY SCRA
colour_b:
        COLCOPY SCRB

; ---------------------------------------------------------------------------
; The back screen as the front moved one character. Column by column, each
; column all 24 or 25 rows unrolled: nine cycles a byte.

.macro SHIFT_H src, dst         ; 39 columns of 25 rows
        .local loop, done
        ldx     #38
loop:
        .repeat 25, R
        lda     src + R*40, x
        sta     dst + R*40, x
        .endrep
        dex
        bmi     done
        jmp     loop
done:   rts
.endmacro

.macro SHIFT_V src, dst         ; 40 columns of 24 rows
        .local loop, done
        ldx     #39
loop:
        .repeat 24, R
        lda     src + R*40, x
        sta     dst + R*40, x
        .endrep
        dex
        bmi     done
        jmp     loop
done:   rts
.endmacro

.macro SHIFT_D src, dst         ; 39 columns of 24 rows
        .local loop, done
        ldx     #38
loop:
        .repeat 24, R
        lda     src + R*40, x
        sta     dst + R*40, x
        .endrep
        dex
        bmi     done
        jmp     loop
done:   rts
.endmacro

; The camera's directions are the game's (sprite.h): down, up, left, right;
; then the diagonals, down-left, down-right, up-left, up-right. Down shows
; the next row below: everything moves up a row.
a_down:  SHIFT_V SCRA + 40, SCRB
a_up:    SHIFT_V SCRA, SCRB + 40
a_left:  SHIFT_H SCRA, SCRB + 1
a_right: SHIFT_H SCRA + 1, SCRB
a_dl:    SHIFT_D SCRA + 40, SCRB + 1
a_dr:    SHIFT_D SCRA + 41, SCRB
a_ul:    SHIFT_D SCRA, SCRB + 41
a_ur:    SHIFT_D SCRA + 1, SCRB + 40
b_down:  SHIFT_V SCRB + 40, SCRA
b_up:    SHIFT_V SCRB, SCRA + 40
b_left:  SHIFT_H SCRB, SCRA + 1
b_right: SHIFT_H SCRB + 1, SCRA
b_dl:    SHIFT_D SCRB + 40, SCRA + 1
b_dr:    SHIFT_D SCRB + 41, SCRA
b_ul:    SHIFT_D SCRB, SCRA + 41
b_ur:    SHIFT_D SCRB + 1, SCRA + 40

shifts:  .addr a_down, a_up, a_left, a_right, a_dl, a_dr, a_ul, a_ur
         .addr b_down, b_up, b_left, b_right, b_dl, b_dr, b_ul, b_ur

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
        lda     _sm_reu
        sta     built_reu
        beq     :+
        jmp     reu_shift
:       lda     _sm_front
        asl     a
        asl     a
        asl     a
        ora     dir
        asl     a
        tax
        lda     shifts,x
        sta     ptr3
        lda     shifts+1,x
        sta     ptr3+1
        jmp     (ptr3)

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
        lda     #<(24*40)       ; the bottom row, from the new origin's
        sta     ptr1
        lda     backhi
        clc
        adc     #>(24*40)
        sta     ptr1+1
        lda     _sm_oy
        clc
        adc     #25             ; (oy + 1) + 24
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
        sta     ptr1+1
        lda     #0
        sta     ptr1
        lda     _sm_oy
        sta     cy
        lda     #25
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
        lda     _sm_front
        bne     :+
        jmp     colour_a
:       jmp     colour_b

; ---------------------------------------------------------------------------
; void __fastcall__ sm_commit(unsigned char flip)
; What the next interrupt shows: the fine scroll from sm_fx and sm_fy and, if
; flip, the other screen. All of it at once, so that a frame never gets the
; new fine scroll with the old screen.
_sm_commit:
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
        ora     #$10            ; display on, 24 rows
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
        lda     built_reu
        sta     pendreu
        lda     #0
        sta     built_reu       ; until the next build says otherwise
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
; sm_reu_detect() looks for one and then checks that a transfer really
; lands in colour RAM, which an emulated REU might not do; sm_reu is set
; only if both are so, and the C side may clear it to compare.

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

; r_addr = base + max(off, 0), r_c64 = c64 + max(-off, 0), r_len = 1000 -
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
        lda     #<1000
        sec
        sbc     offlo,x
        sta     r_len
        lda     #>1000
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
        lda     #<1000          ; 1000 + off
        clc
        adc     offlo,x
        sta     r_len
        lda     #>1000
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

len1000:
        lda     #<1000
        sta     r_len
        lda     #>1000
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
        ldy     #24             ; ... or the bottom
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
        ldx     #25
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

; unsigned char sm_reu_detect(void) -- 1, and sm_reu set, if there is an
; REU and a transfer into colour RAM lands there; 0 if there is none; 2 if
; there is one but colour RAM did not take the transfer (then sm_reu stays
; 0: half a REU path is worse than none). The test bytes are colour RAM's
; last 24, past the screen's thousand, where nothing shows.
_sm_reu_detect:
        lda     #0
        sta     _sm_reu
        ldx     #5              ; registers that read back what was written?
:       txa
        eor     #$A5
        sta     REU_STATUS,x
        dex
        cpx     #1
        bne     :-
        ldx     #2
:       txa
        eor     #$A5
        cmp     REU_STATUS,x
        bne     @none
        inx
        cpx     #6
        bne     :-
        sei
        ldx     #7              ; a pattern through it and back
:       txa
        eor     #$5A
        sta     probe_buf,x
        dex
        bpl     :-
        jsr     probe_set
        lda     #STASH
        jsr     reu_go
        ldx     #7
        lda     #0
:       sta     probe_buf,x
        dex
        bpl     :-
        jsr     probe_set
        lda     #FETCH
        jsr     reu_go
        cli
        ldx     #7
:       txa
        eor     #$5A
        cmp     probe_buf,x
        bne     @none
        dex
        bpl     :-

        sei                     ; now into colour RAM: 3, 12, 5, 10, ...
        ldx     #7
:       lda     ctest,x
        sta     probe_buf,x
        lda     #0
        sta     $DBE8,x
        dex
        bpl     :-
        jsr     probe_set
        lda     #STASH
        jsr     reu_go
        lda     #<$DBE8
        sta     r_c64
        lda     #>$DBE8
        sta     r_c64+1
        lda     #FETCH
        jsr     reu_go
        cli
        ldx     #7
:       lda     $DBE8,x
        and     #$0F
        cmp     ctest,x
        bne     @nocol
        dex
        bpl     :-
        lda     #1
        sta     _sm_reu
        ldx     #0
        rts
@nocol: lda     #2
        ldx     #0
        rts
@none:  lda     #0
        tax
        rts

probe_set:
        lda     #<probe_buf
        sta     r_c64
        lda     #>probe_buf
        sta     r_c64+1
        lda     #<REU_TEST
        sta     r_addr
        lda     #>REU_TEST
        sta     r_addr+1
        lda     #8
        sta     r_len
        lda     #0
        sta     r_len+1
        rts

ctest:  .byte   3, 12, 5, 10, 15, 0, 9, 6

; ---------------------------------------------------------------------------
; The raster interrupt, from the KERNAL's vector at $0314.

irq:
        lda     $D012
        sta     _sm_irqline
        lda     #$01
        sta     $D019
        lda     pend16
        sta     $D016
        lda     pend11
        sta     $D011
        lda     pendswap
        beq     @tail
        lda     #0
        sta     pendswap
        lda     pend18
        sta     $D018
        lda     pendreu
        beq     :+
        jsr     reu_colour
        jmp     :+++
:       lda     _sm_front
        bne     :+
        jsr     colour_a
        jmp     :++
:       jsr     colour_b
:       lda     $D012
        sta     _sm_colend
@tail:  inc     _sm_frame
        jmp     $EA31

; void sm_init(void) -- the character set and the hero into the VIC's bank,
; and the interrupt in. The display stays as it is until sm_show().
_sm_init:
        ldx     #0              ; the character set: what there is, then blank
        lda     #0
:       sta     CHARS,x
        sta     CHARS+$100,x
        sta     CHARS+$200,x
        sta     CHARS+$300,x
        sta     CHARS+$400,x
        sta     CHARS+$500,x
        sta     CHARS+$600,x
        sta     CHARS+$700,x
        inx
        bne     :-
        lda     #<ow_chars
        sta     ptr1
        lda     #>ow_chars
        sta     ptr1+1
        lda     #<CHARS
        sta     ptr2
        lda     #>CHARS
        sta     ptr2+1
        lda     ow_nchars
        sta     cnt
        jsr     copy8s

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
        rts

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
        lda     _ow_bg
        sta     $D021
        lda     _ow_mc1
        sta     $D022
        lda     _ow_mc2
        sta     $D023

        lda     _ow_spr1        ; the hero: sprite 0, multicolour
        sta     $D025
        lda     _ow_spr2
        sta     $D026
        lda     #2              ; red tunic
        sta     $D027
        lda     #1
        sta     $D01C
        sta     $D015
        lda     #0
        sta     $D010
        sta     $D017
        sta     $D01D
        sta     $D01B
        lda     #179            ; see openworld.c: the square at 148, 84
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
