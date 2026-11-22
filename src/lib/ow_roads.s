; The open world's roads on home (Keldmarch): one network joining Millford,
; the castle, home's own town and the four ports, laid by the bootstrap
; (src/openworld.c) once home is built into the vault, and kept there with
; the land. tools/roadref.py is the same algorithm in Python, step for step,
; and says why it is this one; tools/checkisle.py compares the two.
;
; Prim's algorithm on the grid. A breadth-first search from everything
; joined so far (Millford at first, then every road and every place joined)
; stops at the first place not yet joined that it meets, which is so the
; nearest, and the way back from there to the network is laid as road;
; again, until no place is left or none can be reached. A road goes only on
; grass, forest, sand and swamp, never on a mountain or water nor through
; another place, and four ways. A landing reached is road itself at the
; end, so the road meets the ship. The way back keeps going the way it was
; going while that is one nearer, so a road runs straight where it can.
;
; A road is one tile (assets/overworld.txt's `code road`, $96): what shape
; it is drawn in -- a straight, a turn, a crossroads, a fork -- is its edges,
; from its neighbours, as any coast is.
;
; The grid is a byte a square: BLOCK for what no road crosses, ROAD, GOAL
; for a place not joined yet, and in the low two bits the search's distance
; mod 3, plus 1 (0: not seen). That is all the way back needs: a neighbour
; one nearer has the number one less, round the cycle, since neighbours in
; a breadth-first search differ by one at most.
;
; The grid and the queue are in $4000-$6FFF, free in the bootstrap once the
; splash picture is unpacked (cfg/c64-owboot.cfg). Only reading home and
; writing the roads into it need the vault (interrupts off); the search
; does not, so the disk icon goes on blinking. A few seconds at most.

        .include "zeropage.inc"
        .include "q4k/tiles.inc"

        .export _ow_roads
        .import vault_enter, vault_leave
        .import _isle_fx, _isle_fy, _isle_port_x, _isle_port_y

GRID    = $4000                 ; a byte a square, 64 by 64
QLO     = $5000                 ; the queue: squares' addresses in GRID,
QHI     = $6000                 ; low and high bytes
HOME    = $D000                 ; home in the vault (vault.s)
T_ROAD  = $96                   ; assets/overworld.txt's `code road`

BLOCK   = $80
ROAD    = $40
GOAL    = $20
JOINED  = BLOCK | 1             ; a place joined: a source, at distance 0

; The square being looked at is `at` (its address in GRID); ptr1 is 64 less,
; so that its neighbours are (ptr1),y with Y one of these.
OFF_N   = 0
OFF_W   = 63
OFF_AT  = 64
OFF_E   = 65
OFF_S   = 128

p       = ptr1                  ; the square's address - 64
qh      = ptr2                  ; the queue's head, in QLO ...
qhh     = sreg                  ; ... and in QHI (the same low byte)
qt      = ptr3                  ; its tail, in QLO ...
qth     = ptr4                  ; ... and in QHI
nv      = tmp1                  ; the search: the next number
want    = tmp1                  ; the way back: the number one nearer
dir     = tmp2                  ; the way back: the way it last went
atlo    = tmp3                  ; the square's address
athi    = tmp4
goal    = regsave               ; the place found, until the way is laid

        .segment "CODE"

; The next number round the cycle, and the one before, by number.
nexttab: .byte 0, 2, 3, 1
prevtab: .byte 0, 3, 1, 2
; North, east, south and west: where in (p),y, and how far p moves.
offs:   .byte OFF_N, OFF_E, OFF_S, OFF_W
movelo: .byte <-64, <1, <64, <-1
movehi: .byte >-64, >1, >64, >-1

; ptr4 = the square (X, Y) in GRID.
square: lda #0
        sta ptr4
        tya
        lsr
        ror ptr4
        lsr
        ror ptr4
        clc
        adc #>GRID
        sta ptr4+1
        txa
        ora ptr4
        sta ptr4
        rts

; The place (X, Y) is A.
mark:   pha
        jsr square
        pla
        ldy #0
        sta (ptr4),y
        rts

; The neighbour (p),y in the search (X is 0): a place not joined is the
; end of it; a square not seen yet gets the next number and goes on the
; queue.
.macro  look off
        .local new, done
        ldy #off
        lda (p),y
        beq new
        and #GOAL
        beq done
        jmp found
new:    lda nv
        sta (p),y
        tya
        clc
        adc p
        sta (qt,x)
        lda p+1
        adc #0
        sta (qth,x)
        inc qt
        lda qt
        sta qth
        bne done
        inc qt+1
        inc qth+1
done:
.endmacro

; void ow_roads(void)
_ow_roads:
        ; The grid from home: 0 where a road may go, BLOCK elsewhere.
        jsr vault_enter
        lda #<HOME
        sta ptr1
        sta ptr2
        lda #>HOME
        sta ptr1+1
        lda #>GRID
        sta ptr2+1
        ldx #16
        ldy #0
@copy:  lda (ptr1),y
        cmp #T_GRASS
        beq @open
        cmp #T_FOREST
        beq @open
        cmp #T_SHORE
        beq @open
        cmp #T_SWAMP
        beq @open
        lda #BLOCK
        bne @put
@open:  lda #0
@put:   sta (ptr2),y
        iny
        bne @copy
        inc ptr1+1
        inc ptr2+1
        dex
        bne @copy
        jsr vault_leave

        ; The places: Millford joined, the rest to be.
        ldx _isle_fx+1          ; the castle
        ldy _isle_fy+1
        lda #BLOCK | GOAL
        jsr mark
        ldx _isle_fx+4          ; home's town
        ldy _isle_fy+4
        lda #BLOCK | GOAL
        jsr mark
        lda #3
        sta tmp4
@port:  ldx tmp4
        ldy _isle_port_y,x
        lda _isle_port_x,x
        tax
        lda #BLOCK | GOAL
        jsr mark
        dec tmp4
        bpl @port
        ldx _isle_fx            ; Millford
        ldy _isle_fy
        lda #JOINED
        jsr mark

; A search: the queue empty, then every place joined and every road on it
; at distance 0, the last search's numbers cleared on the way.
search: lda #<QLO
        sta qh
        sta qhh
        sta qt
        sta qth
        lda #>QLO
        sta qh+1
        sta qt+1
        lda #>QHI
        sta qhh+1
        sta qth+1
        lda #<GRID              ; p walks the grid a page at a time
        sta p
        lda #>GRID
        sta p+1
        ldx #0
@page:  ldy #0
@sq:    lda (p),y
        bmi @block
        and #ROAD
        beq @put                ; (0: not a road, and its number gone)
        ora #1
        sta (p),y
        bne @push               ; (always)
@block: cmp #JOINED
        bne @next
@push:  tya
        sta (qt,x)
        lda p+1
        sta (qth,x)
        inc qt
        lda qt
        sta qth
        bne @next
        inc qt+1
        inc qth+1
        jmp @next
@put:   sta (p),y
@next:  iny
        bne @sq
        inc p+1
        lda p+1
        cmp #>GRID + 16
        bne @page

; Breadth first, from the head of the queue.
bfs_pop:   lda qh
        cmp qt
        bne :+
        lda qh+1
        cmp qt+1
        bne :+
        jmp lay                 ; nothing left to reach: done
:       ldy #0
        lda (qh),y
        sta atlo
        lda (qhh),y
        sta athi
        inc qh
        lda qh
        sta qhh
        bne :+
        inc qh+1
        inc qhh+1
:       lda atlo
        sec
        sbc #OFF_AT
        sta p
        lda athi
        sbc #0
        sta p+1
        ldy #OFF_AT
        lda (p),y
        and #3
        tax
        lda nexttab,x
        sta nv
        ldx #0
        ; North, unless on the top row.
        lda athi
        cmp #>GRID
        bne bfs_n
        lda atlo
        cmp #64
        bcc bfs_e
bfs_n:     look OFF_N
        ; East, unless on the right-hand edge.
bfs_e:     lda atlo
        and #63
        cmp #63
        beq bfs_s
        look OFF_E
        ; South, unless on the bottom row.
bfs_s:     lda athi
        cmp #>GRID + 15
        bne bfs_s2
        lda atlo
        cmp #$C0
        bcs bfs_w
bfs_s2:    look OFF_S
        ; West, unless on the left-hand edge.
bfs_w:     lda atlo
        and #63
        beq bfs_next
        look OFF_W
bfs_next:  jmp bfs_pop

; The place at (p),y is the nearest not joined, seen from the square at
; p + 64. Out of the way back's reach until it is laid, then joined.
found:  lda #BLOCK
        sta (p),y
        tya
        clc
        adc p
        sta goal
        lda p+1
        adc #0
        sta goal+1
        lda #0
        sta dir                 ; north first

; The way back: road, until a square already on the network.
@step:  ldy #OFF_AT
        lda (p),y
        and #ROAD | BLOCK
        bne @laid
        lda (p),y
        ora #ROAD
        sta (p),y
        and #3
        tax
        lda prevtab,x
        sta want
        lda p                   ; where it is, for the edges
        clc
        adc #OFF_AT
        sta atlo
        lda p+1
        adc #0
        sta athi
        ldx dir                 ; the same way, if it can
        jsr try
        bcs @step
        ldx #0                  ; else the first way that is one nearer
@other: cpx dir
        beq :+
        jsr try
        bcs @step
:       inx
        cpx #4
        bne @other
@laid:  ldy #0                  ; (no way back: cannot happen)
        lda #JOINED
        sta (goal),y
        jmp search

; Way X from the square, if it is on the map and one nearer: p moved and
; the carry set; else the carry clear, and X as it was.
try:    txa
        beq @north
        cmp #2
        beq @south
        bcs @west
        lda atlo                ; east
        and #63
        cmp #63
        beq @no
        bne @look
@north: lda athi
        cmp #>GRID
        bne @look
        lda atlo
        cmp #64
        bcc @no
        bcs @look
@south: lda athi
        cmp #>GRID + 15
        bne @look
        lda atlo
        cmp #$C0
        bcs @no
        bcc @look
@west:  lda atlo
        and #63
        beq @no
@look:  ldy offs,x
        lda (p),y
        and #3
        cmp want
        bne @no
        clc
        lda p
        adc movelo,x
        sta p
        lda p+1
        adc movehi,x
        sta p+1
        stx dir
        sec
        rts
@no:    clc
        rts

; Every road into home, and the landings it reached with it, so that a
; road runs down to the ship moored there.
lay:    lda #3
        sta tmp4
@land:  ldx tmp4
        ldy _isle_port_y,x
        lda _isle_port_x,x
        tax
        jsr square
        ldy #0
        lda (ptr4),y
        cmp #JOINED
        bne :+
        lda #ROAD
        sta (ptr4),y
:       dec tmp4
        bpl @land
        jsr vault_enter
        lda #<HOME
        sta ptr1
        sta ptr2
        lda #>HOME
        sta ptr1+1
        lda #>GRID
        sta ptr2+1
        ldx #16
        ldy #0
@sq:    lda (ptr2),y
        and #ROAD
        beq :+
        lda #T_ROAD
        sta (ptr1),y
:       iny
        bne @sq
        inc ptr1+1
        inc ptr2+1
        dex
        bne @sq
        jmp vault_leave
