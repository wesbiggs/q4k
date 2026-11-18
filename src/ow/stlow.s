; q4k -- the open world: status.c's heavy lifting, in assembly, for room.
;
; In OWLO, the engine's file below $0801 (cfg/c64-openworld.cfg), with its
; few variables beside the code (it is RAM, loaded with the engine), since
; the engine's BSS is full. status.c says what each is for; the game
; block's offsets here are checked there.
;
;   st_draw     the party's status in the two lines under the map
;   st_look     L, and a dungeon's chest drawn open after it
;   st_arrived  a step ends on a chest not yet taken: opened
;   st_chests   a dungeon level built: the chests taken drawn open
;   st_bob      the keeper's characters swapped with keeperbob's

        .export _st_draw, _st_look, _st_arrived, _st_chests, _st_bob
        .export _st_up, _msg_up
        .import _party, _area, _hx, _hy, _sm_ox, _sm_oy, _sm_frame
        .import _sm_text_clear, _sm_redraw_box, _sm_npcs, _sm_commit
        .import _next_frame, _ov_look
        .import _sm_box_c0, _sm_box_c1, _sm_box_r0, _sm_box_r1
        .import _pre_dir, _frame
        .import _dun_chests, _dun_cx, _dun_cy, _dun_index, _depth, _looted
        .import _ow_mt0, _ow_chq
        .importzp ptr1, ptr2

G_COUNT   = $CE3F + 86          ; the game block's party_count (status.c
G_POTIONS = $CE3F + 94          ; checks these three)
G_ARROWS  = $CE3F + 95
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
T_CHEST   = $70                 ; tiles.h's T_CHEST (status.c checks)
T_MONSTER = $8E                 ; and T_MONSTER
T_CHESTOPEN = $93               ; the dungeon set's own (dungeon_tiles.txt)
T_KEEPBOB = $94

        .segment "OWLO"

_st_up:  .byte 0                ; the status is in the window
_msg_up: .byte 0                ; something was said over it
pos:    .byte 0                 ; where out() writes: 0-39 the first row,
ink:    .byte 0                 ; 40-79 the second; and in what colour
mem:    .byte 0
col:    .byte 0
nval:   .byte 0
nlead:  .byte 0
sx:     .byte 0
q:      .byte 0
bit0:   .byte 0

; --- The status --------------------------------------------------------------

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

; A in decimal, no leading zeros. Keeps Y.
num:    sta     nval
        tya
        pha
        lda     #0
        sta     nlead
        ldx     #2
@digit: ldy     #0
        lda     nval
@sub:   cmp     tens,x
        bcc     @got
        sbc     tens,x
        iny
        bne     @sub            ; (always)
@got:   sta     nval
        tya
        bne     @show
        cpx     #0              ; the units always show
        beq     @show
        lda     nlead
        beq     @skip
@show:  inc     nlead
        tya
        ora     #$30
        jsr     out
@skip:  dex
        bpl     @digit
        pla
        tay
        rts

tens:   .byte   1, 10, 100

; A string at X/A (low, high), zero ended, from pos.
str:    sta     ptr2+1
        stx     ptr2
        ldy     #0
:       lda     (ptr2),y
        beq     :+
        jsr     out
        iny
        bne     :-
:       rts

; Set pos to A and ink to X.
at:     sta     pos
        stx     ink
        rts

; void st_draw(void): the window cleared, and the party as a fight's panel
; has them (src/fight/fight.c's show_party()).
_st_draw:
        jsr     _sm_text_clear
        lda     #0
        sta     mem
        sta     col
@member:
        lda     mem
        cmp     G_COUNT
        bcs     @rest
        tax
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
:       ldx     #5              ; health, green, or red at none
        ldy     #A_HP
        lda     (ptr1),y
        bne     :+
        ldx     #2
:       lda     col
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
@next:  lda     col
        clc
        adc     #10
        sta     col
        inc     mem
        jmp     @member

@rest:  cmp     #MAX_PARTY      ; the draughts and arrows, if there is room
        bcs     @done
        lda     #29             ; (column 39 is under the border)
        ldx     #3
        jsr     at
        ldx     #<s_draught
        lda     #>s_draught
        jsr     str
        lda     #1
        sta     ink
        lda     G_POTIONS
        jsr     num
        lda     #69
        ldx     #3
        jsr     at
        ldx     #<s_arrows
        lda     #>s_arrows
        jsr     str
        lda     #1
        sta     ink
        lda     G_ARROWS
        jsr     num
@done:  lda     #1
        sta     _st_up
        rts

member: .byte   0, ACTOR, 2 * ACTOR, 3 * ACTOR
s_draught: .asciiz "Draught "
s_arrows:  .asciiz "Arrows "

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
        lda     _area
        cmp     #AREA_DUNGEON
        bne     no
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
        cmp     #AREA_DUNGEON
        bne     no
        jsr     under
        cmp     #T_CHEST
        bne     no
        beq     _st_look        ; (always)

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
        sta     bit0
        ldx     #0
@chest: cpx     _dun_chests
        bcs     no
        txa
        clc
        adc     bit0
        pha
        and     #7
        tay
        lda     bits,y
        sta     sx
        pla
        lsr     a
        lsr     a
        lsr     a
        tay
        lda     _looted,y
        and     sx
        beq     @next
        lda     #0
        sta     ptr1
        lda     _dun_cy,x
        lsr     a
        ror     ptr1
        lsr     a
        ror     ptr1
        clc
        adc     #>WORLD
        sta     ptr1+1
        lda     _dun_cx,x
        ora     ptr1
        sta     ptr1
        ldy     #0
        lda     #T_CHESTOPEN
        sta     (ptr1),y
@next:  inx
        bne     @chest          ; (always)

bits:   .byte   1, 2, 4, 8, 16, 32, 64, 128

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
