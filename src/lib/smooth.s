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

        .export _sm_init, _sm_draw_all, _sm_build, _sm_shift, _sm_edge, _sm_commit
        .export _sm_sprite, _sm_animate, _sm_show
        .export _sm_ox, _sm_oy, _sm_fx, _sm_fy, _sm_front, _sm_frame
        .export _sm_colend, _sm_rowend, _sm_irqline

        .import ow_chars, ow_nchars, ow_chcol, ow_mt0, ow_mt1, ow_chq
        .import ow_rnd, ow_water, ow_nwater, ow_sprites, ow_nsprites
        .import _ow_bg, _ow_mc1, _ow_mc2, _ow_spr1, _ow_spr2
        .importzp ptr1, ptr2, ptr3

SCRA     = $4000
SCRB     = $4400
CHARS    = $4800
SPRITES  = $5000
SPRBLOCK = (SPRITES & $3FFF) / 64
WORLD    = $B000                ; 64x64, page aligned
D018_A   = ((SCRA & $3FFF) / $400) * 16 + ((CHARS & $3FFF) / $800) * 2
D018_B   = ((SCRB & $3FFF) / $400) * 16 + ((CHARS & $3FFF) / $800) * 2
IRQLINE  = 250
T_WATER  = $64
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

pend16:     .res 1              ; what the interrupt writes next
pend11:     .res 1
pend18:     .res 1
pendswap:   .res 1

cx:         .res 1              ; char_at's arguments and workings
cy:         .res 1
tx:         .res 1
ty:         .res 1
qofs:       .res 1
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
        .byte   $2C             ; bit abs: skip the next
@sea:   lda     #T_WATER
        sec
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

; The camera's directions are the game's (sprite.h): down, up, left, right.
; Down shows the next row below: everything moves up a row.
a_down:  SHIFT_V SCRA + 40, SCRB
a_up:    SHIFT_V SCRA, SCRB + 40
a_left:  SHIFT_H SCRA, SCRB + 1
a_right: SHIFT_H SCRA + 1, SCRB
b_down:  SHIFT_V SCRB + 40, SCRA
b_up:    SHIFT_V SCRB, SCRA + 40
b_left:  SHIFT_H SCRB, SCRA + 1
b_right: SHIFT_H SCRB + 1, SCRA

shifts:  .addr a_down, a_up, a_left, a_right, b_down, b_up, b_left, b_right

; void __fastcall__ sm_build(unsigned char dir)
; The back screen, as the front would be with the camera a character further
; in `dir`. Never call it while a flip is pending: the back screen is still
; on show until the interrupt has made it. It is the two halves below, about
; 10K and 5K cycles, which can go in different frames (NTSC has no room for
; both in the frame after a flip): sm_shift, then sm_edge, same `dir`.
_sm_build:
        pha
        jsr     _sm_shift
        pla
        jmp     _sm_edge

; void __fastcall__ sm_shift(unsigned char dir) -- the copy.
_sm_shift:
        and     #3
        sta     dir
        lda     _sm_front
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

; void __fastcall__ sm_edge(unsigned char dir) -- the new row or column.
_sm_edge:
        and     #3
        sta     dir
        lda     _sm_front       ; the back screen's page
        beq     :+
        lda     #>SCRA
        .byte   $2C
:       lda     #>SCRB
        sta     backhi
        sta     ptr1+1
        lda     #0
        sta     ptr1

        ldx     dir
        beq     @down
        dex
        beq     @up
        dex
        beq     @left

        lda     #39             ; right: a column on the right
        sta     ptr1
        lda     _sm_ox
        clc
        adc     #40
        sta     cx
        lda     _sm_oy
        sta     cy
        jmp     fill_col

@left:  lda     _sm_ox
        sec
        sbc     #1
        sta     cx
        lda     _sm_oy
        sta     cy
        jmp     fill_col

@down:  lda     #<(24*40)       ; a row at the bottom
        sta     ptr1
        lda     backhi
        clc
        adc     #>(24*40)
        sta     ptr1+1
        lda     _sm_oy
        clc
        adc     #25
        sta     cy
        lda     _sm_ox
        sta     cx
        jmp     fill_row

@up:    lda     _sm_oy
        sec
        sbc     #1
        sta     cy
        lda     _sm_ox
        sta     cx
        jmp     fill_row

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
        lda     _sm_front
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

; The map's rows: WORLD + y * 64.
rowlo:
        .repeat 64, N
        .byte   <(WORLD + N * 64)
        .endrep
rowhi:
        .repeat 64, N
        .byte   >(WORLD + N * 64)
        .endrep
