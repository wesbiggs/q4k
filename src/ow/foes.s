;
; Open world: a dungeon's monsters, the part of them done for every square
; (src/ow/dungeon.c has the rest, and says what they do). In the dungeon's
; overlay (OVDCODE): in C this came to three times the size, and the slot
; has the room for this and not that.
;
; Squares are the engine's (biased by BIAS, as sm_npc_x/y and hx/hy are).
; Monster i is sprite i + 1: sm_npc_x/y[i] its square, 0 if it is dead,
; and sm_npc_ox/oy[i] the pixels it is drawn off it while it glides there.
;

        .export _foe_sees, _foe_closer, _foe_glide, _foe_room, _foe_turn
        .export _foe_muster, _foe_who, _foe_spawn, _foe_top, _foe_nat
        .export _tx, _ty, _glide, _pace, _speeds, _pending, _base
        .import _foe_art, _sm_npc_n, _rand
        .import _dun_up_x, _dun_up_y, _dun_dn_x, _dun_dn_y
        .import _local, _sm_npc_x, _sm_npc_y, _sm_npc_ox, _sm_npc_oy
        .import _hx, _hy, _dirdx, _dirdy, _dir, _moving, _speed
        .import _sm_frame, _frame
        .importzp ptr1, tmp1

BIAS    = 32
DUN_W   = 48
DUN_H   = 32
FOES    = 6                     ; OWG_FOES
T_WALL  = $6B
T_MONSTER = $8E                 ; the keeper
SEE_X   = 10                    ; on the screen with the hero: 9 squares to
SEE_Y   = 6                     ; its left, 10 to its right, 5 up and down
FOE_A   = $CE40 + 144         ; the game block's foe_a[] (owgame.h: dungeon.c
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
        tay
        lda     FOE_A,y
        and     #$C0
        ora     _sm_npc_x,x
        sec
        sbc     #BIAS
        sta     FOE_A,y
        lda     FOE_B,y
        and     #$E0
        ora     _sm_npc_y,x
        sec
        sbc     #BIAS
        sta     FOE_B,y
        lda     _glide,x
@still: ldx     #0
        rts

; void foe_muster(void) -- the level's monsters (from base) onto the screen
; as sprites 1 to 6: where they are, what they look like, how fast they go,
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
                                ; sprite is off, so what it gets is unseen)
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
        sta     $D028,x
        lda     k_art,y
        clc
        adc     #<((_foe_art - $4000) / 64)
        sta     $43F9,x         ; both screens' sprite pointers
        sta     $47F9,x
        dex
        bpl     @one
        lda     #$FF
        sta     $D01C           ; multicolour, as the hero
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
        adc     #BIAS           ; (carry clear)
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
        ldy     _speed
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
; frame's, early enough on the raster to be done before the next.
_foe_room:
        lda     _sm_frame
        cmp     _frame
        bne     @no
        lda     $D011
        bmi     @no             ; past line 255
        lda     $D012
        cmp     #THINK_LINE
        bcs     @no
        lda     #1
        ldx     #0
        rts
@no:    lda     #0
        tax
        rts
