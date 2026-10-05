; The continents' generator.
;
; Built in assembly because in C it came to 1.3K, and HIGH has no such room.
; tools/isleref.py is the same algorithm in Python, bit for bit, and
; tools/checkisle.py holds the two against each other; read that for the
; picture, and include/q4k/isle.h for what it promises.
;
; (Not isle.s: cl65 makes its own isle.s from isle.c on the way to isle.o,
; and deletes it afterwards.)
;
; It does not use rand(): it has a random generator of its own, a 16-bit
; LFSR, so building a continent leaves the game's random numbers alone and
; needs no reseeding afterwards.
;
; The work comes in pieces -- isle_begin() and then isle_work() until it says
; there is no more -- because building takes a couple of seconds, and the
; ship should keep sailing while it does. (isle_build() does all of it.) The
; pieces run in the order the Python model's calls fall in, which is what
; keeps the random numbers lined up.
;
; Squares are bytes of local[] -- 64 by 31: the last row is the overworld's
; row buffer, which the ship's voyage fills in while this is under way --
; while it works, and G_ values:
; odd is walkable, so "can I stand here" is one bit. A walkable square is
; G_LAND + 2 * its biome. At the end each becomes a tile through FINAL.

        .include "zeropage.inc"
        .include "q4k/tiles.inc"

        .export _isle_build, _isle_begin, _isle_work
        .export _isle_sx, _isle_sy, _isle_dock_x, _isle_dock_y, _isle_biome
        .import _local, _local_w, _local_h, _lore_seed

W            = 64
H            = 31      ; rows; the 32nd of local[] is not ours
BIOMES       = 5
BIOME_MOUNTS = 4
BIOME_SWAMP  = 2

G_LAND       = 1
G_MTN        = 2
G_LAKE       = 4
G_DOCK       = $0B      ; walkable, but spoken for: never an obstacle

CENTRE       = (H / 2) * W + W / 2
OUTER        = 17       ; the walker goes back to the middle this often...
INNER        = 128      ; ...and takes this many steps between

; Zero page is cc65's scratch, free for the length of one piece of work --
; which calls nothing of cc65's -- but not between pieces. What has to last
; that long is in BSS below.
at      = ptr4          ; where the walker is, as an offset into local[]
; ptr1: the square being looked at.  ptr2: one of its neighbours.
to      = sreg          ; where the walker would step to
r       = tmp1          ; the last draw the walker made
tmp     = tmp3

        .segment "BSS"

_isle_sx:       .res 1
_isle_sy:       .res 1
_isle_dock_x:   .res 1
_isle_dock_y:   .res 1
_isle_biome:    .res 1
seed:           .res 2  ; the LFSR
lay:            .res 1  ; what the walker is laying down
phase:          .res 1  ; 1 land, 2 ranges, 3 lakes, 4 tiles, 5 done
arms:           .res 1  ; walks from the middle still to take
dir2:           .res 1  ; twice the compass point, for indexing words
count:          .res 1
inner:          .res 1
code:           .res 1
len:            .res 1
nn:             .res 1
ee:             .res 1
b:              .res 18 ; the ring's walkability, at twice its index

        .segment "HIDATA"

; Steps through local[], N E S W; and the squares about one: the middle,
; the edge neighbours N E S W, then the corners NW NE SE SW.
step:   .word $10000-W, 1, W, $FFFF
ring:   .word 0, $10000-W, 1, W, $FFFF, $10000-W-1, $10000-W+1, W+1, W-1
; Where a ship heading each way comes in: the middle of the opposite edge.
start:  .word (H-1)*W + W/2, (H/2)*W, W/2, (H/2)*W + W-1

; The tile for each G_ value.
final:  .byte T_WATER, T_GRASS, T_MOUNTAIN, T_FOREST, T_WATER, T_SWAMP
        .byte T_GRASS, T_SHORE, T_GRASS, T_GRASS, T_GRASS, T_PORT

        .segment "HICODE"

; A = a random byte: eight bits of a Galois LFSR, taps $B400, whose period is
; 65535. Clobbers X.
rnd:    ldx #8
@bit:   lsr seed+1
        ror seed
        bcc @no
        lda seed+1
        eor #$B4
        sta seed+1
@no:    dex
        bne @bit
        lda seed
        rts

; ptr1 = &local[at]
setp:   clc
        lda at
        adc #<_local
        sta ptr1
        lda at+1
        adc #>_local
        sta ptr1+1
        rts

; ptr2 = ptr1 + ring[X / 2], and Y = 0.
nbr:    clc
        lda ptr1
        adc ring,x
        sta ptr2
        lda ptr1+1
        adc ring+1,x
        sta ptr2+1
        ldy #0
        rts

; One step of the walker, in a random direction, unless that would leave
; columns 4-59 and rows 3-27. Returns the draw in A (and in r).
walk:   jsr rnd
        sta r
        and #3
        asl
        tax
        clc
        lda at
        adc step,x
        sta to
        lda at+1
        adc step+1,x
        sta to+1
        lda to
        and #(W-1)
        cmp #4
        bcc @out
        cmp #(W-4)
        bcs @out
        lda to+1                ; rows 3-27 are offsets $00C0 up to $06FF
        beq @low
        cmp #7
        bcc @in
        bcs @out
@low:   lda to
        cmp #$C0
        bcc @out
@in:    lda to
        sta at
        lda to+1
        sta at+1
@out:   lda r
        rts

; One arm of the land: INNER steps of the walker from the middle. Each step
; stamps a plus, or a 3x3 three times in four, and the next starts inside it,
; so all of it is joined. Now and then the walker takes up another biome --
; usually the dominant one -- and lays that down until it next does.
arm:    lda #<CENTRE
        sta at
        lda #>CENTRE
        sta at+1
        lda #INNER
        sta inner
@step:  jsr walk
        jsr rnd
        and #63
        bne @stamp
        jsr rnd
        and #15
        cmp #BIOMES
        bcc @pick
        lda _isle_biome
@pick:  asl
        ora #G_LAND
        sta lay
@stamp: jsr setp
        ldx #16                 ; nine squares...
        lda r
        and #$0C
        bne @cell
        ldx #8                  ; ...or five
@cell:  jsr nbr
        lda lay
        sta (ptr2),y
        dex
        dex
        bpl @cell
        dec inner
        bne @step
        rts

; Can the square at ptr1 be taken out of the walkable ones without cutting
; any of the rest off? Carry set if so.
;
; Going round the eight squares about it, the ones that touch it edgewise
; must all be on one run of walkable squares -- and a corner joins the two
; edges beside it only if it is walkable too. Then anything that walked
; through this square can walk round it. Four edges are joined if there are
; at least one fewer links than edges, a link being two adjacent edges and
; the corner between them.
removable:
        ldx #16
@read:  jsr nbr
        lda (ptr2),y
        and #1
        sta b,x
        dex
        dex
        bne @read
        lda b+2                 ; edges: N E S W are b1-b4, at b+2, +4, +6, +8
        clc
        adc b+4
        adc b+6
        adc b+8
        sta nn
        lda b+2                 ; links: N&E&NE, E&S&SE, S&W&SW, W&N&NW
        and b+4
        and b+12
        sta ee
        lda b+4
        and b+6
        and b+14
        clc
        adc ee
        sta ee
        lda b+6
        and b+8
        and b+16
        clc
        adc ee
        sta ee
        lda b+8
        and b+2
        and b+10
        clc
        adc ee
        sta ee
        lda nn
        beq @no
        lda ee
        clc
        adc #1
        cmp nn                  ; carry set if e + 1 >= n
        rts
@no:    clc
        rts

; A mountain range or a lake: a walk from a random walkable square, turning
; into `code` each square it can, for `len` steps.
obstacle:
@pick:  jsr rnd
        and #(W-1)
        cmp #4
        bcc @pick
        cmp #(W-4)
        bcs @pick
        sta tmp                 ; x
        jsr rnd
        and #31
        cmp #3
        bcc @pick
        cmp #(H-3)
        bcs @pick               ; A = y
        ldx #0
        stx at                  ; at = y * 64 + x
        lsr
        ror at
        lsr
        ror at
        sta at+1
        lda at
        ora tmp
        sta at
        jsr setp
        ldy #0
        lda (ptr1),y
        lsr
        bcc @pick               ; not walkable: draw again
@walk:  jsr setp
        ldy #0
        lda (ptr1),y
        lsr
        bcc @next
        lda (ptr1),y
        cmp #G_DOCK
        beq @next
        jsr removable
        bcc @next
        lda code
        ldy #0
        sta (ptr1),y
@next:  jsr walk
        dec len
        bne @walk
        rts

; A = at & 63, tmp = at / 64.
xy:     lda at+1
        sta tmp
        lda at
        asl
        rol tmp
        asl
        rol tmp
        lda at
        and #(W-1)
        rts

; Where a ship heading dir2 / 2 comes ashore: the first walkable square along
; the middle row or column from the edge it comes in at.
dock:   ldx dir2
        lda start,x
        sta at
        lda start+1,x
        sta at+1
        jsr xy
        sta _isle_sx
        lda tmp
        sta _isle_sy
@scan:  jsr setp
        ldy #0
        lda (ptr1),y
        lsr
        bcs @found
        ldx dir2
        clc
        lda at
        adc step,x
        sta at
        lda at+1
        adc step+1,x
        sta at+1
        jmp @scan
@found: lda #G_DOCK
        sta (ptr1),y
        jsr xy
        sta _isle_dock_x
        lda tmp
        sta _isle_dock_y
        rts

; void isle_begin(unsigned char dir): A = the compass point. The seed is the
; game's lore_seed over the compass point; eight draws are thrown away so
; that nearby seeds part company.
_isle_begin:
        asl
        sta dir2
        asl
        asl
        ora #5
        sta seed
        lda _lore_seed
        sta seed+1
        ldy #8
@warm:  jsr rnd
        dey
        bne @warm

        lda #<_local            ; all sea
        sta ptr1
        lda #>_local
        sta ptr1+1
        lda #8
        sta count
        ldy #0
        tya
@clr:   sta (ptr1),y
        iny
        bne @clr
        inc ptr1+1
        dec count
        bne @clr
        lda #W
        sta _local_w
        lda #H
        sta _local_h

@dom:   jsr rnd                 ; what it is mostly made of
        and #7
        cmp #BIOMES
        bcs @dom
        sta _isle_biome
        asl
        ora #G_LAND
        sta lay
        lda #OUTER
        sta arms
        lda #1
        sta phase
        rts

; unsigned char isle_work(void): one piece of what is left, and nonzero if
; there is more after it.
_isle_work:
        ldx phase
        dex
        beq @arm
        dex
        bne @n2
        jmp @range
@n2:    dex
        bne @n3
        jmp @lake
@n3:    dex
        bne @done
        jmp @tiles
@done:  ldx #0                  ; finished
        lda #0
        rts

@arm:   jsr arm
        dec arms
        bne @more
        jsr dock
        jsr rnd                 ; ranges: 3-6, and ten more if it is mountains
        and #3
        clc
        adc #3
        ldx _isle_biome
        cpx #BIOME_MOUNTS
        bne @nm
        clc
        adc #10
@nm:    sta count
        lda #2
        sta phase
@more:  ldx #0
        lda #1
        rts

@range: jsr rnd
        and #15
        clc
        adc #20
        sta len
        lda #G_MTN
        sta code
        jsr obstacle
        dec count
        beq @lakes
        jmp @more
@lakes: jsr rnd                 ; lakes: 1-2, and four more if it is a swamp
        and #1
        clc
        adc #1
        ldx _isle_biome
        cpx #BIOME_SWAMP
        bne @nl
        clc
        adc #4
@nl:    sta count
        lda #3
        sta phase
        jmp @more

@lake:  jsr rnd
        and #7
        clc
        adc #14
        sta len
        lda #G_LAKE
        sta code
        jsr obstacle
        dec count
        beq @tile
        jmp @more
@tile:  lda #4
        sta phase
        jmp @more

@tiles: lda #5
        sta phase
        lda #<_local            ; and now the tiles
        sta ptr1
        lda #>_local
        sta ptr1+1
        lda #8
        sta count
        ldy #0
@fin:   lda (ptr1),y
        tax
        lda final,x
        sta (ptr1),y
        iny
        bne @fin
        inc ptr1+1
        dec count
        bne @fin
        ldx #0
        lda #0
        rts

; void isle_build(unsigned char dir): all of it at once.
_isle_build:
        jsr _isle_begin
@go:    jsr _isle_work
        bne @go
        rts
