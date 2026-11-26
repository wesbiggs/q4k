; SPDX-License-Identifier: GPL-3.0-or-later
;
; Open world: the monsters that roam the overworld. They turn up at the edge
; of the screen as the hero walks, on what the walk has just brought into
; view, and come at the hero the way a dungeon's monsters do (foes.s), a
; turn for every step; one that gets beside them is a fight.
;
; Where it runs: under the KERNAL, in two halves, each a file of its own,
; which the world's overlay loads (roamcall.s) when it finds one is not
; there: ROAM (ROAMCODE, and the records, ROAMBSS) at $FC00, over the
; dungeon's fog, which means nothing on the overworld (ow-cpu.rl, ow-reu.rl:
; the first time out, and after a dungeon, whose way in clears the fog);
; ROAMHI (ROAMHI, with the pictures) at $EAC0, the top of the tune's room,
; past what a tune may have (ow-cpu.rh, ow-reu.rh: the first time out). The overlay calls rm_run with the machine in
; all-RAM (vault_enter()), interrupts off, with what it is to do in A:
;
;   R_ENTER   back on the overworld, the screen off: the monsters kept if
;             this is the square a fight with one of them started on, else
;             none, and then a roll on each of the screen's four edges (the
;             R_SHOW after it shows them)
;   R_CLEAR   into a town or a dungeon: none
;   R_ROLL    the end of a step that can meet one (as the old roll had it:
;             not onto a place, nor in a ship off the deep sea): the turn
;             starts, and a roll on the edge the step brought into view
;   R_MOVE    the end of any other step: only the turn
;   R_SHOW    the people being set up again (show_ships()): the monsters after
;             the king's ship
;   R_IDLE    every frame: a picture unpacked, or the monsters a frame nearer
;             their squares and one more square of their turn; and the fight,
;             once the one that got beside the hero is there
;   R_CAN+d   whether the hero may step that way: not while one is coming
;             for a fight; and a monster on that square is a fight
;
; It answers in X and Y: X's bit 7 to have the people placed again (sm_npcs),
; bit 6 if there are no monsters now, its low six bits 1 + a fight's OWF_*
; (Y its arg) or 0; Y the answer, else.
;
; A roll is the old one (world.c had it), out of 200 by the ground, but for
; the square it rolls for, not the hero's: one square in from the edge that
; came into view, since the edge itself may not have been looked at yet and
; still be dark. On foot only land rolls, and in a ship only the deep sea.
; With OWG_CALM (the tests'), none, unless enc_always is 4, which also makes
; every roll one; and enc_always 1 (or 3, in the deep sea) is still the old
; fight on every roll, at once.
;
; Squares are the engine's (hx, hy: the world's, & 127); the window
; (WORLD) has them & 63. A monster is a record here, and its picture is one
; of the sprite blocks of the classes the party's leader is not (the
; leader's is the hero's), unpacked from roam_art.s: it is a person
; (sm_npc_*) after the king's ship once its picture is there.

        .export rm_sig, rm_sig2, rm_run
        .include "zeropage.inc"
        .import roam_art, k_art, k_colour, k_speed
        .import _hx, _hy, _moving, _dir, _dirdx, _dirdy, _speed, _boat
        .import _land, _enc_always, _looking, _rand
        .import _sm_npc_n, _sm_npc_x, _sm_npc_y, _sm_npc_ox, _sm_npc_oy
        .import _sm_npc_ptr, _sm_npc_col

SLOTS     = 4                   ; the classes' blocks the leader leaves
WORLD     = $B000               ; engine.h's: the 64 by 64 window
SPRITES   = $5000               ; block 64: smooth.s's SPRITES
G_CLS     = $CE37 + 13          ; the leader's class (world.c checks these)
G_LEVEL   = $CE37 + 87          ; party_level
G_QUIET   = $CE37 + 138
OWG_CALM  = 4
IBIOME    = $02A7 + 30          ; OW->ibiome[] (q4k/ow.h)
ISLE_HOME = 4
NATIVE    = 16                  ; OWG_NATIVE
WANDERERS = 11
SEA_KINDS = 5
SEA_FLAG  = $40                 ; r_k: $80 | a land's kind, $C0 | the sea's
T_FOREST  = $61
T_SHORE   = $65
T_SWAMP   = $66
T_FLOOR   = $6C
T_DEEP    = $95
OWF_WANDER = 0                  ; (q4k/ow.h)
OWF_SEA   = 5
OWF_ROAM  = 6
THINK_LINE = 190                ; foes.s's: no turn later on the raster

; Scratch, in cc65's (none of it kept across a call: the caller is C)
cx        = ptr1
cy        = ptr1+1
ax        = ptr2
ay        = ptr2+1
sx        = ptr3
sy        = ptr3+1
tx        = ptr4
ty        = ptr4+1
nx        = tmp1
ny        = tmp2
t         = tmp3
fi        = tmp4
gx        = sreg
gy        = sreg+1

R_ENTER = 0
R_CLEAR = 1
R_ROLL  = 2
R_MOVE  = 3
R_SHOW  = 4
R_IDLE  = 5
R_CAN   = 8

        .segment "ROAMHEAD"
; First, where a dungeon's fog clears it: that this is here (roamcall.s).
rm_sig: .byte   $A5, $5A, $C3
        .segment "ROAMHIHEAD"
; And the half in the tune's room, which nothing but a load clears.
rm_sig2: .byte  $C3, $5A, $A5

        .segment "ROAMCODE"

rm_run: ldx     #0
        stx     rflag
        stx     rarg
        jsr     @go
        ldx     #SLOTS - 1      ; none left: bit 6, and R_IDLE need not be
@live:  lda     r_k,x           ; called till something turns up
        bne     @out
        dex
        bpl     @live
        lda     rflag
        ora     #$40
        sta     rflag
@out:   ldx     rflag
        ldy     rarg
        rts
@go:    cmp     #R_CAN
        bcs     can
        asl     a
        tax
        lda     ops+1,x
        pha
        lda     ops,x
        pha
        rts
ops:    .word   enter-1, clear-1, roll-1, house-1, show-1, idle-1

; --- R_CAN + d ------------------------------------------------------------

can:    ldy     pending
        bne     @no             ; (one is coming: rarg 0)
        and     #7
        tax
        lda     _hx
        clc
        adc     _dirdx,x
        and     #127
        pha
        lda     _hy
        clc
        adc     _dirdy,x
        and     #127
        tay
        pla
        jsr     occ
        bcs     fight           ; (X: who)
        inc     rarg            ; free of them
@no:    rts

; A fight with monster X: it goes, the rest stay for when the party is
; back on this square (R_ENTER). Land's: OWF_ROAM, the kind and the ground
; underfoot (ow_fight.c's roam_ground[]); the sea's: OWF_SEA and which.
fight:  lda     #0
        sta     pending
        lda     r_k,x
        pha
        lda     #0
        sta     r_k,x
        lda     _hx
        sta     bx
        lda     _hy
        sta     by
        lda     #1
        sta     back
        lda     _hx
        ldy     _hy
        jsr     tileat
        ldx     #3
@g:     cmp     ground,x
        beq     :+
        dex
        bpl     @g
:       inx                     ; the ground, 0 to 4
        txa
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     rarg
        pla
        tax
        and     #SEA_FLAG
        bne     @sea
        txa
        and     #31
        ora     rarg
        sta     rarg
        lda     #OWF_ROAM + 1
        sta     rflag
        rts
@sea:   txa
        and     #7
        sta     rarg
        lda     #OWF_SEA + 1
        sta     rflag
        rts
ground: .byte   T_FOREST, T_SWAMP, T_SHORE, T_FLOOR

; --- R_CLEAR: none --------------------------------------------------------

clear:  lda     #0
        ldx     #SLOTS - 1
:       sta     r_k,x
        dex
        bpl     :-
        sta     pending
        sta     back
        lda     #SLOTS
        sta     think
        rts

; --- R_ENTER --------------------------------------------------------------

enter:  jsr     mode
        sta     inship
        lda     back
        beq     @new
        lda     _hx
        cmp     bx
        bne     @new
        lda     _hy
        cmp     by
        bne     @new
        ldx     #SLOTS - 1      ; the rest, still there: their pictures
@k:     lda     r_art,x         ; again (the engine's load put the classes'
        ora     #$80            ; back), and nobody on the way anywhere
        sta     r_art,x
        lda     #0
        sta     r_ox,x
        sta     r_oy,x
        dex
        bpl     @k
        lda     #0
        sta     back
        sta     pending
        lda     #SLOTS
        sta     think
        bne     @done           ; (always)
@new:   jsr     clear
        jsr     calm
        bcs     @done
        lda     #3
        sta     e
@e:     lda     e
        jsr     edge
        dec     e
        bpl     @e
@done:  inc     changed         ; (shown by the R_SHOW after it)
        rts

; --- R_SHOW: the monsters after the people there are --------------------

show:   lda     _sm_npc_n
        sta     base
        ; (and on)

; The monsters whose pictures are there, as people from base.
publish:
        ldy     base
        ldx     #0
@one:   lda     r_k,x
        beq     @n
        lda     r_art,x
        bmi     @n
        lda     r_x,x
        sta     _sm_npc_x,y
        lda     r_y,x
        sta     _sm_npc_y,y
        lda     r_ox,x
        sta     _sm_npc_ox,y
        lda     r_oy,x
        sta     _sm_npc_oy,y
        lda     r_col,x
        sta     _sm_npc_col,y
        jsr     blockof
        clc
        adc     #64
        sta     _sm_npc_ptr,y
        iny
@n:     inx
        cpx     #SLOTS
        bne     @one
        sty     _sm_npc_n
        rts

; --- R_ROLL, R_MOVE: the end of a step ------------------------------------

; Aboard or ashore, those of the other kind gone; those off the screen gone
; too; and a new turn, each banking its speed (one coming for a fight a
; square's, to follow the hero to where they are now).
house:  jsr     mode
        cmp     inship
        sta     inship
        beq     :+
        jsr     clear
:       ldx     #SLOTS - 1
@one:   lda     r_k,x
        beq     @n
        lda     r_x,x
        sec
        sbc     _hx
        clc
        adc     #9              ; 9 to the hero's left, 10 to the right
        and     #127
        cmp     #20
        bcs     @drop
        lda     r_y,x
        sec
        sbc     _hy
        clc
        adc     #5              ; and 5 up and down
        and     #127
        cmp     #11
        bcs     @drop
        lda     #0
        sta     r_glide,x
        lda     r_pace,x
        clc
        adc     r_spd,x
        sta     r_pace,x
        bne     @n              ; (always)
@drop:  lda     #0
        sta     r_k,x
        inc     changed
@n:     dex
        bpl     @one
        ldx     pending
        beq     :+
        lda     #10
        sta     r_pace-1,x
:       lda     #0
        sta     think
        rts

roll:   jsr     house
        lda     _hx             ; the old roll, for the tests: a fight at
        ldy     _hy             ; once on every roll
        jsr     tileat
        sta     t
        ldx     _enc_always
        cpx     #1
        beq     @force
        cpx     #3
        bne     @edge
        cmp     #T_DEEP
        bne     @edge
@force: jsr     tidx
        bmi     @edge
        lda     odds,x
        beq     @edge
        jsr     _rand           ; (the dice it had: rand() % 200, once)
        lda     t
        sta     rarg
        ldx     #OWF_WANDER + 1
        cmp     #T_DEEP
        bne     :+
        ldx     #OWF_SEA + 1
:       stx     rflag
        rts
@edge:  jsr     calm
        bcs     @rts
        ldx     _dir            ; the edge the step brings in: a diagonal's
        lda     _dirdy,x        ; either
        sta     sy
        lda     _dirdx,x
        sta     sx
        beq     @dy
        lda     sy
        beq     @dx
        jsr     _rand
        lsr     a
        bcc     @dy
@dx:    lda     sx              ; right 0, left 1
        asl     a
        lda     #0
        rol     a
        jmp     edge
@dy:    lda     sy              ; down 2, up 3
        asl     a
        lda     #1
        rol     a
        jmp     edge
@rts:   rts

; Carry set if nothing may turn up: OWG_CALM's, unless enc_always is 4.
calm:   lda     G_QUIET
        and     #OWG_CALM
        beq     :+
        lda     _enc_always
        cmp     #4
        beq     :+
        sec
        rts
:       clc
        rts

; In a ship (SEA_FLAG) or not (0), in A.
mode:   lda     _boat
        beq     :+
        lda     #SEA_FLAG
:       rts

        .segment "ROAMHI"

; A roll for a square on edge A of the screen, one in: 0 right, 1 left,
; 2 down, 3 up, along it at random.
edge:   tax
        lda     _hx
        clc
        adc     edge_x,x
        sta     cx
        lda     _hy
        clc
        adc     edge_y,x
        sta     cy
        cpx     #2
        bcs     @row
@col:   jsr     _rand           ; 5 up to 5 down
        and     #15
        cmp     #11
        bcs     @col
        sbc     #4              ; (carry clear: 5)
        clc
        adc     cy
        sta     cy
        jmp     spawn
@row:   jsr     _rand           ; 9 left to 10 right
        and     #31
        cmp     #20
        bcs     @row
        sbc     #8              ; (carry clear: 9)
        clc
        adc     cx
        sta     cx
        ; (and on)

; A monster at (cx, cy), at the ground's odds, if there is room for one:
; a free record, nobody there, and ground of the kind (land, or the deep
; sea in a ship).
spawn:  lda     cx
        and     #127
        sta     cx
        lda     cy
        and     #127
        sta     cy
        ldx     #SLOTS - 1
@free:  lda     r_k,x
        beq     @got
        dex
        bpl     @free
@no:    rts
@got:   stx     fi
        lda     cx
        ldy     cy
        jsr     occ
        bcs     @no
        lda     cx
        ldy     cy
        jsr     tileat
        jsr     tidx
        bmi     @no
        lda     inship
        beq     @land
        txa
        bne     @no
        beq     @odds
@land:  txa
        beq     @no
@odds:  lda     odds,x
        beq     @no
        sta     t
        lda     _enc_always     ; (4, the tests': every roll one)
        cmp     #4
        beq     @hit
@roll:  jsr     _rand
        cmp     #200
        bcs     @roll
        cmp     t
        bcs     @no
@hit:
        lda     G_LEVEL
        sta     t
        lda     inship
        bne     @sea
        ldx     #0              ; the wanderers by the party's level, as
@top:   lda     t               ; ow_fight.c picks them, or a land's own
        cmp     w_from,x
        bcc     :+
        inx
        cpx     #WANDERERS
        bne     @top
:       stx     top
        lda     _land
        cmp     #ISLE_HOME
        beq     @wand
        jsr     _rand
        and     #3
        bne     @wand
        ldx     _land
        lda     IBIOME,x
        asl     a
        adc     #NATIVE
        sta     t
        jsr     _rand
        and     #1
        ora     t
        bne     @kind           ; (always)
@wand:  jsr     _rand
        and     #15
        cmp     top
        bcs     @wand
@kind:  sta     t               ; its picture, colour and speed (foes.s's)
        tay
        cpy     #NATIVE
        bcc     :+
        tya
        sbc     #NATIVE - WANDERERS
        tay
:       lda     k_art,y
        pha
        lda     k_colour,y
        pha
        lda     k_speed,y
        pha
        lda     t
        ora     #$80
        bne     @put            ; (always)
@sea:   ldx     #0              ; what comes up out of the sea, likewise
@stop:  lda     t
        cmp     sea_from,x
        bcc     :+
        inx
        cpx     #SEA_KINDS
        bne     @stop
:       stx     top
@pick:  jsr     _rand
        and     #7
        cmp     top
        bcs     @pick
        tay
        lda     sea_art,y
        pha
        lda     sea_col,y
        pha
        lda     sea_spd,y
        pha
        tya
        ora     #$80 | SEA_FLAG
@put:   ldx     fi
        sta     r_k,x
        pla
        sta     r_spd,x
        pla
        sta     r_col,x
        pla
        ora     #$80            ; (its picture still to unpack)
        sta     r_art,x
        lda     cx
        sta     r_x,x
        lda     cy
        sta     r_y,x
        lda     #0
        sta     r_ox,x
        sta     r_oy,x
        sta     r_pace,x
        sta     r_glide,x
        inc     changed
        rts

        .segment "ROAMCODE"

; --- R_IDLE: every frame ----------------------------------------------------

idle:   lda     _looking
        bne     @rts            ; (L: nothing moves)
        lda     _hx
        sta     tx
        lda     _hy
        sta     ty
        ldx     #SLOTS - 1      ; a picture to unpack: that, this frame
@u:     lda     r_k,x
        beq     :+
        lda     r_art,x
        bmi     @unpack
:       dex
        bpl     @u
        jsr     glide
        jsr     room
        bcs     :+
        jsr     turn
:       ldx     pending         ; one beside the hero, there, and the hero
        beq     @pub            ; still: the fight
        lda     anyg
        ora     _moving
        bne     @pub
        dex
        jmp     fight
@unpack:
        jsr     unpack
        inc     changed
@pub:   jsr     publish
        lda     _moving         ; (walking, the engine places them)
        bne     @rts
        lda     anyg
        ora     changed
        beq     @rts
        lda     #0
        sta     changed
        lda     #$80
        sta     rflag
@rts:   rts

; Every monster a frame nearer its square: the squares it went this turn
; times the hero's pixels a frame (foes.s's foe_glide). anyg if any is not
; there yet.
glide:  lda     #0
        sta     anyg
        ldx     #SLOTS - 1
@one:   lda     r_k,x
        beq     @n
        lda     r_glide,x
        bne     :+
        lda     #1
:       ldy     _speed
        cpy     #2
        bne     :+
        asl     a
:       sta     px
        lda     r_ox,x
        jsr     @near
        sta     r_ox,x
        lda     r_oy,x
        jsr     @near
        sta     r_oy,x
@n:     dex
        bpl     @one
        rts
@near:  beq     @done
        inc     anyg
        cmp     #$80
        bcs     @neg
        sec
        sbc     px
        bcs     @done
        lda     #0
        rts
@neg:   clc
        adc     px
        bcc     @done
        lda     #0
@done:  rts

; Carry clear if there is time for a square of a turn before the frame's
; interrupt, at line 250 (as foes.s's foe_room has it).
room:   lda     #$35            ; (the I/O for a moment, the KERNAL still out)
        sta     $01
        lda     $D011
        ldx     $D012
        ldy     #$34
        sty     $01
        asl     a
        bcs     @yes            ; (256 and on: after it)
        cpx     #THINK_LINE
        bcc     @yes
        cpx     #250
        bcs     @yes
        sec
        rts
@yes:   clc
        rts

        .segment "ROAMHI"

; One square of the turn: monster `think`'s next, if its pace has banked a
; square, else on to the next one's.
turn:   ldx     think
        cpx     #SLOTS
        bcs     @rts
        lda     r_k,x
        beq     @next
        lda     r_art,x
        bmi     @next
        lda     r_pace,x
        cmp     #10
        bcc     @next
        sbc     #10
        sta     r_pace,x
        jsr     closer
        ldx     think
        cmp     #1
        bne     :+
        inc     r_glide,x       ; (and its turn goes on)
@rts:   rts
:       cmp     #2
        bne     :+
        lda     pending
        bne     :+
        inx
        stx     pending
        dex
:       lda     #0
        sta     r_pace,x
@next:  inc     think
        rts

        .segment "ROAMHI"

; Monster X a square nearer the hero (foes.s's foe_closer): straight at them
; if it can, else along the longer way, else the other. In A: 1 if it went
; (and is drawn where it was, to glide on), 0 if it could not, 2 if it is
; beside them already.
closer: stx     fi
        lda     r_x,x
        sta     cx
        lda     r_y,x
        sta     cy
        ldx     #0              ; how far, and which way: ax, sx, ay, sy
        jsr     apart
        inx
        jsr     apart
        lda     ax
        cmp     #2
        bcs     @far
        lda     ay
        cmp     #2
        bcs     @far
        lda     #2
        rts
@far:   lda     tx              ; the square the hero is going to, if they
        ldy     ty              ; are walking, is theirs as well
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
:       and     #127
        sta     gx
        tya
        and     #127
        sta     gy
        lda     sx
        ldy     sy
        beq     @axes
        tax
        beq     @axes
        jsr     try
        bcc     @went
@axes:  lda     ax
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
        rts
@went:  lda     #1
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

; How far cx (X 0) or cy (X 1) is from tx or ty, the short way round the
; engine's 128 squares: in ax or ay, and the step toward, -1, 0 or 1, in sx
; or sy.
apart:  lda     cx,x
        sec
        sbc     tx,x
        and     #127
        beq     @here
        cmp     #64
        bcs     @less
        sta     ax,x            ; past it: back
        lda     #$FF
        sta     sx,x
        rts
@less:  eor     #$7F            ; (carry set: 128 less)
        adc     #0
        sta     ax,x
        lda     #1
        sta     sx,x
        rts
@here:  sta     ax,x
        sta     sx,x
        rts

; A step of (A, Y) for monster fi from (cx, cy): taken, carry clear, if it
; is onto its kind's ground, not dark, where nobody stands or the hero is
; going; else carry set.
try:    sta     dx
        sty     dy
        clc
        adc     cx
        and     #127
        sta     nx
        tya
        clc
        adc     cy
        and     #127
        sta     ny
        lda     nx
        cmp     tx
        bne     :+
        lda     ny
        cmp     ty
        beq     @no
:       lda     nx
        cmp     gx
        bne     :+
        lda     ny
        cmp     gy
        beq     @no
:       lda     nx
        ldy     ny
        jsr     occ
        bcs     @no
        lda     nx
        ldy     ny
        jsr     tileat
        jsr     tidx
        bmi     @no
        ldy     fi
        lda     r_k,y
        and     #SEA_FLAG
        beq     @land
        txa
        bne     @no
        beq     @ok
@land:  txa
        beq     @no
@ok:    ldx     fi              ; there, drawn 16 pixels a square back
        lda     nx
        sta     r_x,x
        lda     ny
        sta     r_y,x
        lda     dx
        jsr     @back
        adc     r_ox,x
        sta     r_ox,x
        lda     dy
        jsr     @back
        adc     r_oy,x
        sta     r_oy,x
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

        .segment "ROAMCODE"

; --- The rest -------------------------------------------------------------

; Carry set, and X who, if a monster stands on square (A, Y).
occ:    sta     t
        ldx     #SLOTS - 1
@o:     lda     r_k,x
        beq     @n
        lda     r_x,x
        cmp     t
        bne     @n
        tya
        cmp     r_y,x
        beq     @yes
@n:     dex
        bpl     @o
        clc
        rts
@yes:   sec
        rts

; The tile at square (A, Y), from the window. Keeps X.
tileat: and     #63
        sta     @at+1
        tya
        pha
        and     #3
        lsr     a
        ror     a
        ror     a
        ora     @at+1
        sta     @at+1
        pla
        and     #63
        lsr     a
        lsr     a
        ora     #>WORLD
        sta     @at+2
@at:    lda     WORLD
        rts

; Tile A's place in tiles[] in X, N clear; or N set if it is none of them.
tidx:   ldx     #NTILES - 1
:       cmp     tiles,x
        beq     :+
        dex
        bpl     :-
:       rts

; The sprite block of monster X's picture, in A: the classes' but the
; leader's. Keeps X and Y.
blockof:
        lda     G_CLS
        asl     a
        sta     bk
        txa
        cmp     bk
        bcc     :+
        adc     #1              ; (carry set: 2)
:       rts

        .segment "ROAMHI"

; Monster X's picture into its block: two bytes a row, the third 0, and
; the last five rows 0.
unpack: lda     r_art,x
        and     #$7F
        sta     r_art,x
        pha                     ; roam_art + 32 a picture
        lsr     a
        lsr     a
        lsr     a
        sta     @src+2
        pla
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     #<roam_art
        sta     @src+1
        lda     @src+2
        adc     #>roam_art
        sta     @src+2
        jsr     blockof         ; SPRITES + 64 a block
        pha
        lsr     a
        ror     a
        ror     a
        and     #$C0
        sta     @dst+1
        pla
        lsr     a
        lsr     a
        clc
        adc     #>SPRITES
        sta     @dst+2
        stx     fi
        ldy     #0
        ldx     #0
@row:   jsr     @two
        jsr     @two
        lda     #0
        jsr     @dst
        cpy     #32
        bne     @row
@z:     jsr     @dst
        cpx     #64
        bne     @z
        ldx     fi
        rts
@two:
@src:   lda     roam_art,y
        iny
@dst:   sta     SPRITES,x
        inx
        rts

; The ground in the old roll's order, its odds out of 200, and what may be
; walked on: the deep sea by the sea's (0), the rest by the land's.
tiles:  .byte   T_DEEP, $96, $60, T_SHORE, T_FOREST, T_SWAMP, T_FLOOR
        .byte   $63, $6A                ; (the road, grass; a pass, a bridge)
NTILES = * - tiles
odds:   .byte   6, 4, 8, 7, 12, 16, 5, 0, 0
edge_x: .byte   9, $F8, 0, 0
edge_y: .byte   0, 0, 4, $FC
; ow_fight.c's: the level each wanderer is met from (bestiary.c's), and the
; sea's; the sea's pictures in roam_art.s, colours and speeds.
w_from: .byte   1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 7
sea_from: .byte 1, 2, 3, 5, 7
sea_art: .byte  10, 11, 12, 10, 13
sea_col: .byte  3, 7, 2, 5, 4
sea_spd: .byte  14, 20, 8, 12, 8

        .segment "ROAMBSS"
r_k:    .res    SLOTS           ; 0: none
r_x:    .res    SLOTS
r_y:    .res    SLOTS
r_ox:   .res    SLOTS           ; pixels off its square, gliding there
r_oy:   .res    SLOTS
r_pace: .res    SLOTS           ; tenths of a square banked
r_glide: .res   SLOTS           ; squares it went this turn
r_col:  .res    SLOTS
r_spd:  .res    SLOTS           ; tenths of a square a turn
r_art:  .res    SLOTS           ; its picture; bit 7 till it is unpacked
rflag:  .res    1
rarg:   .res    1
base:   .res    1               ; the first person that is a monster
back:   .res    1               ; a fight started, on square (bx, by)
bx:     .res    1
by:     .res    1
pending: .res   1               ; 1 + the one that got beside the hero
think:  .res    1               ; whose turn it is
inship: .res    1               ; SEA_FLAG in a ship
changed: .res   1               ; who is shown has changed
anyg:   .res    1
e:      .res    1
top:    .res    1
bk:     .res    1
px:     .res    1
dx:     .res    1
dy:     .res    1
