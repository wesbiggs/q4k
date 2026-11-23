; SPDX-License-Identifier: GPL-3.0-or-later
; q4k -- the open world: the party's lines under the map, and the few
; things the engine says or asks that came after its memory was full.
;
; All of it is in OWLO, the engine's file below $0801 (cfg/c64-openworld.cfg:
; the KERNAL's text screen, which the engine never shows), with its few
; variables beside the code (it is RAM, loaded with the engine), since the
; engine's BSS is full. In assembly, for room. The game block's offsets
; here are checked in engine.c.
;
;  - The text window shows the party as a fight's panel does (fight.c's
;    show_party(): each member's name, then health and a wizard's magic;
;    the health green over half, yellow down to a quarter and red under
;    it), and in the last column, if there is room, the draughts and arrows
;    (by their pictures, mkowfont.py's) and the gold. Anything
;    said takes the window over (sm_say()), and the lines come back with
;    the next step (st_step()).
;  - SPACE hides the window: no split, and the map on all 25 rows
;    (smooth.s's _sm_rows), redrawn there and then; SPACE again brings it
;    back. OWG_HIDE in the game block keeps it hidden across programs.
;    Anything said while it is hidden brings it back, to stay.
;  - A new game opens with a greeting (nothing has happened in it yet).
;  - F1 and F3 ask first, Y or N, as the game does.
;  - A dungeon's chest is opened by standing on it, if it was never taken,
;    and a taken one is drawn open (chestopen, in WORLD only: local[] keeps
;    the chest, so that L there says it is empty).
;  - The keeper idles between two frames, as everyone else does: his
;    characters swapped with keeperbob's (assets/dungeon_tiles.txt).

        .export _st_show, _st_start, _st_step, _st_toggle, _st_save, _st_load
        .export _sm_say
        .export _st_look, _st_arrived, _st_chests, _st_bob
        .import _party, _area, _hx, _hy, _sm_ox, _sm_oy, _sm_frame
        .import _sm_text_clear, _sm_redraw_box, _sm_npcs, _sm_commit
        .import _next_frame, _ov_look, _sm_say_raw, _sm_draw_all, _sm_ink
        .import _sm_rows, _sm_npclim, _sm_scdir, _yes, _save_game, _rebuild
        .import _sm_box_c0, _sm_box_c1, _sm_box_r0, _sm_box_r1
        .import _pre_dir, _frame
        .import _dun_chests, _dun_cx, _dun_cy, _dun_index, _depth, _looted
        .import _ow_mt0, _ow_chq, _loot_table, _town_index
        .export _st_tchests
        .importzp ptr1, ptr2

G_COUNT   = $CE37 + 86          ; the game block's party_count (engine.c
G_GOLD    = $CE37 + 90
G_TURN    = $CE37 + 92          ; checks these)
G_POTIONS = $CE37 + 94
G_ARROWS  = $CE37 + 95
G_QUIET   = $CE37 + 138
OWG_HIDE  = 8
OWB_LOAD  = 8
MAPROWS   = 22                  ; smooth.s's: the map with the window ...
ALLROWS   = 25                  ; ... and without
NPC_LIM   = $DC - 15 + 1        ; smooth.s's NPC_YMAX + 1
ACTOR     = 21                  ; an Actor: hp at 2, hp_max 3, the name 4-12,
A_HP      = 2                   ; mp 17, mp_max 18
A_HPMAX   = 3
A_NAME    = 4
A_MP      = 17
A_MPMAX   = 18
MAX_PARTY = 4

TEXT      = $4000 + 22 * 40     ; the window's first row, in screen A ...
TEXTB     = TEXT + $0400        ; ... and B
TEXTC     = $D800 + 22 * 40     ; and its colour
FONT      = $A0                 ; the text's characters (smooth.s)
WORLD     = $B000
CHARS     = $4800
BIAS      = 32                  ; squares (engine.h)
AREA_DUNGEON = 3
T_CHEST   = $70                 ; tiles.h's T_CHEST (engine.c checks)
T_MONSTER = $8E                 ; and T_MONSTER
T_CHESTOPEN = $95               ; the dungeon's and castle's sets' own
T_KEEPBOB = $94

        .segment "OWLO"

st_up:  .byte 0                 ; the status is in the window
msg_up: .byte 0                 ; something was said over it
pos:    .byte 0                 ; where out() writes: 0-39 the first row,
ink:    .byte 0                 ; 40-79 the second; and in what colour
mem:    .byte 0
col:    .byte 0
nval:   .word 0
rj:     .byte 0                 ; num's: right-justified, the digit, and
dig:    .byte 0                 ; the digits
dbuf:   .res  5
nlead:  .byte 0
sx:     .byte 0
q:      .byte 0
bit0:   .byte 0
bitn:   .byte 0
cnt:    .byte 0
mx:     .byte 0
DUN_LOOT  = 36                  ; game.h's: the castle's chests' bits from
NUM_LOOT  = 2                   ; here (loot_data.h; engine.c checks)
LOOT_SIZE = 6

; --- The status --------------------------------------------------------------

        .segment "HICODE"       ; (the helpers: OWLO is full)

; A as a letter (PETSCII, as smooth.s's font_of has it) at pos, in both
; screens, in ink. Keeps X and Y.
out:    cmp     #$C0
        bcc     :+
        and     #$7F            ; A-Z
        bne     @put            ; (always)
:       cmp     #$40
        bcc     @put
        and     #$1F            ; a-z
@put:   clc
        adc     #FONT
        stx     sx
        ldx     pos
        sta     TEXT,x
        sta     TEXTB,x
        lda     ink
        sta     TEXTC,x
        inc     pos
        ldx     sx
        rts

        .segment "CODE"         ; (OWLO and HICODE are full)

; A in decimal (num16: X the high byte), no leading zeros, from pos; numr
; (numr16) ends it just before pos instead, right-justified. Keeps Y.
num:    ldx     #0
num16:  sta     nval
        stx     nval+1
        lda     #0
        beq     numgo           ; (always)
numr:   ldx     #0
numr16: sta     nval
        stx     nval+1
        lda     #1
numgo:  sta     rj
        tya
        pha
        ldy     #0              ; the digits so far, into dbuf
        ldx     #4
@digit: lda     #$30
        sta     dig
@sub:   lda     nval
        sec
        sbc     tenl,x
        sta     nlead
        lda     nval+1
        sbc     tenh,x
        bcc     @got
        sta     nval+1
        lda     nlead
        sta     nval
        inc     dig
        bne     @sub            ; (always)
@got:   lda     dig
        cpy     #0              ; after the first digit, every one shows
        bne     @keep
        cpx     #0              ; and the units always
        beq     @keep
        cmp     #$30
        beq     @skip
@keep:  sta     dbuf,y
        iny
@skip:  dex
        bpl     @digit
        lda     rj
        beq     @out
        sty     nlead
        lda     pos
        sec
        sbc     nlead
        sta     pos
@out:   ldx     #0
:       lda     dbuf,x
        jsr     out
        inx
        dey
        bne     :-
        pla
        tay
        rts

        .segment "RODATA"       ; (COMMON: OW-DATA's)
tenl:   .byte   <1, <10, <100, <1000, <10000
tenh:   .byte   >1, >10, >100, >1000, >10000

        .segment "HICODE"

; Set pos to A and ink to X.
at:     sta     pos
        stx     ink
        rts

s_hail:    .asciiz "Hail, adventurers! Press ? for help."

        .segment "OWLO"

; --- The window --------------------------------------------------------------

; void sm_say(const char *s): everything said goes through here (A low, X
; high): the window back if it was hidden, the party's lines off it, and
; then smooth.s's sm_say_raw(). The lines come back at the next step.
_sm_say:
        pha
        txa
        pha
        lda     G_QUIET
        and     #OWG_HIDE
        beq     :+
        jsr     window_on
        jmp     @said
:       lda     st_up
        beq     @said
        jsr     _sm_text_clear
@said:  lda     #0
        sta     st_up
        lda     #1
        sta     msg_up
        pla
        tax
        pla
        jmp     _sm_say_raw

; The window back: the split, the map on its 22 rows (the shift copy made
; again for them), and the text rows cleared of it.
window_on:
        lda     G_QUIET
        and     #<~OWG_HIDE
        sta     G_QUIET
        lda     #MAPROWS
        ldx     #NPC_LIM
        jsr     rows
        jmp     _sm_text_clear

; The window hidden: no split, and the map on all 25 rows, drawn now.
window_off:
        lda     G_QUIET
        ora     #OWG_HIDE
        sta     G_QUIET
        lda     #0
        sta     st_up
        sta     msg_up
        lda     #ALLROWS
        ldx     #$FF
        jsr     rows
        sta     _pre_dir        ; (any shift made for the next step: $FF)
        jsr     _sm_draw_all
        jsr     _sm_npcs
        lda     _sm_frame       ; (the frames that took were not missed)
        sta     _frame
        rts

; The map's rows A, and the lowest a sprite may stand X; the CPU's shift
; copy patched again. Returns $FF in A.
rows:   sta     _sm_rows
        stx     _sm_npclim
        lda     #$FF
        sta     _sm_scdir
        rts

; void st_show(void): the party under the map, unless it is hidden.
_st_show:
        lda     G_QUIET
        and     #OWG_HIDE
        bne     :+
        jmp     _st_draw
:       rts

; void st_step(void): a step begins: the party's lines over what was said.
_st_step:
        lda     msg_up
        beq     :+
        lda     #0
        sta     msg_up
        jmp     _st_draw
:       rts

; void st_toggle(void): SPACE.
_st_toggle:
        lda     #0
        sta     msg_up
        lda     G_QUIET
        and     #OWG_HIDE
        beq     window_off
        jsr     window_on
        jmp     _st_draw

; void st_start(void): the engine has the screen: a new game's greeting, or
; the party (or the full screen, if it was hidden when the engine was left).
_st_start:
        lda     G_TURN
        ora     G_TURN+1
        bne     :+
        lda     #7              ; yellow
        sta     _sm_ink
        lda     #<s_hail
        ldx     #>s_hail
        jmp     _sm_say
:       lda     G_QUIET
        and     #OWG_HIDE
        bne     window_off
        jmp     _st_draw

; void st_save(void), st_load(void): F1 and F3, asked first.
_st_save:
        lda     #<s_save_q
        ldx     #>s_save_q
        jsr     _yes
        tax
        beq     _st_show
        jmp     _save_game

_st_load:
        lda     #<s_load_q
        ldx     #>s_load_q
        jsr     _yes
        tax
        beq     _st_show
        lda     #OWB_LOAD
        jmp     _rebuild

s_save_q:  .asciiz "Overwrite save? (Y/N)"     ; (the game's words)
s_load_q:  .asciiz "Load game? (Y/N)"

; st_draw: the window cleared, and the party as a fight's panel has them
; (src/fight/fight.c's show_party()).
_st_draw:
        jsr     _sm_text_clear
        lda     #0
        sta     mem
        sta     col
@member:
        lda     mem
        cmp     G_COUNT
        bcc     :+
        jmp     @rest
:       tax
        lda     member,x
        clc
        adc     #<_party
        sta     ptr1
        lda     #>_party
        adc     #0
        sta     ptr1+1
        ldx     col             ; the name, in white
        inx
        txa
        ldx     #1
        jsr     at
        ldy     #A_NAME
:       lda     (ptr1),y
        beq     :+
        jsr     out
        iny
        cpy     #A_NAME + 9
        bne     :-
:       ldy     #A_HPMAX        ; health: green over half, yellow down
        lda     (ptr1),y        ; to a quarter, red under it
        lsr     a
        sta     q               ; (half, rounded down)
        ldx     #5
        ldy     #A_HP
        lda     (ptr1),y
        cmp     q
        beq     :+
        bcs     @ink            ; more than half
:       ldx     #7
        ldy     #A_HPMAX
        lda     (ptr1),y
        clc
        adc     #3
        ror     a
        lsr     a
        sta     q               ; (a quarter, rounded up)
        ldy     #A_HP
        lda     (ptr1),y
        cmp     q
        bcs     @ink            ; at least a quarter
        ldx     #2
@ink:   lda     col
        clc
        adc     #41
        jsr     at
        lda     (ptr1),y
        jsr     num
        lda     #$2F            ; /
        jsr     out
        ldy     #A_HPMAX
        lda     (ptr1),y
        jsr     num
        ldy     #A_MPMAX        ; a wizard's magic, in cyan
        lda     (ptr1),y
        beq     @next
        lda     col
        clc
        adc     #47
        ldx     #3
        jsr     at
        ldy     #A_MP
        lda     (ptr1),y
        jsr     num
@next:  lda     col             ; nine across: eight for a name, and two
        clc                     ; for a third member's magic before the
        adc     #9              ; arrows at 29 ("26/26 12  Arrows")
        sta     col
        inc     mem
        jmp     @member

@rest:  cmp     #MAX_PARTY      ; the draughts, arrows and gold, if there
        bcs     @done           ; is room, by their pictures: "D 12 A 34"
        lda     #30             ; over "C  5678", the numbers right-justified
        ldx     #4              ; (column 39 is under the border)
        jsr     at
        lda     #$DE            ; a draught, in purple (mkowfont.py)
        jsr     out
        lda     #34
        ldx     #1
        jsr     at
        lda     G_POTIONS
        jsr     numr
        lda     #35
        ldx     #2              ; (brown would be 9: a multicolour
        jsr     at              ; character here, so red)
        lda     #$DF            ; an arrow
        jsr     out
        lda     #39
        ldx     #1
        jsr     at
        lda     G_ARROWS
        jsr     numr
        lda     #70
        ldx     #7
        jsr     at
        lda     #$DD            ; a coin, in yellow
        jsr     out
        lda     #79
        ldx     #1
        jsr     at
        lda     G_GOLD
        ldx     G_GOLD+1
        jsr     numr16
@done:  lda     #1
        sta     st_up
        rts

member: .byte   0, ACTOR, 2 * ACTOR, 3 * ACTOR

; --- Chests ------------------------------------------------------------------

; ptr1 at the hero's square in WORLD: (hy - BIAS) * 64 + hx - BIAS.
under:  lda     #0
        sta     ptr1
        lda     _hy
        sec
        sbc     #BIAS
        lsr     a
        ror     ptr1
        lsr     a
        ror     ptr1
        clc
        adc     #>WORLD
        sta     ptr1+1
        lda     _hx
        sec
        sbc     #BIAS
        ora     ptr1
        sta     ptr1
        ldy     #0
        lda     (ptr1),y        ; (what is there: A, with Y 0)
        rts

; void st_look(void): L (the overlay's), and then a dungeon's chest on the
; hero's square, emptied now or before, is drawn open: the square redrawn
; on the hidden screen and flipped to, as a town's roofs are (town.c's
; roof_check()).
_st_look:
        jsr     _ov_look
        lda     _area           ; (a place: the castle's chests too)
        beq     no
        jsr     under
        cmp     #T_CHEST
        bne     no
        lda     #T_CHESTOPEN
        sta     (ptr1),y
        lda     _hx             ; its four characters on the screen
        asl     a
        sec
        sbc     _sm_ox
        sta     _sm_box_c0
        tax
        inx
        stx     _sm_box_c1
        lda     _hy
        asl     a
        sec
        sbc     _sm_oy
        sta     _sm_box_r0
        tax
        inx
        stx     _sm_box_r1
        jsr     _sm_redraw_box
        lda     #$FF
        sta     _pre_dir
        jsr     _sm_npcs
        lda     #1
        jsr     _sm_commit
        jsr     _next_frame
        lda     _sm_frame
        sta     _frame
no:     rts

; void st_arrived(void): the end of a step: a chest not yet taken (drawn
; shut) is opened.
_st_arrived:
        lda     _area
        beq     no
        jsr     under
        cmp     #T_CHEST
        bne     no
        beq     _st_look        ; (always)

; The WORLD square (A, Y) drawn open, if it shows a chest and looted[]'s
; bit bitn is set. (A chest under a roof shows the roof, and is left.)
mark:   sta     mx
        lda     bitn
        and     #7
        tax
        lda     bits,x
        sta     sx
        lda     bitn
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     _looted,x
        and     sx
        beq     @no
        lda     #0
        sta     ptr1
        tya
        lsr     a
        ror     ptr1
        lsr     a
        ror     ptr1
        clc
        adc     #>WORLD
        sta     ptr1+1
        lda     mx
        ora     ptr1
        sta     ptr1
        ldy     #0
        lda     (ptr1),y
        cmp     #T_CHEST
        bne     @no
        lda     #T_CHESTOPEN
        sta     (ptr1),y
@no:    rts

; void st_chests(void): a dungeon level just built into WORLD: the chests
; taken before, by their looted[] bits as d_look() counts them, drawn open.
_st_chests:
        lda     _dun_index      ; (dun_index * MAX_DEEP + depth - 1) * 3
        asl     a
        asl     a
        clc
        adc     _depth
        sec
        sbc     #1
        sta     bit0
        asl     a
        adc     bit0
        sta     bitn
        lda     #0
        sta     cnt
@chest: ldx     cnt
        cpx     _dun_chests
        bcs     @done
        ldy     _dun_cy,x
        lda     _dun_cx,x
        jsr     mark
        inc     bitn
        inc     cnt
        bne     @chest          ; (always)
@done:  rts

; void st_tchests(void): the castle's (or a town's) WORLD after its roofs
; (town.c's put_roofs(), which puts back what a roof covered as local[]
; has it): its chests taken before, by loot_table's bits, drawn open.
_st_tchests:
        lda     #DUN_LOOT
        sta     bitn
        lda     #0
        sta     cnt
@entry: ldx     cnt
        lda     _loot_table,x   ; map, x, y
        cmp     _town_index
        bne     @next
        ldy     _loot_table+2,x
        lda     _loot_table+1,x
        jsr     mark
@next:  inc     bitn
        lda     cnt
        clc
        adc     #LOOT_SIZE
        sta     cnt
        cmp     #NUM_LOOT * LOOT_SIZE
        bne     @entry
        rts

        .segment "HICODE"
bits:   .byte   1, 2, 4, 8, 16, 32, 64, 128
        .segment "OWLO"

; --- The keeper idles --------------------------------------------------------

; The zero page pointer at X to CHARS + 8 * the character of metatile A's
; quarter q.
chptr:  clc
        adc     q
        tay
        lda     _ow_chq,y
        sta     0,x
        lda     #>(CHARS / 8)   ; ($0900 + c) * 8 = CHARS + c * 8
        sta     1,x
        asl     0,x
        rol     1,x
        asl     0,x
        rol     1,x
        asl     0,x
        rol     1,x
        rts

; void st_bob(void): in a dungeon, the keeper's four characters and
; keeperbob's change places in the character set (where the two share one,
; it changes places with itself).
_st_bob:
        lda     _area
        cmp     #AREA_DUNGEON
        bne     @out
        lda     #0
        sta     q
@quarter:
        lda     _ow_mt0 + T_MONSTER - $60
        ldx     #ptr1
        jsr     chptr
        lda     _ow_mt0 + T_KEEPBOB - $60
        ldx     #ptr2
        jsr     chptr
        ldy     #7
:       lda     (ptr1),y
        tax
        lda     (ptr2),y
        sta     (ptr1),y
        txa
        sta     (ptr2),y
        dey
        bpl     :-
        lda     q
        clc
        adc     #32
        sta     q
        bpl     @quarter        ; 0, 32, 64, 96
@out:   rts

; Sprite 0's pointer on the KERNAL's screen, which this file loads over
; while the disk icon is showing: the icon's block (ow_disk.s's DISK_BLOCK).
        .segment "OWSPTR"
        .byte   $0E, 0, 0, 0, 0, 0, 0, 0
