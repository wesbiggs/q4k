; The towns that are made rather than drawn: the map and the people in it.
;
; In assembly because in C it came to 1.8K, when HIGH had a hundred bytes.
; tools/townref.py is the same thing in Python, draw for draw; read that for
; the layout. The random numbers are the land generator's LFSR (isle_gen.s),
; seeded afresh.
;
; (Not town_gen.s: cl65 would make its own from town_gen.c and delete it.)

        .include "zeropage.inc"
        .include "q4k/tiles.inc"

        .export _town_tiles, _proc_ents, _proc_n
        .import _local, _lore_seed, _land
        .import isle_rnd, isle_seed

W            = 21
H            = 19

ENT_SHOP     = 1
ENT_INN      = 2
ENT_VILLAGER = 3
SHOP_ITEM    = 2
SHOP_BOW     = 3
SHOP_MAGIC   = 4
ISLE_HOME    = 4
INN_PROC     = 255

        .segment "BSS"

tx:             .res 1          ; a rectangle, or a square, and what goes in it
ty:             .res 1
tw:             .res 1
th:             .res 1
tt:             .res 1
rows:           .res 1
cols:           .res 1
gnd:            .res 1          ; this land's ground, and its scenery
scn:            .res 1
lot:            .res 1
si:             .res 1
mt:             .res 1
shopk:          .res 1
third:          .res 1
nb:             .res 1          ; buildings
bn:             .res 1          ; the one being made
bw:             .res 1
bh:             .res 1
bx0:            .res 1
by0:            .res 1
byb:            .res 1
bdx:            .res 1
dectile:        .res 1
ord:            .res 6
_proc_n:        .res 1
_proc_ents:     .res 20         ; five people: x, y, kind, param

        .segment "CHDATA1"

ground: .byte T_GRASS, T_GRASS, T_SWAMP, T_SHORE, T_GRASS
scenery: .byte 0, T_TREE, 0, 0, T_BOULDER
stock:  .byte T_SWORD, T_SHIELD, T_POTION, T_BOW
lotx:   .byte 2, 8, 14, 2, 8, 14        ; where each lot starts, and where the
loty:   .byte 2, 2, 2, 9, 9, 9          ; bottom wall of a building in it goes
lotb:   .byte 7, 7, 7, 14, 14, 14

        .segment "MAPCODE"           ; an engine of its own: engine.h

; ptr1 = &local[ty * 64 + tx]
locate: lda ty
        ldx #0
        stx ptr1
        lsr
        ror ptr1
        lsr
        ror ptr1
        sta ptr1+1
        lda ptr1
        ora tx
        clc
        adc #<_local
        sta ptr1
        lda ptr1+1
        adc #>_local
        sta ptr1+1
        rts

put:    jsr locate
        ldy #0
        lda tt
        sta (ptr1),y
        rts

; Fill the tw by th rectangle at tx, ty with tt.
rect:   jsr locate
        lda th
        sta rows
@row:   ldy tw
        dey
        lda tt
@c:     sta (ptr1),y
        dey
        bpl @c
        lda ptr1
        clc
        adc #64
        sta ptr1
        bcc @n
        inc ptr1+1
@n:     dec rows
        bne @row
        rts

; A = A mod X.
modn:   stx mt
@l:     cmp mt
        bcc @d
        sbc mt
        bcs @l
@d:     rts

; Somebody stands in the building's doorway, one square in: A = kind, X = param.
ent:    pha
        lda _proc_n
        asl
        asl
        tay
        lda bdx
        sta _proc_ents,y
        lda byb
        sec
        sbc #2
        sta _proc_ents+1,y
        pla
        sta _proc_ents+2,y
        txa
        sta _proc_ents+3,y
        inc _proc_n
        rts

; void town_tiles(unsigned char biome): the town of the land you are on, into
; local[], and its people into proc_ents.
_town_tiles:
        tax
        lda ground,x
        sta gnd
        lda scenery,x
        sta scn

        lda _land               ; the seed, as the land's is
        asl
        asl
        asl
        ora #3
        sta isle_seed
        lda _lore_seed
        sta isle_seed+1
        ldy #4
@warm:  jsr isle_rnd
        dey
        bne @warm

        lda #0                  ; a wall all round, floor within
        sta tx
        sta ty
        sta _proc_n
        lda #W
        sta tw
        lda #H
        sta th
        lda #T_WALL
        sta tt
        jsr rect
        lda #1
        sta tx
        sta ty
        lda #(W-2)
        sta tw
        lda #(H-2)
        sta th
        lda #T_FLOOR
        sta tt
        jsr rect

        lda #0                  ; the lots: ground, and scenery one in four
        sta lot
@lot:   ldx lot
        lda loty,x
        sta ty
        lda #6
        sta rows
@row:   ldx lot
        lda lotx,x
        sta tx
        lda #5
        sta cols
@cell:  lda gnd
        sta tt
        lda scn
        beq @put
        jsr isle_rnd
        and #3
        bne @put
        lda scn
        sta tt
@put:   jsr put
        inc tx
        dec cols
        bne @cell
        inc ty
        dec rows
        bne @row
        inc lot
        lda lot
        cmp #6
        bne @lot

        ldx #5                  ; which lot each building gets: a shuffle
@id:    txa
        sta ord,x
        dex
        bpl @id
        ldx #5
@shuf:  stx si
        jsr isle_rnd
        ldx si
        inx
        jsr modn
        tay
        ldx si
        lda ord,x
        sta mt
        lda ord,y
        sta ord,x
        lda mt
        sta ord,y
        ldx si
        dex
        bne @shuf

        jsr isle_rnd            ; the shop, the third building, the houses
        ldx #3
        jsr modn
        sta shopk
        jsr isle_rnd
        ldx #3
        jsr modn
        sta third
        lda _land               ; home's: always the magic shop, in its place
        cmp #ISLE_HOME
        bne @nomag
        lda #0
        sta third
@nomag:
        jsr isle_rnd
        ldx #3
        jsr modn
        clc
        adc #3
        sta nb

        lda #0
        sta bn
@bld:   ldx bn
        lda ord,x
        sta lot
        lda #5
        sta bw
        sta bh
        lda bn                  ; the third, if it is a house, is small or large
        cmp #2
        bne @size
        lda third
        cmp #1
        bne @big
        lda #4
        sta bw
        sta bh
        bne @size
@big:   cmp #2
        bne @size
        lda #6
        sta bh
@size:  ldx lot
        lda lotx,x
        sta bx0
        lda bw
        cmp #4
        bne @flush
        jsr isle_rnd
        and #1
        clc
        adc bx0
        sta bx0
@flush: ldx lot
        lda lotb,x
        sta byb
        sec
        sbc bh
        clc
        adc #1
        sta by0
        lda bx0                 ; walls, and floor in them
        sta tx
        lda by0
        sta ty
        lda bw
        sta tw
        lda bh
        sta th
        lda #T_WALL
        sta tt
        jsr rect
        inc tx
        inc ty
        dec tw
        dec tw
        dec th
        dec th
        lda #T_FLOOR
        sta tt
        jsr rect
        jsr isle_rnd            ; the door
        ldx bw
        dex
        dex
        jsr modn
        clc
        adc bx0
        adc #1
        sta bdx
        sta tx
        lda byb
        sta ty
        lda #T_DOOR
        sta tt
        jsr put

        lda #0                  ; who is in, and what they sell
        sta dectile
        lda bn
        bne @shop
        lda #ENT_INN
        ldx #INN_PROC
        jsr ent
        lda #T_BED
        sta dectile
        jmp @decor
@shop:  cmp #1
        bne @third
        ldx shopk
        cpx #2
        bne @sk
        ldx #SHOP_BOW
@sk:    lda stock,x
        sta dectile
        lda #ENT_SHOP
        jsr ent
        jmp @decor
@third: cmp #2
        bne @house
        lda third
        bne @decor
        lda _land
        cmp #ISLE_HOME
        beq @magic
        lda #T_POTION
        sta dectile
        lda #ENT_SHOP
        ldx #SHOP_ITEM
        jsr ent
        jmp @decor
@magic: lda #T_BOOK
        sta dectile
        lda #ENT_SHOP
        ldx #SHOP_MAGIC
        jsr ent
        jmp @decor
@house: lda #ENT_VILLAGER
        ldx #0
        jsr ent

@decor: lda dectile             ; on the corners of the top row
        beq @next
        sta tt
        lda bx0
        clc
        adc #1
        sta tx
        lda by0
        clc
        adc #1
        sta ty
        jsr put
        lda bw
        sec
        sbc #2
        clc
        adc bx0
        sta tx
        jsr put
@next:  inc bn
        lda bn
        cmp nb
        beq @gate
        jmp @bld

@gate:  lda #(W/2)
        sta tx
        lda #(H-1)
        sta ty
        lda #T_DOOR
        sta tt
        jmp put
