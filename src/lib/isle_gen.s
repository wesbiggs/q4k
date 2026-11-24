; SPDX-License-Identifier: GPL-3.0-or-later
; The land generator: the home continent and the four across the sea.
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
        .export _isle_story, _isle_port_x, _isle_port_y, _isle_fx, _isle_fy
        .import isle_rnd, isle_seed        ; (isle_rnd.s: the towns' too)
        .import _local, _local_w, _local_h, _lore_seed
        .import vault_enter, vault_leave

W            = 64
H            = 31      ; rows; the 32nd of local[] is not ours
BIOMES       = 5
BIOME_MOUNTS = 4
BIOME_SWAMP  = 2

HOME         = 4        ; the compass point that means the home continent
HOME_AT      = $D000    ; home is built in place, under the I/O area (vault.s)
HOME_H       = 64
G_LAND       = 1
G_MTN        = 2
G_LAKE       = 4
; The specials are even: as far as connectivity goes they are not there, since
; a landing or a town is stepped on and not through.
G_DOCK       = $0A
G_TOWN       = $0C
G_CASTLE     = $0E
G_DUNGEON    = $10
G_LAIR       = $12
G_SHOAL      = $14      ; the shallows round the land (shoal, below) ...
G_NEW        = $16      ; ... while a pass is marking them
SHOAL_PASSES = 5
T_DEEP       = $95      ; the open sea: the overworld set's own code
PROT         = $40      ; added to the land beside a landing: no obstacle takes it
LANDING_APART = 3

OUTER        = 17       ; the walker goes back to the middle this often...
OUTER_HOME   = 30       ; (home has more land to cover, so its rounds are longer:
INNER_HOME   = 256      ; a count of 0 does 256)
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
_isle_dock_x:                   ; a continent's one landing is port 0
_isle_port_x:   .res 4
_isle_dock_y:
_isle_port_y:   .res 4
_isle_fx:       .res 5          ; the places, in the order the lists below give
_isle_fy:       .res 5
_isle_biome:    .res 1
_isle_story:    .res 1          ; a continent's first place: 0 a town, 1 a dungeon
home:           .res 1
base:           .res 2          ; the map: local[], or home's place under the I/O
rowend:         .res 2          ; the offset of the first row the walker may not use
centre:         .res 2
ymask:          .res 1
ylim:           .res 1
pages:          .res 1          ; of map
innerlen:       .res 1
nland:          .res 1
lim:            .res 1
pi:             .res 1          ; which port the dock is for
pidx:           .res 1          ; the place being put down
pcount:         .res 1
pkind:          .res 1
papart:         .res 1
ptries:         .res 1
px:             .res 1
py:             .res 1
ax:             .res 1
ay:             .res 1
need:           .res 1
dd:             .res 1
lay:            .res 1  ; what the walker is laying down
phase:          .res 1  ; 1 land, 2 ranges, 3 lakes, 4 places, 5 shallows,
                        ; 6 tiles, 7 done
arms:           .res 1  ; walks from the middle still to take
dir2:           .res 1  ; twice the compass point, for indexing words
count:          .res 1
inner:          .res 1
code:           .res 1
len:            .res 1
nn:             .res 1
ee:             .res 1
b:              .res 18 ; the ring's walkability, at twice its index

.ifdef OW_WORLD                 ; the open world's engines: in the world's
        .segment "OVWCODE"      ; overlay (Makefile's ow_isle.o)
.else
        .segment "CHDATA1"
.endif

; Steps through local[], N E S W; and the squares about one: the middle,
; the edge neighbours N E S W, then the corners NW NE SE SW.
step:   .word $10000-W, 1, W, $FFFF
ring:   .word 0, $10000-W, 1, W, $FFFF, $10000-W-1, $10000-W+1, W+1, W-1
; Where a ship heading each way comes in: the middle of the opposite edge.
start:  .word (H-1)*W + W/2, (H/2)*W, W/2, (H/2)*W + W-1
        .word (HOME_H-1)*W + W/2, (HOME_H/2)*W, W/2, (HOME_H/2)*W + W-1

; The tile for each G_ value.
final:  .byte T_DEEP, T_GRASS, T_MOUNTAIN, T_FOREST, T_WATER, T_SWAMP
        .byte T_GRASS, T_SHORE, T_GRASS, T_GRASS, T_SHORE, T_GRASS
        .byte T_TOWN, T_GRASS, T_CASTLE, T_GRASS, T_DUNGEON, T_GRASS, T_LAIR
        .byte T_GRASS, T_WATER              ; G_SHOAL: shallow water

; Home's places, and how far each would like to be from the ones before it.
hkind:  .byte G_TOWN, G_CASTLE, G_DUNGEON, G_LAIR, G_TOWN
happart: .byte 0, 24, 12, 12, 10

.ifdef OW_WORLD
        .segment "OVWCODE"
.else
        .segment "MAPCODE"           ; an engine of its own: engine.h
.endif

rnd = isle_rnd                  ; A = a random byte; clobbers X
seed = isle_seed                ; its LFSR

; ptr1 = &map[at]
setp:   clc
        lda at
        adc base
        sta ptr1
        lda at+1
        adc base+1
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
; columns 4-59 and rows 3 to three from the bottom. Returns the draw in A (and in r).
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
        lda to+1                ; rows 3 and on are offsets $00C0 and up
        beq @low
        cmp rowend+1
        bcc @in
        bne @out
        lda to
        cmp rowend
        bcs @out
        bcc @in
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
arm:    lda centre
        sta at
        lda centre+1
        sta at+1
        lda innerlen
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

; A random square at least four in from the sides and three from the ends
; (rows 3 up to ylim):
; px, py and at = py * 64 + px.
pickxy:
@again: jsr rnd
        and #(W-1)
        cmp #4
        bcc @again
        cmp #(W-4)
        bcs @again
        sta px
        jsr rnd
        and ymask
        cmp #3
        bcc @again
        cmp ylim
        bcs @again
        sta py
        ldx #0
        stx at                  ; at = y * 64 + x
        lsr
        ror at
        lsr
        ror at
        sta at+1
        lda at
        ora px
        sta at
        rts

; A mountain range or a lake: a walk from a random walkable square, turning
; into `code` each square it can, for `len` steps. Not the land beside a
; landing, which a landing needs.
obstacle:
@pick:  jsr pickxy
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
        and #PROT
        bne @next
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
; the middle row or column from the edge it comes in at. It becomes a landing,
; port number pi, and the land beside it is spoken for.
dock:   lda dir2
        ldx home
        beq @tab
        clc
        adc #8
@tab:   tax
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
        ldx #8
@prot:  jsr nbr
        lda (ptr2),y
        lsr
        bcc @np
        lda (ptr2),y
        ora #PROT
        sta (ptr2),y
@np:    dex
        dex
        bne @prot
        jsr xy
        ldx pi
        sta _isle_port_x,x
        lda tmp
        sta _isle_port_y,x
        rts

; The landings: a continent's one, or home's four, where the ship for each
; compass point is kept: on the coast that faces it, which is where a ship
; heading the other way would come ashore.
docks:  lda #0
        sta pi
        lda home
        bne @home
        jmp dock
@home:  lda pi
        asl
        eor #4
        sta dir2
        jsr dock
        inc pi
        lda pi
        cmp #4
        bne @home
        rts

; |a - b| of the two coordinates, whichever is larger, against `need`: carry
; set if (px, py) is that far from (ax, ay) or farther.
away:   lda px
        sec
        sbc ax
        bcs @x
        eor #$FF
        adc #1
@x:     sta dd
        lda py
        sec
        sbc ay
        bcs @y
        eor #$FF
        adc #1
@y:     cmp dd
        bcs @m
        lda dd
@m:     cmp need
        rts

; Put place pidx down, on land that can be spared, far enough from the rest
; and, for a lair, with a mountain beside it. How far it would like to be is
; given up two at a time every 128 tries, to four, so that a small land
; cannot stop it.
place:  ldx pidx
        lda home
        beq @cont
        lda hkind,x
        sta pkind
        lda happart,x
        sta papart
        jmp @init
@cont:  lda #8
        sta papart
        lda #G_TOWN
        cpx #0
        bne @k
        ldy _isle_story
        beq @k
        lda #G_DUNGEON
@k:     sta pkind
@init:  lda #0
        sta ptries
@try:   inc ptries
        lda ptries
        and #$7F
        bne @pick
        lda papart
        cmp #5
        bcc @pick
        sbc #2
        sta papart
@pick:  jsr pickxy
        jsr setp
        ldy #0
        lda (ptr1),y
        lsr
        bcc @try1               ; not walkable
        lda (ptr1),y
        cmp #G_DOCK
        bcs @try1               ; beside a landing, or a place
        jsr removable
        bcc @try1
        lda pkind
        cmp #G_LAIR
        bne @far
        ldx #8
@mtn:   jsr nbr
        lda (ptr2),y
        cmp #G_MTN
        beq @far
        dex
        dex
        bne @mtn
@try1:  jmp @try
@far:   lda #LANDING_APART
        sta need
        ldx #0
@land:  lda _isle_port_x,x
        sta ax
        lda _isle_port_y,x
        sta ay
        jsr away
        bcc @try1
        inx
        cpx nland
        bne @land
        lda papart
        sta need
        ldx #0
@old:   cpx pidx
        beq @put
        lda _isle_fx,x
        sta ax
        lda _isle_fy,x
        sta ay
        jsr away
        bcc @try1
        inx
        bne @old
@put:   ldx pidx
        lda px
        sta _isle_fx,x
        lda py
        sta _isle_fy,x
        lda pkind
        ldy #0
        sta (ptr1),y
        rts

; void isle_begin(unsigned char dir): A = the compass point, or HOME. The seed
; is the game's lore_seed over the compass point; eight draws are thrown away
; so that nearby seeds part company.
_isle_begin:
        ldx #0
        cmp #HOME
        bne @nh
        inx
@nh:    stx home
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

        lda #<_local            ; a continent's measurements...
        sta base
        lda #>_local
        sta base+1
        lda #<(((H-3)*W))
        sta rowend
        lda #>(((H-3)*W))
        sta rowend+1
        lda #<((H/2)*W + W/2)
        sta centre
        lda #>((H/2)*W + W/2)
        sta centre+1
        lda #31
        sta ymask
        lda #(H-3)
        sta ylim
        lda #8
        sta pages
        lda #INNER
        sta innerlen
        lda home
        beq @mapped
        lda #<HOME_AT           ; ...or home's
        sta base
        lda #>HOME_AT
        sta base+1
        lda #<(((HOME_H-3)*W))
        sta rowend
        lda #>(((HOME_H-3)*W))
        sta rowend+1
        lda #<((HOME_H/2)*W + W/2)
        sta centre
        lda #>((HOME_H/2)*W + W/2)
        sta centre+1
        lda #63
        sta ymask
        lda #(HOME_H-3)
        sta ylim
        lda #16
        sta pages
        lda #INNER_HOME & $FF
        sta innerlen
@mapped:
        lda base                ; all sea
        sta ptr1
        lda base+1
        sta ptr1+1
        lda pages
        sta count
        ldy #0
        tya
@clr:   sta (ptr1),y
        iny
        bne @clr
        inc ptr1+1
        dec count
        bne @clr
        lda home
        bne @nosize
        lda #W
        sta _local_w
        lda #H
        sta _local_h
@nosize:

        ldx #BIOMES             ; what it is mostly made of: home is only ever
        ldy #1                  ; plains or forest, and has four landings
        lda home
        beq @lim
        ldx #2
        ldy #4
@lim:   stx lim
        sty nland
@dom:   jsr rnd
        and #7
        cmp lim
        bcs @dom
        sta _isle_biome
        asl
        ora #G_LAND
        sta lay
        lda #OUTER
        ldx home
        beq @arms
        lda #OUTER_HOME
@arms:  sta arms
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
        bne @n4
        jmp @places
@n4:    dex
        bne @n5
        jmp @shoal
@n5:    dex
        bne @done
        jmp @tiles
@done:  ldx #0                  ; finished
        lda #0
        rts

@arm:   jsr arm
        dec arms
        bne @more
        jsr docks
        jsr rnd                 ; ranges: 3-6 (4-7 at home), and ten more if
        and #3                  ; it is mountains
        clc
        adc #3
        ldx home
        beq @nh2
        adc #1
@nh2:   ldx _isle_biome
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
@tile:  lda #0                  ; the places: five at home, two on a continent
        sta pidx
        lda #2
        ldx home
        beq @np
        lda #5
@np:    sta pcount
        lda #4
        sta phase
        jmp @more

@places:
        jsr place
        inc pidx
        dec pcount
        bne @pmore
        lda #SHOAL_PASSES
        sta count
        lda #5
        sta phase
@pmore: jmp @more

@shoal: ldx #0                  ; the first pass always, the rest 3 in 4
        lda count
        cmp #SHOAL_PASSES
        beq @first
        inx
@first: stx code
        jsr scan
        dec count
        bne @pmore
        lda #2                  ; and then the clean-up
        sta code
        jsr scan
        lda #6
        sta phase
        jmp @more

@tiles: lda #7
        sta phase
        lda base                ; and now the tiles
        sta ptr1
        lda base+1
        sta ptr1+1
        lda pages
        sta count
        ldy #0
@fin:   lda (ptr1),y
        and #$3F                ; without the mark that kept an obstacle off
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

; The shallows (isleref.py's shoal()): one pass over rows 1 to two from the
; bottom, columns 1-62, making a square of sea G_NEW if one of its eight
; neighbours is held -- not sea, nor marked by this pass -- (code 0: the
; first pass, always; 1: a later one, three times in four), or (code 2, the
; clean-up) if it has them west and east of it or north and south; then the
; pass's marks are made G_SHOAL. ptr2, ptr1 and at are the rows above, on and
; below (the generator thread keeps those, ow_gen.s); tmp the column.
scan:   lda base
        sta ptr2
        clc
        adc #W
        sta ptr1
        lda base+1
        sta ptr2+1
        adc #0
        sta ptr1+1
        lda ylim                ; h - 3 rows, plus one: 1 .. h - 2
        clc
        adc #1
        sta len
@row:   lda ptr1
        clc
        adc #W
        sta at
        lda ptr1+1
        adc #0
        sta at+1
        ldy #1
@sq:    sty tmp
        lda (ptr1),y
        bne @next
        lda code
        cmp #2
        beq @clean
        dey
        jsr col3
        bcs @hit
        iny
        jsr col3
        bcs @hit
        iny
        jsr col3
        bcc @next
@hit:   lda code
        beq @set
        jsr rnd                 ; (keeps Y)
        and #3
        beq @next
@set:   ldy tmp
        lda #G_NEW
        sta (ptr1),y
@next:  ldy tmp
        iny
        cpy #W-1
        bne @sq
        lda ptr1                ; a row down
        sta ptr2
        lda ptr1+1
        sta ptr2+1
        lda at
        sta ptr1
        lda at+1
        sta ptr1+1
        dec len
        bne @row
        jmp promote
@clean: dey
        lda (ptr1),y
        jsr held
        bcc @ns
        iny
        iny
        lda (ptr1),y
        jsr held
        bcs @set
@ns:    ldy tmp
        lda (ptr2),y
        jsr held
        bcc @next
        lda (at),y
        jsr held
        bcs @set
        bcc @next

; Carry set if any of column Y of the rows above, on and below is held.
col3:   lda (ptr2),y
        jsr held
        bcs @y
        lda (ptr1),y
        jsr held
        bcs @y
        lda (at),y
        jmp held
@y:     rts

; Carry set if the square A is held: neither sea nor this pass's mark.
held:   beq @no
        cmp #G_NEW
        beq @no
        sec
        rts
@no:    clc
        rts

; This pass's marks, made shallows.
promote:
        lda base
        sta ptr1
        lda base+1
        sta ptr1+1
        ldx pages
        ldy #0
@p:     lda (ptr1),y
        cmp #G_NEW
        bne @pn
        lda #G_SHOAL
        sta (ptr1),y
@pn:    iny
        bne @p
        inc ptr1+1
        dex
        bne @p
        rts

; void isle_build(unsigned char dir): all of it at once. Home is built where
; it is kept, which takes all-RAM for the length of the build -- several
; seconds in which nothing else runs -- so nobody should be waiting on a
; sound or the clock.
_isle_build:
        cmp #HOME
        beq @home
        jsr _isle_begin
@go:    jsr _isle_work
        bne @go
        rts
@home:  jsr vault_enter
        lda #HOME
        jsr _isle_begin
@hgo:   jsr _isle_work
        bne @hgo
        jmp vault_leave
