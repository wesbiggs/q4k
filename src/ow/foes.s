; SPDX-License-Identifier: GPL-3.0-or-later
;
; Open world: a dungeon's monsters, the part of them done for every square
; (src/ow/dungeon.c has the rest, and says what they do). In the dungeon's
; overlay (OVDCODE): in C this came to three times the size, and the slot
; has the room for this and not that.
;
; Squares are the engine's (biased by BIAS, as sm_npc_x/y and hx/hy are).
; Monster i is person i: sm_npc_x/y[i] its square, 0 if it is dead,
; and sm_npc_ox/oy[i] the pixels it is drawn off it while it glides there.
;

        .export _foe_sees, _foe_closer, _foe_glide, _foe_room, _foe_turn
        .export _foe_muster, _foe_who, _foe_spawn, _foe_top, _foe_nat
        .export _tx, _ty, _glide, _pace, _speeds, _pending, _base
        .export k_art, k_colour, k_speed        ; (and roam.s's)
        .import sm_char_at, sm_cx, sm_cy, ow_chcol
        .import _sm_ox, _sm_oy, _sm_front
        .import _foe_art, _sm_npc_n, _rand, _sm_npc_ptr, _sm_npc_col
        .import _dun_up_x, _dun_up_y, _dun_dn_x, _dun_dn_y
        .import _local, _sm_npc_x, _sm_npc_y, _sm_npc_ox, _sm_npc_oy
        .import _hx, _hy, _dirdx, _dirdy, _dir, _moving, _speed
        .import _sm_frame, _frame
        .importzp ptr1, ptr3, tmp1

BIAS    = 32
DUN_W   = 48
DUN_H   = 32
FOES    = 6                     ; OWG_FOES
T_WALL  = $6B
T_MONSTER = $8E                 ; the keeper
SEE_X   = 10                    ; on the screen with the hero: 9 squares to
SEE_Y   = 6                     ; its left, 10 to its right, 5 up and down
FOE_A   = $CE37 + 144         ; the game block's foe_a[] (owgame.h: dungeon.c
FOE_B   = FOE_A + 4 * FOES    ; checks), and foe_b[]
NATIVE  = 16                    ; OWG_NATIVE
WANDERERS = 11
T_FLOOR = $6C
T_BLOOD = $92                   ; assets/dungeon_tiles.txt's code blood
WORLD   = $B000                 ; engine.h's: the area's map, as drawn
APART   = 5                     ; placed no nearer either stairs
THINK_LINE = 190                ; a turn starts no later on the raster: 60
                                ; lines before the interrupt at 250

        .segment "OVDCODE"

; bestiary.c's wanderers[] and natives[], as far as the map needs them, by
; kind (OWG_FOE_KIND: the wanderers', then the natives' from NATIVE): the
; picture (tools/mkfields.py's FOE_ART, a block each in foe_art.s), the
; colour and the speed. Change a monster there and here.
A_RAT = 0
A_GOBLIN = 1
A_WOLF = 2
A_WASP = 3
A_BANDIT = 4
A_SKELETON = 5
A_OGRE = 6
A_WRAITH = 7
A_DRAKE = 8
A_KEEPER = 9
        .segment "RODATA"       ; (OW-DATA, which has the room the slot has
                                ; not)
k_art:  .byte   A_RAT, A_GOBLIN, A_WOLF, A_WASP, A_BANDIT, A_BANDIT
        .byte   A_SKELETON, A_SKELETON, A_OGRE, A_WRAITH, A_DRAKE
        .byte   A_GOBLIN, A_BANDIT, A_OGRE, A_WRAITH, A_RAT, A_WRAITH
        .byte   A_DRAKE, A_KEEPER, A_OGRE, A_KEEPER
k_colour:
        .byte   8, 3, 15, 7, 2, 10, 1, 15, 14, 4, 2      ; TC_*, or their own
        .byte   7, 9, 5, 7, 9, 5, 7, 8, 12, 3
k_speed:
        .byte   13, 10, 15, 22, 10, 10, 8, 8, 7, 12, 17
        .byte   11, 14, 8, 16, 9, 10, 12, 12, 7, 9

        .segment "OVDCODE"

pr:     .res 1                  ; paint_row's row, its map row, columns,
prow:   .res 1
pc0:    .res 1
pc1:    .res 1
pc:     .res 1                  ; and the column it is at
po_x:   .res 1                  ; ... the map's character at its top left,
po_y:   .res 1
po_scr: .res 1                  ; ... the screen's page,
po_col: .res 1                  ; ... and whether to colour it
_tx:    .res 1                  ; where the hero ended their last step
_ty:    .res 1
_glide: .res FOES               ; squares it moved this turn: its pace
_pace:  .res FOES               ; tenths of a square banked
_speeds: .res FOES              ; tenths of a square a turn (the arena's)
_pending: .res 1                ; 1 + the one that got beside the hero
_base:  .res 1                  ; the game block's first monster here
ti:     .res 1                  ; whose turn it is
_foe_top: .res 1                ; wanderers[] there are at this depth
_foe_nat: .res 1                ; NATIVE + this land's natives, or 0: home
fi:     .res 1                  ; the monster
cx:     .res 1                  ; its square, and the one being looked at
cy:     .res 1
ax:     .res 1                  ; how far from (tx, ty) ...
ay:     .res 1
sx:     .res 1                  ; ... and which way, -1, 0 or 1
sy:     .res 1
maj:    .res 1                  ; the line: the longer way, the shorter,
mnr:    .res 1                  ; the step along each, and how far it has
mdx:    .res 1                  ; to go, and the error
mdy:    .res 1
ndx:    .res 1
ndy:    .res 1
cnt:    .res 1
err:    .res 1
dx:     .res 1                  ; the step tried
dy:     .res 1
gx:     .res 1                  ; where the hero is going, or is
gy:     .res 1
nx:     .res 1                  ; the square tried
ny:     .res 1

; What is at square (A, Y), in A: T_WALL off the level. Clobbers ptr1, tmp1.
at:     sec
        sbc     #BIAS
        cmp     #DUN_W
        bcs     @off
        sta     tmp1
        lda     #>_local
        sta     ptr1+1
        tya
        sec
        sbc     #BIAS
        cmp     #DUN_H
        bcs     @off
        jsr     grid
        ldy     #0
        lda     (ptr1),y
        rts
@off:   lda     #T_WALL
        rts
        .assert <_local = 0, lderror, "foes.s: local[] must start a page"

; ptr1 = ptr1+1's page + A * 64 + tmp1: square (tmp1, A) of a map 64 wide
; (local[] or WORLD). Keeps X and Y.
grid:   pha
        and     #3
        lsr     a
        ror     a
        ror     a
        ora     tmp1
        sta     ptr1
        pla
        lsr     a
        lsr     a
        clc
        adc     ptr1+1
        sta     ptr1+1
        rts

; How far (cx, cy) is from (tx, ty), and which way to go: ax, ay, sx, sy.
apart:  ldx     #0
        jsr     @one
        inx
@one:   lda     cx,x
        sec
        sbc     _tx,x
        beq     @here
        bcc     @less
        sta     ax,x            ; past it: back
        lda     #$FF
        sta     sx,x
        rts
@less:  eor     #$FF
        adc     #1              ; (carry clear)
        sta     ax,x
        lda     #1
        sta     sx,x
        rts
@here:  sta     ax,x
        sta     sx,x
        rts

; Monster i (in A) to fi, cx, cy, and how far it is from the hero.
fetch:  sta     fi
        tax
        lda     _sm_npc_x,x
        sta     cx
        lda     _sm_npc_y,x
        sta     cy
        jmp     apart

; unsigned char __fastcall__ foe_sees(unsigned char i) -- whether monster i
; sees (tx, ty): on the screen with it, and no wall on the line between,
; drawn eight ways as Bresenham's.
_foe_sees:
        jsr     fetch
        lda     ax
        cmp     #SEE_X + 1
        bcs     @far
        lda     ay
        cmp     #SEE_Y + 1
        bcc     :+
@far:   jmp     @no
:       ldx     #0              ; the longer way is the major one
        stx     mdx
        stx     mdy
        stx     ndx
        stx     ndy
        lda     ax
        cmp     ay
        bcc     @tall
        sta     maj
        lda     ay
        sta     mnr
        lda     sx
        sta     mdx
        lda     sy
        sta     ndy
        jmp     @go
@tall:  lda     ay
        sta     maj
        lda     ax
        sta     mnr
        lda     sy
        sta     mdy
        lda     sx
        sta     ndx
@go:    lda     maj
        sta     cnt
        lsr     a
        sta     err
@step:  dec     cnt             ; the squares between, not the ends
        beq     @yes
        bmi     @yes            ; (the same square)
        lda     cx
        clc
        adc     mdx
        sta     cx
        lda     cy
        clc
        adc     mdy
        sta     cy
        lda     err
        clc
        adc     mnr
        cmp     maj
        bcc     :+
        sbc     maj
        tax
        lda     cx
        clc
        adc     ndx
        sta     cx
        lda     cy
        clc
        adc     ndy
        sta     cy
        txa
:       sta     err
        lda     cx
        ldy     cy
        jsr     at
        cmp     #T_WALL
        bne     @step
@no:    lda     #0
        tax
        rts
@yes:   lda     #1
        ldx     #0
        rts

; unsigned char __fastcall__ foe_closer(unsigned char i) -- monster i a
; square nearer (tx, ty): straight at it if it can, else along the longer
; way, else the other. 1 if it went (its square changed, and it is drawn
; where it was, to glide on), 0 if it could not, 2 if it is beside the
; hero already: a fight.
_foe_closer:
        jsr     fetch
        lda     ax
        cmp     #2
        bcs     @far
        lda     ay
        cmp     #2
        bcs     @far
        lda     #2
        ldx     #0
        rts
@far:   lda     _hx             ; the square the hero is going to, if they
        ldy     _hy             ; are walking, is taken as well as theirs
        ldx     _moving
        beq     :+
        ldx     _dir
        clc
        adc     _dirdx,x
        pha
        tya
        clc
        adc     _dirdy,x
        tay
        pla
:       sta     gx
        sty     gy
        lda     sx              ; straight at them
        ldy     sy
        beq     @axes
        tax
        beq     @axes
        jsr     try
        bcc     @went
@axes:  lda     ax              ; the longer way first
        cmp     ay
        bcc     @ud
        jsr     @lr
        bcc     @went
        jsr     @ud1
        bcc     @went
        bcs     @stuck
@ud:    jsr     @ud1
        bcc     @went
        jsr     @lr
        bcc     @went
@stuck: lda     #0
        tax
        rts
@went:  lda     #1
        ldx     #0
        rts
@lr:    lda     sx
        beq     @nope
        ldy     #0
        jmp     try
@ud1:   ldy     sy
        beq     @nope
        lda     #0
        jmp     try
@nope:  sec
        rts

; A step of (A, Y) for monster fi from (cx, cy): taken, with carry clear,
; if it is onto floor (not the keeper) that nobody stands on or is going
; to, and on a diagonal not between two corners of wall; else carry set.
try:    sta     dx
        sty     dy
        clc
        adc     cx
        sta     ptr1            ; (the new square, while at() is not needed)
        tya
        clc
        adc     cy
        tay                     ; y
        ldx     ptr1            ; x
        cpx     _hx
        bne     :+
        cpy     _hy
        beq     @no1
:       cpx     gx
        bne     :+
        cpy     gy
        bne     :+
@no1:   jmp     @no
:       stx     nx
        sty     ny
        ldx     #FOES - 1       ; anyone there?
@who:   lda     _sm_npc_x,x
        cmp     nx
        bne     :+
        lda     _sm_npc_y,x
        cmp     ny
        beq     @no
:       dex
        bpl     @who
        lda     nx
        ldy     ny
        jsr     at
        cmp     #T_WALL
        beq     @no
        cmp     #T_MONSTER
        beq     @no
        lda     dx              ; a diagonal: one of its corners open
        beq     @ok
        lda     dy
        beq     @ok
        lda     nx
        ldy     cy
        jsr     at
        cmp     #T_WALL
        bne     @ok
        lda     cx
        ldy     ny
        jsr     at
        cmp     #T_WALL
        beq     @no
@ok:    ldx     fi              ; there, and drawn 16 pixels a square back
        lda     nx
        sta     _sm_npc_x,x
        lda     ny
        sta     _sm_npc_y,x
        lda     dx
        jsr     @back
        adc     _sm_npc_ox,x
        sta     _sm_npc_ox,x
        lda     dy
        jsr     @back
        adc     _sm_npc_oy,x
        sta     _sm_npc_oy,x
        clc
        rts
@no:    sec
        rts
@back:  asl     a               ; -16 * d, with carry clear for the add
        asl     a
        asl     a
        asl     a
        eor     #$FF
        sec
        adc     #0
        clc
        rts

; unsigned char __fastcall__ foe_turn(unsigned char i) -- monster i's turn:
; if it sees the hero, as many squares toward them as its pace has banked,
; at its speed a turn; one that cannot see them banks nothing. The squares
; it went (glide[i]), 0 if none; and one that is beside them is pending.
_foe_turn:
        sta     ti
        tax
        lda     #0
        sta     _glide,x
        lda     _sm_npc_x,x
        beq     @done           ; dead
        txa
        jsr     _foe_sees
        ldx     ti
        tay
        bne     @sees
        sta     _pace,x         ; (0)
        beq     @done
@sees:  lda     _pace,x
        clc
        adc     _speeds,x
        sta     _pace,x
@loop:  ldx     ti
        lda     _pace,x
        cmp     #10
        bcc     @done
        sbc     #10
        sta     _pace,x
        txa
        jsr     _foe_closer
        ldx     ti
        cmp     #1
        bne     @stop
        inc     _glide,x
        bne     @loop           ; (always)
@stop:  cmp     #2
        bne     :+
        lda     _pending
        bne     :+
        inx
        stx     _pending
        dex
:       lda     #0
        sta     _pace,x
@done:  ldx     ti              ; where it went, into the game block
        lda     _glide,x
        beq     @still
        txa
        clc
        adc     _base
        tay                     ; (carry clear: sbc takes one more)
        lda     _sm_npc_x,x     ; the square less BIAS, and the kind's bits
        sbc     #BIAS - 1       ; kept: square + BIAS has bits of its own
        eor     FOE_A,y         ; there, which or-ing into the kind's
        and     #63             ; changed what it was (a bone archer came
        eor     FOE_A,y         ; back a wasp)
        sta     FOE_A,y
        lda     _sm_npc_y,x     ; (carry set)
        sbc     #BIAS
        eor     FOE_B,y
        and     #31
        eor     FOE_B,y
        sta     FOE_B,y
        lda     _glide,x
@still: ldx     #0
        rts

; void foe_muster(void) -- the level's monsters (from base) onto the screen
; as people 0 to 5: where they are, what they look like, how fast they go,
; none gliding; sm_npc_n 0 if none is alive.
_foe_muster:
        lda     #0
        sta     _sm_npc_n
        sta     _pending
        ldx     #FOES - 1
@one:   lda     #0
        sta     _sm_npc_ox,x
        sta     _sm_npc_oy,x
        sta     _pace,x
        sta     _sm_npc_x,x
        sta     _sm_npc_y,x
        txa
        clc
        adc     _base
        tay
        lda     FOE_A,y
        ora     #$3F            ; killed (OWG_FOE_DEAD, its kind all ones)?
        and     FOE_B,y
        cmp     #$E0
        bcc     @live
        jsr     stain
        bcc     @dead           ; (always: stain clears it)
@live:  lda     FOE_A,y
        and     #63
        beq     @dead           ; (none: there was no room for it)
        clc
        adc     #BIAS
        sta     _sm_npc_x,x
        lda     FOE_B,y
        and     #31
        adc     #BIAS
        sta     _sm_npc_y,x
        lda     #FOES
        sta     _sm_npc_n
@dead:  lda     FOE_B,y         ; its kind: b's top three bits, then a's two
                                ; (a dead one's runs past the tables: its
                                ; square is 0, never shown, so what it gets is unseen)
        lsr     a
        lsr     a
        lsr     a
        and     #$1C
        sta     tmp1
        lda     FOE_A,y
        asl     a
        rol     a
        rol     a
        and     #3
        ora     tmp1
        cmp     #NATIVE
        bcc     :+
        sbc     #NATIVE - WANDERERS
:       tay
        lda     k_speed,y
        sta     _speeds,x
        lda     k_colour,y
        sta     _sm_npc_col,x   ; (the multiplexer's: smooth.s)
        lda     k_art,y
        clc
        adc     #<((_foe_art - $4000) / 64)
        sta     _sm_npc_ptr,x
        dex
        bpl     @one
        rts

; void __fastcall__ foe_spawn(unsigned char any) -- the level's monsters
; into the game block (from base), the first time it is seen since the way
; in: none if `any` is 0, else on open floor (not the stairs, a chest or the
; keeper) away from both stairs, as many as 256 tries each find room for.
; Each is one of the first foe_top wanderers[], or over the sea (foe_nat) a
; quarter of the time one of the land's two natives: battle_wander()'s
; choice.
_foe_spawn:
        sta     err             ; (any)
        ldx     #0
@one:   stx     ti
        txa
        clc
        adc     _base
        sta     fi              ; (its index in the block)
        tay
        lda     #0
        sta     FOE_A,y
        sta     FOE_B,y
        lda     err
        beq     @skip
        lda     #0
        sta     cnt
@try:   dec     cnt
        bne     :+
@skip:  jmp     @next           ; no room
:       jsr     _rand
        and     #63
        cmp     #DUN_W
        bcs     @try
        sta     cx
        jsr     _rand
        and     #31
        sta     cy
        clc                     ; (rand() leaves the carry anyhow: without
        adc     #BIAS           ; this, the floor looked at was a row down)
        tay
        lda     cx
        adc     #BIAS
        jsr     at
        cmp     #T_FLOOR
        bne     @try
        lda     _dun_up_x       ; away from the stairs, either way
        ldy     _dun_up_y
        jsr     @apart
        bcc     @try
        lda     _dun_dn_x
        ldy     _dun_dn_y
        jsr     @apart
        bcc     @try
        ldy     _base           ; nobody there yet
@who:   cpy     fi
        beq     @kind
        lda     FOE_A,y
        and     #63
        cmp     cx
        bne     :+
        lda     FOE_B,y
        and     #31
        cmp     cy
        beq     @try
:       iny
        bne     @who
@kind:  lda     _foe_nat
        beq     @wand
        jsr     _rand
        and     #3
        bne     @wand
        jsr     _rand
        and     #1
        ora     _foe_nat
        bne     @put            ; (always)
@wand:  jsr     _rand
        and     #15
        cmp     _foe_top
        bcs     @wand
@put:   sta     tmp1            ; a: x and the kind's low bits; b: y and
        ldy     fi              ; its high ones
        lsr     a
        ror     a
        ror     a
        and     #$C0
        ora     cx
        sta     FOE_A,y
        lda     tmp1
        asl     a
        asl     a
        asl     a
        and     #$E0
        ora     cy
        sta     FOE_B,y
@next:  ldx     ti
        inx
        cpx     #FOES
        beq     :+
        jmp     @one
:       rts
; Carry set if (cx, cy) is APART or more from (A, Y) one way or the other.
@apart: sty     tmp1
        sec
        sbc     cx
        clc
        adc     #APART - 1
        cmp     #2 * APART - 1
        bcs     :+
        lda     tmp1
        sec
        sbc     cy
        clc
        adc     #APART - 1
        cmp     #2 * APART - 1
:       rts

; Where monster Y of the game block was killed, a stain on the map (the
; dungeon's tile set's `blood`, on WORLD only: local[], what is walked on,
; keeps its floor). Keeps X and Y; returns with carry clear.
stain:  lda     FOE_A,y
        and     #63
        sta     tmp1
        lda     #>WORLD
        sta     ptr1+1
        lda     FOE_B,y
        and     #31
        jsr     grid
        sty     fi
        ldy     #0
        lda     #T_BLOOD
        sta     (ptr1),y
        ldy     fi
        rts

; unsigned char __fastcall__ foe_who(unsigned char d) -- the monster on the
; square next to the hero that way (engine.h's DIR_*), or $FF.
_foe_who:
        tax
        lda     _hx
        clc
        adc     _dirdx,x
        sta     cx
        lda     _hy
        clc
        adc     _dirdy,x
        sta     cy
        ldx     #FOES - 1
@one:   lda     _sm_npc_x,x
        cmp     cx
        bne     :+
        lda     _sm_npc_y,x
        cmp     cy
        beq     @it
:       dex
        bpl     @one
@it:    txa
        ldx     #0
        rts

; unsigned char foe_glide(void) -- every monster's picture a frame nearer
; its square, at its pace (the squares it moved, times the hero's pixels a
; frame, so that it gets there as a step of the hero's does). Nonzero if
; any is still on its way.
_foe_glide:
        lda     #0
        sta     tmp1            ; any
        ldx     #FOES - 1
@one:   lda     _glide,x
        bne     :+              ; at least a square's pace: a turn that
        lda     #1              ; moved it nowhere left it still off its
:       ldy     _speed          ; square, and it never got there
        cpy     #2
        bne     :+
        asl     a
:       sta     ptr1            ; pixels this frame
        lda     _sm_npc_ox,x
        jsr     @near
        sta     _sm_npc_ox,x
        lda     _sm_npc_oy,x
        jsr     @near
        sta     _sm_npc_oy,x
        dex
        bpl     @one
        lda     tmp1
        ldx     #0
        rts
@near:  beq     @done           ; there
        inc     tmp1
        cmp     #$80            ; (carry: it is negative)
        bcs     @neg
        sec
        sbc     ptr1
        bcs     @done
        lda     #0
        rts
@neg:   clc
        adc     ptr1
        bcc     @done
        lda     #0
@done:  rts

; unsigned char foe_room(void) -- whether a monster's turn may start: this
; frame's, early enough on the raster to be done before the next (counting
; the lines just after the frame's interrupt, where a quiet frame's loop
; runs, as the whole frame: fog_left).
_foe_room:
        jsr     _fog_left
        cmp     #LAST_LINE - THINK_LINE + 1
        lda     #0
        rol     a               ; (carry: that many lines or more)
        ldx     #0
        rts

; paint_row -- character row pr of the screen whose page is po_scr (0 to
; sm_rows - 1), its columns pc0 to pc1, drawn there and then from the map
; as it is with its top left character at (po_x, po_y); with their colour
; too (as the flip would colour them: ow_chcol) if po_col. What the party
; has just seen (the fog, below), and its neighbours, whose edges it
; changes.
SCRA    = $4000
SCRB    = $4400
paint_row:
        lda     #0              ; pr * 40: * 8, and * 32 on top
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
        sta     ptr1            ; (colour RAM's low byte is the same)
        sta     ptr3
        lda     ptr1+1
        adc     #>$D800
        sta     ptr3+1
        sec
        sbc     #>$D800
        clc
        adc     po_scr
        sta     ptr1+1
        lda     pr
        clc
        adc     po_y
        sta     prow
        ldy     pc0
@col:   sty     pc
        tya
        clc
        adc     po_x
        sta     sm_cx
        lda     prow
        sta     sm_cy
        jsr     sm_char_at      ; (keeps ptr1 and ptr3)
        ldy     pc
        sta     (ptr1),y
        ldx     po_col
        beq     :+
        tax
        lda     ow_chcol,x
        sta     (ptr3),y
:       cpy     pc1             ; (carry set at the last)
        iny
        bcc     @col
        rts

; --- What the party can see (dungeon.c's fog) ------------------------------
;
; The classic game's fov.c, the squares' part: a square seen is drawn
; (WORLD from fog_drawn, the level as built, in WORLD's last rows: below)
; and its bit set in fog_seen and in the level's in OWG_FOG (fog_at: under
; the KERNAL, where a write reaches the RAM), and a box of what is new
; grows round it: one for the squares round the hero, one for each ray and
; one for their room, so that a corridor seen far off is not one box with
; what is beside them, which would have the whole of the box between drawn
; again. Squares are the level's here (0 to DUN_W - 1), not the engine's.

T_SECRETDOOR = $7C
T_DARK  = $93                   ; assets/dungeon_tiles.txt's code dark
FOV_ROW = DUN_W / 8
BOXES   = 6                     ; round the hero, four rays, the room
ROOMBOX = 5

        .export _fog_drawn, _fog_seen, _fog_at, _fog_ox, _fog_oy
        .export _fog_reach, _fog_x0, _fog_y0, _fog_x1, _fog_y1
        .export _fog_near, _fog_rect, _fog_dark, _fog_show, _fog_forget
        .export _fog_left, _fog_lit
        .import _sm_sh_x0, _sm_sh_y0, _sm_sh_x1, _sm_sh_y1, _sm_shores_rect
        .import _sm_rows, _pre_dir, sm_fromreu, _dir, _dirdx, _dirdy
        .importzp ptr2

; The level as built (fog_drawn, DUN_W a row) is in WORLD's rows from
; FOG_ROW to its end, which a level never reaches: build_area() copies the
; level into rows 0 to DUN_H - 1 and clears the rest to the set's offmap
; tile, and fog_dark() fills these rows after that, every time a level is
; built (the stairs, the way in, and back from another program, which
; loads the engine again). What is drawn goes no lower than the view with
; the hero, or L's view, on the level's last row and the text window
; hidden: sm_oy = 2 * hy - HERO_PY / 8 - 1 (engine.c's place_camera), and
; 25 character rows from there, two to a square; a step's builds read no row
; past the camera's at its end, and pre_dir's shift reads only the screen.
; That is VIEW_ROW (37), and sm_shores() works out its edges from the row
; below it too, so FOG_ROW leaves one more: the offmap tile there, as
; before. sm_shores() reads these rows as tiles too (show_area()), so the
; edges in SHORE's rows from VIEW_ROW + 2 on mean nothing; nothing draws
; them. Every other reader in a dungeon (passable(), tile_at(), the
; monsters' `at`, the rays) reads local[], with what is off the level a
; wall.
HERO_PY = 84                    ; (engine.h)
VIEW_ROW = (2 * (DUN_H - 1 + BIAS) - HERO_PY / 8 - 1 + 24) / 2 - BIAS
FOG_ROW = 40
        .assert FOG_ROW >= VIEW_ROW + 2, error, "foes.s: fog_drawn is in view"
        .assert FOG_ROW * 64 + DUN_W * DUN_H <= 64 * 64, error, "foes.s: fog_drawn is past WORLD"
_fog_drawn = WORLD + FOG_ROW * 64

        .segment "OVDBSS"       ; (after the art, in no file)
_fog_seen:  .res FOV_ROW * DUN_H

        .segment "OVDCODE"
_fog_at:  .res 2                ; this level's bits in OWG_FOG
_fog_ox:  .res 1                ; where the hero sees from
_fog_oy:  .res 1
_fog_x0:  .res 1                ; fog_rect's box
_fog_y0:  .res 1
_fog_x1:  .res 1
_fog_y1:  .res 1
_fog_reach: .res 4              ; how far each ray went (east, west, south,
                                ; north), the square that stopped it too
bx0:    .res BOXES              ; the boxes of what is newly seen ($FF:
by0:    .res BOXES              ; none), as sm_shores_rect's x0, y0, x1, y1
bx1:    .res BOXES
by1:    .res BOXES
pst:    .res BOXES              ; the map's character row to draw next of
                                ; each (0: none yet, its edges first),
pcx:    .res BOXES              ; and the column in it (0: its first)
pes:    .res BOXES              ; the square row whose edges are next ($FF:
                                ; none yet)
mc0:    .res 1                  ; a box's columns on the screen
mc1:    .res 1
painted: .res 1                 ; fog_show drew something
lim:    .res 1                  ; the raster lines a row of edges needs
bi:     .res 1                  ; the box show() grows
fx:     .res 1                  ; the square being seen
fy:     .res 1
rect_y: .byte   $FF             ; fog_rect's next row ($FF: none)
rect_t: .res 1                  ; the raster lines a row of it needs
ri:     .res 1                  ; the ray
rfb:    .res 1                  ; its square's byte in fog_seen, and bit
rfm:    .res 1
rdx:    .res 1                  ; the ray's way
rdy:    .res 1
fbit:   .byte   1, 2, 4, 8, 16, 32, 64, 128
rays:   .byte   1, $FF, 0, 0    ; east, west, south, north
        .byte   0, 0, 1, $FF

; Square (fx, fy)'s byte in fog_seen in Y, and its bit in X and A.
; Clobbers tmp1.
bitof:  lda     fy              ; fy * 6 + fx / 8
        asl     a
        sta     tmp1
        asl     a
        adc     tmp1            ; (carry clear: fy < 32)
        sta     tmp1
        lda     fx
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     tmp1
        tay
        lda     fx
        and     #7
        tax
        lda     fbit,x
        tax
        rts

; Square (fx, fy) seen, into box bi if it is new; nothing off the level.
show:   lda     fx
        cmp     #DUN_W
        bcs     @no
        lda     fy
        cmp     #DUN_H
        bcs     @no
        jsr     bitof
        and     _fog_seen,y
        bne     @no             ; seen before
        txa
        ora     _fog_seen,y
        sta     _fog_seen,y
        sta     (ptr2),y        ; (fog_at, under the KERNAL)
        jsr     drawn_at        ; WORLD's square as it is drawn
        ldy     #0
        lda     (ptr1),y
        pha
        lda     fx
        sta     tmp1
        lda     #>WORLD
        sta     ptr1+1
        lda     fy
        jsr     grid
        pla
        sta     (ptr1),y
        ldx     bi              ; the box, which starts again
        lda     #0
        sta     pst,x
        sta     pcx,x
        lda     #$FF
        sta     pes,x
        lda     fx
        cmp     bx0,x
        bcs     :+
        sta     bx0,x
:       cmp     bx1,x
        bcc     :+
        sta     bx1,x
:       lda     fy
        cmp     by0,x
        bcs     :+
        sta     by0,x
:       cmp     by1,x
        bcc     @no
        sta     by1,x
@no:    rts

; void fog_forget(void) -- nothing new to draw (show_area() draws it all).
_fog_forget:
        lda     #$FF
        sta     rect_y
        ldx     #BOXES - 1
:       lda     #$FF
        sta     bx0,x
        sta     by0,x
        lda     #0
        sta     bx1,x
        sta     by1,x
        sta     pst,x
        sta     pcx,x
        dex
        bpl     :-
        rts

; void fog_near(void) -- what the hero sees from (fog_ox, fog_oy) but their
; room: the squares round them, and straight along the corridors to the
; first wall or secret door (which looks like one), which is seen too, into
; the boxes of what is new (with what is still to draw of them); fog_reach
; says how far each ray went.
_fog_near:
        jsr     at_fog
        lda     #0
        sta     bi
        ldx     _fog_oy
        dex
        stx     fy
@r:     ldx     _fog_ox
        dex
        stx     fx
@c:     jsr     show
        inc     fx
        lda     fx
        sec
        sbc     _fog_ox
        cmp     #2
        bne     @c
        inc     fy
        lda     fy
        sec
        sbc     _fog_oy
        cmp     #2
        bne     @r
        ldx     #3              ; the rays: a square's bit and the map's
@ray:   stx     ri              ; byte stepped along, not worked out anew,
        stx     bi              ; since all but a few are seen already
        inc     bi              ; (box 1 + the ray)
        lda     rays,x
        sta     rdx
        lda     rays+4,x
        sta     rdy
        lda     #0
        sta     _fog_reach,x
        lda     _fog_ox
        sta     fx
        lda     _fog_oy
        sta     fy
        jsr     bitof
        sty     rfb
        stx     rfm
        lda     fx
        sta     tmp1
        lda     #>_local
        sta     ptr1+1
        lda     fy
        jsr     grid
        lda     ptr1
        sta     @tile+1
        lda     ptr1+1
        sta     @tile+2
@step:  ldx     ri
        inc     _fog_reach,x
        lda     fx
        clc
        adc     rdx
        sta     fx
        cmp     #DUN_W
        bcs     @end
        lda     fy
        clc
        adc     rdy
        sta     fy
        cmp     #DUN_H
        bcs     @end
        lda     @tile+1         ; the map's byte
        clc
        adc     lsteplo,x
        sta     @tile+1
        lda     @tile+2
        adc     lstephi,x
        sta     @tile+2
        cpx     #2              ; the bit
        bcs     @vert
        txa
        bne     @west
        asl     rfm             ; east
        bne     @bit
        rol     rfm             ; (carry set: 1)
        inc     rfb
        bne     @bit            ; (always)
@west:  lsr     rfm
        bne     @bit
        ror     rfm             ; (carry set: $80)
        dec     rfb
        jmp     @bit
@vert:  lda     rfb             ; a row down or up
        clc
        adc     vstep-2,x
        sta     rfb
@bit:   ldy     rfb
        lda     rfm
        and     _fog_seen,y
        bne     @tile
        jsr     show            ; not seen before
@tile:  lda     $FFFF           ; (the map's byte: patched)
        cmp     #T_WALL
        beq     @end
        cmp     #T_SECRETDOOR
        bne     @step
@end:   ldx     ri
        dex
        bmi     :+
        jmp     @ray
:       rts

lsteplo: .byte  <1, <-1, <64, <-64      ; the map's step east, west, south,
lstephi: .byte  >1, >-1, >64, >-64      ; north
vstep:  .byte   FOV_ROW, <-FOV_ROW      ; and the bits', south and north

; ptr1 = fog_drawn + fy * DUN_W + fx. Clobbers tmp1.
        .assert DUN_W = 48, error, "foes.s: drawn_at multiplies by 48"
drawn_at:
        lda     #0
        sta     ptr1+1
        lda     fy              ; * 16
        asl     a
        asl     a
        asl     a
        asl     a
        rol     ptr1+1
        sta     tmp1
        ldx     ptr1+1
        asl     a               ; * 32
        rol     ptr1+1
        clc
        adc     tmp1            ; * 48
        sta     ptr1
        txa
        adc     ptr1+1
        sta     ptr1+1
        lda     ptr1
        clc
        adc     #<_fog_drawn
        sta     ptr1
        lda     ptr1+1
        adc     #>_fog_drawn
        sta     ptr1+1
        lda     ptr1
        clc
        adc     fx
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       rts

; ptr2 from fog_at, for show.
at_fog: lda     _fog_at
        sta     ptr2
        lda     _fog_at+1
        sta     ptr2+1
        rts

; void fog_rect(void) -- the squares fog_x0-x1 by fog_y0-y1 to be seen (a
; room, walls and all, as the hero comes into it), into a box of their own:
; fog_lit does it, a row at a time.
_fog_rect:
        lda     _fog_y0
        sta     rect_y
        lda     _fog_x1         ; a row's time: four lines a square, and four
        sec
        sbc     _fog_x0
        asl     a
        asl     a
        adc     #8              ; (carry clear)
        sta     rect_t
        rts

; void __fastcall__ fog_lit(unsigned char all) -- fog_rect's rows seen, while
; the frame has the room for each (all of them if all: the screen is off).
_fog_lit:
        tax
        jsr     at_fog
        lda     #ROOMBOX
        sta     bi
@r:     lda     rect_y
        cmp     #$FF
        beq     @done
        cmp     _fog_y1
        beq     :+
        bcs     @end
:       sta     fy
        txa
        bne     :+
        jsr     _fog_left
        cmp     rect_t
        bcc     @done           ; no room: the rest next time
:       txa
        pha
        lda     _fog_x0
        sta     fx
@c:     jsr     show
        lda     fx
        inc     fx
        cmp     _fog_x1
        bcc     @c
        pla
        tax
        inc     rect_y
        bne     @r              ; (always)
@end:   lda     #$FF
        sta     rect_y
@done:  rts

; void fog_dark(void) -- a level just built into WORLD: its rows into
; fog_drawn, and every square fog_seen has not seen dark in WORLD.
_fog_dark:
        lda     #0
        sta     fx
        sta     fy
@copy:  jsr     drawn_at        ; a row: WORLD's into fog_drawn's
        lda     ptr1
        sta     ptr2
        lda     ptr1+1
        sta     ptr2+1
        lda     #0
        sta     tmp1
        lda     #>WORLD
        sta     ptr1+1
        lda     fy
        jsr     grid
        ldy     #DUN_W - 1
:       lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     :-
        inc     fy
        lda     fy
        cmp     #DUN_H
        bne     @copy
        lda     #0
        sta     fy
@r:     lda     #0
        sta     fx
@c:     jsr     bitof
        and     _fog_seen,y
        bne     @n
        lda     fx
        sta     tmp1
        lda     #>WORLD
        sta     ptr1+1
        lda     fy
        jsr     grid
        ldy     #0
        lda     #T_DARK
        sta     (ptr1),y
@n:     inc     fx
        lda     fx
        cmp     #DUN_W
        bne     @c
        inc     fy
        lda     fy
        cmp     #DUN_H
        bne     @r
        rts

; void __fastcall__ fog_show(unsigned char back) -- what fog_near and
; fog_rect newly saw, drawn while the frame has the room, a row of
; characters at a time; what has not, the next time. A box's edges first
; (sm_shores_rect: its squares and those round them), then its characters
; that can change: its squares' and the one round them, on whichever
; quarter's corner it is. Rows off the screen are not drawn: the scrolling
; draws them from the map as they come on. Not back: onto the screen in
; front, with their colour; the back screen is then no longer the front
; shifted, if it had been made ready for the next step (pre_dir). Back:
; onto the back screen, past its shift for a flip still to come (the map a
; character on, the way the step goes), which colours it from the screen
; (as it always does without an REU; with one, sm_fromreu 0 says so).
_fog_show:
        tax
        beq     @front
        ldy     _dir            ; the back screen: the map a character on
        lda     _sm_ox
        clc
        adc     _dirdx,y
        sta     po_x
        lda     _sm_oy
        clc
        adc     _dirdy,y
        sta     po_y
        lda     _sm_front
        eor     #1
        ldy     #0              ; no colour
        beq     @set            ; (always)
@front: lda     _sm_ox
        sta     po_x
        lda     _sm_oy
        sta     po_y
        lda     _sm_front
        ldy     #1
@set:   sty     po_col
        tax                     ; the page
        lda     #>SCRA
        cpx     #0
        beq     :+
        lda     #>SCRB
:       sta     po_scr
        lda     #0
        sta     painted
        lda     #BOXES - 1
        sta     bi
@box:   ldx     bi
        lda     bx0,x
        cmp     #$FF
        beq     @next           ; nothing new
        cpx     #ROOMBOX
        bne     :+
        lda     rect_y
        cmp     #$FF
        bne     @next           ; the room not all lit yet
:
        lda     pst,x
        bne     :+
        jsr     edges           ; (a box just grown: its edges first)
        bcs     @out            ; no room left this frame
:       jsr     rows
        bcs     @out
@next:  dec     bi
        bpl     @box
@out:   lda     painted
        beq     @done
        lda     po_col
        beq     :+
        lda     #$FF            ; the front: the back is not it shifted
        sta     _pre_dir
        rts
:       sta     sm_fromreu      ; the back: the flip colours it (0)
@done:  rts

; Box bi's edges (sm_shores_rect): its squares and a square round them, on
; the level, a row of squares at a time from pes, while the frame has the
; room (a row is about two and a half raster lines a square: three are
; allowed); carry set if it ran out. Then its first row to draw, the
; character row above its top squares'.
edges:  lda     bx0,x           ; the columns
        beq     :+
        sec
        sbc     #1
:       sta     _sm_sh_x0
        lda     bx1,x
        cmp     #DUN_W - 1
        bcs     :+
        adc     #1
:       sta     _sm_sh_x1
        sec                     ; the time a row takes: 3 * (w + 2) + 4
        sbc     _sm_sh_x0
        adc     #2              ; (carry set: w + 2)
        sta     tmp1
        asl     a
        adc     tmp1
        adc     #4
        sta     lim             ; (the lines a row needs)
        lda     pes,x
        cmp     #$FF
        bne     @row
        lda     by0,x           ; the first: the row above its top
        beq     :+
        sec
        sbc     #1
:       sta     pes,x
@row:   ldx     bi
        lda     by1,x           ; past the last, the row below its bottom?
        cmp     #DUN_H - 1
        bcs     :+
        adc     #1
:       cmp     pes,x
        bcc     @done
        jsr     _fog_left       ; the room for it, this frame?
        cmp     lim
        bcc     @full
        ldx     bi
        lda     pes,x
        sta     _sm_sh_y0
        sta     _sm_sh_y1
        jsr     _sm_shores_rect
        ldx     bi
        inc     pes,x
        bne     @row            ; (always)
@done:  lda     #$FF
        sta     pes,x
        lda     by0,x           ; drawn from the row above its top squares'
        clc
        adc     #BIAS
        asl     a
        sec
        sbc     #1
        sta     pst,x
        clc
        rts
@full:  sec
        rts

; Box bi's rows from pst on (and its column pcx), while the frame has the
; room, as many characters at a time as it has: carry set if it ran out
; (the rest for next time), clear when the box is done. A character is
; about six raster lines (sm_char_at's, measured with interrupts off):
; eight are allowed, up to line 236.
CHAR_LINES = 8
LAST_LINE = 236
rows:   ldx     bi              ; the columns on the screen: the squares'
        lda     bx0,x           ; and one more either side, clipped
        clc
        adc     #BIAS
        asl     a
        sec
        sbc     #1
        sec
        sbc     po_x
        bcs     :+
        lda     #0              ; (left of the screen)
:       cmp     #40
        bcc     :+
        jmp     @done           ; right of it: none of it is on
:       sta     mc0
        lda     bx1,x
        clc
        adc     #BIAS
        asl     a
        adc     #2              ; (carry clear)
        sec
        sbc     po_x
        bcs     :+
        jmp     @done           ; left of it
:       cmp     #40
        bcc     :+
        lda     #39
:       sta     mc1
@row:   ldx     bi
        lda     by1,x           ; past the last row (the one below its
        clc                     ; bottom squares')?
        adc     #BIAS
        asl     a
        adc     #2              ; (carry clear)
        cmp     pst,x
        bcc     @done
        lda     pst,x
        sec
        sbc     po_y
        bcc     @next           ; above the screen
        cmp     _sm_rows
        bcs     @next           ; below the map's rows
        sta     pr
        lda     pcx,x           ; where in the row: on the screen
        sec
        sbc     po_x
        bcc     :+              ; (0, or left of it: the first)
        cmp     mc0
        bcs     :++
:       lda     mc0
:       sta     pc0
@chunk: jsr     _fog_left       ; the room for how many, this frame?
        lsr     a               ; / CHAR_LINES
        lsr     a
        lsr     a
        beq     @full
        clc                     ; that many from pc0, at most to mc1
        adc     pc0
        sec
        sbc     #1
        cmp     mc1
        bcc     :+
        lda     mc1
:       sta     pc1
        jsr     paint_row
        lda     #1
        sta     painted
        lda     pc1
        cmp     mc1
        bcs     @next           ; the row is done
        adc     #1              ; (carry clear)
        sta     pc0
        bne     @chunk          ; (always)
@next:  ldx     bi
        lda     #0
        sta     pcx,x
        inc     pst,x
        jmp     @row
@done:  ldx     bi
        lda     #$FF
        sta     bx0,x
        lda     #0
        sta     pst,x
        sta     pcx,x
        clc
        rts
@full:  ldx     bi              ; the column to go on from, the map's
        lda     pc0
        clc
        adc     po_x
        sta     pcx,x
        sec
        rts

; unsigned char fog_left(void) -- the raster lines left in this frame, the
; one the main loop is handling, before LAST_LINE: all of them just after
; the frame's interrupt (line 250, where an idle frame's loop runs), and
; none once the next has come. Keeps X and Y.
_fog_left:
        lda     _sm_frame
        cmp     _frame
        bne     @none
        lda     $D011
        bmi     @all            ; past line 255: the frame has only begun
        lda     $D012
        cmp     #250
        bcs     @all
        cmp     #LAST_LINE
        bcs     @none
        eor     #$FF            ; LAST_LINE - the line
        sec
        adc     #LAST_LINE
        rts
@all:   lda     #LAST_LINE
        rts
@none:  lda     #0
        rts
