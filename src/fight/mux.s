; Quest for the King -- the multicolour fight: the screen, and more sprites
; than the VIC has.
;
; The fight asks for up to sixteen sprites (vs_*: the party, six monsters or
; the four quarters of a big one, a shot, the cursor) and the VIC shows
; eight, two of which are the field's rounded corners: so the six others'
; registers are used again further down the screen once what they were
; showing has been drawn. mx_plan() works out who gets which, in the main
; program; the raster interrupt only carries it out.
;
; The corners, as the open world's (src/ow/smooth.s): the field is black
; outside a quarter circle of three squares' radius in each corner. Real
; sprites 0 and 1, hi-res, black, doubled both ways (48 by 42), in front of
; everything (the lower a sprite's number, the nearer it is drawn), are the
; top two to line 92 and then, from an interrupt at LINE_CNR, the bottom
; two, whose pictures are at SPR_CORNER (src/fight/corners.s).
;
; The plan. The sprites wanted are sorted by how high they stand. The first
; six take real sprites 2-7, in order, and are set before the frame
; begins. Each one after that wants the real sprite given out longest ago
; (they are given out in turn, so that is the next in turn as well), which
; is free once the raster is 22 lines past the y it was given: a sprite at
; y is drawn on lines y+1 to y+21. If the newcomer stands at least 3 lines
; below that it gets an `event` -- on that line the interrupt rewrites that
; sprite's registers -- and otherwise there is no room for it this frame.
; Squares are 16 lines apart and a sprite 21 tall, so the seventh sprite on
; two neighbouring rows is the one that goes, and when any did the next plan
; starts its sort one sprite on, so that among those level with each other
; it is another's turn: a crowd flickers, and nobody vanishes.
;
; Two plans are kept, and the interrupt at the bottom of the frame takes up
; a new one whole, so the main program never changes what a frame is being
; drawn from.
;
; The frame. One raster interrupt, moved down the screen:
;
;   line 251  below the last text row: the background back to the field's,
;             the new plan if there is one, the corners at the top, the
;             first six sprites, and
;             the music and the effects, once a frame (ow_tune.s, as
;             the open world's engines call it), and the KERNAL's own work
;             (the keyboard, the clock) through $EA31
;   events    each on the line its sprite comes free; several at once when
;             their lines are within two of each other; and the corners
;             going down, at LINE_CNR, in among them
;   line 198  inside the solid black row under the field: the background to
;             black, for the text. Nothing shows of when, in a row with no
;             background in it, so it needs no stable raster.
;
; The CIA's timer interrupt is off while this runs, as in the prototype
; (src/ow/smooth.s).

        .export _vs_x, _vs_xh, _vs_y, _vs_ptr, _vs_col
        .export _mx_frame, _mx_drops, _mx_most
        .export _mx_font, _mx_colour, _mx_start, _mx_stop
        .export _mx_plan, _mx_wait
        .importzp ptr1, ptr2
        .import ow_tune_irq

NV       = 16                   ; sprites to ask for
SCREEN   = $4000
INFO     = $4400
FONT     = $4C00
SPRPTR   = SCREEN + $3F8
FI_PAL   = 0
FI_COLOUR = 32

LINE_TOP  = 251                 ; the frame's first interrupt
LINE_TEXT = 198                 ; the background goes black
LINE_CNR  = 100                 ; the corners go down (from 93 to 150)
NREAL     = 6                   ; real sprites for the plan: 2-7
SPR_CORNER = $5C                ; (include/q4k/fight.h)
CNR_XL    = 24                  ; the field: 40 columns, rows 0-17
CNR_XR    = 24 + 320 - 48       ; (past 255)
CNR_YT    = 50                  ; lines 51-92
CNR_YB    = 51 + 18 * 8 - 43    ; lines 152-194
TALL      = 22                  ; lines from a sprite's y until it is done
MARGIN    = 3                   ; and from there to the next one's y

        .segment "BSS"

_vs_x:   .res NV                ; where, low byte
_vs_xh:  .res NV                ; ... and the ninth bit
_vs_y:   .res NV                ; 0: this one is off
_vs_ptr: .res NV                ; which block
_vs_col: .res NV
_mx_frame: .res 1
_mx_drops: .res 1
_mx_most:  .res 1

; The two plans, sixteen entries each (the second's are at +16): what to
; write, and for the seventh entry on, the line to write it on. Entry j uses
; real sprite real_of[j].
p_x:    .res 32
p_y:    .res 32
p_msb:  .res 32                 ; its bit of $D010, or 0
p_ptr:  .res 32
p_col:  .res 32
p_line: .res 32
p_cnt:  .res 32                 ; at +0 and +16: how many entries

cur:    .res 1                  ; the plan on show: 0 or 16
pending: .res 1                 ; the other one is ready
phase:  .res 1                  ; which interrupt is next: 0 top, 1 an
                                ; event, 2 the text's
evi:    .res 1                  ; the next event's entry
evend:  .res 1                  ; one past the last
cdown:  .res 1                  ; the corners have gone down this frame
d010:   .res 1                  ; $D010 as it stands
bgcol:  .res 1

; mx_plan's workings.
base:   .res 1
rot:    .res 1
n:      .res 1
m:      .res 1
k:      .res 1
cnt:    .res 1
key:    .res 1
keyy:   .res 1
oi:     .res 1
line:   .res 1
tmpx:   .res 1
ord:    .res NV
asgy:   .res NV

; What the caller's machine looked like.
old18:  .res 1
old16:  .res 1
old11:  .res 1
olddd:  .res 1
old20:  .res 1
old21:  .res 1
oldirq: .res 2

        .segment "RODATA"

bits:   .byte $01,$02,$04,$08,$10,$20,$40,$80
nbits:  .byte $FE,$FD,$FB,$F7,$EF,$DF,$BF,$7F
ena:    .byte $03,$07,$0F,$1F,$3F,$7F,$FF   ; the corners, and n of the plan's
real_of: .byte 2,3,4,5,6,7,2,3,4,5,6,7,2,3,4,5   ; an entry's real sprite

        .segment "CODE"

; ---------------------------------------------------------------------------
; void mx_plan(void) -- see the top of the file.

_mx_plan:
        lda     #0              ; whatever was ready is withdrawn: after
        sta     pending         ; this the interrupt will not swap plans
        sta     _mx_drops
        lda     cur
        eor     #16
        sta     base

        ; The sprites that are on, starting from `rot`.
        ldx     rot
        ldy     #0
        lda     #NV
        sta     cnt
@c:     lda     _vs_y,x
        beq     @skip
        txa
        sta     ord,y
        iny
@skip:  inx
        cpx     #NV
        bne     :+
        ldx     #0
:       dec     cnt
        bne     @c
        sty     n

        ; Sorted by y, by insertion: there are few, and they are nearly in
        ; order already. Level ones stay as they came.
        ldx     #1
@outer: cpx     n
        bcs     @sorted
        stx     oi
        lda     ord,x
        sta     key
        tay
        lda     _vs_y,y
        sta     keyy
        txa
        tay                     ; Y: where the key might go
@inner: dey
        bmi     @place
        ldx     ord,y
        lda     keyy
        cmp     _vs_y,x
        bcs     @place          ; the one before is no lower: here
        lda     ord,y
        sta     ord+1,y
        jmp     @inner
@place: iny
        lda     key
        sta     ord,y
        ldx     oi
        inx
        bne     @outer          ; (always)

@sorted:
        lda     #0
        sta     m
        sta     k
@as:    ldy     k
        cpy     n
        bcs     @done
        ldx     ord,y           ; X: the sprite
        ldy     m
        cpy     #NREAL
        bcc     @take           ; (its line is not looked at)
        cpy     #NV
        bcs     @drop
        lda     asgy-NREAL,y    ; when the one six before is done
        clc
        adc     #TALL
        sta     line
        adc     #MARGIN         ; (carry clear: y is under 200)
        cmp     _vs_y,x
        beq     @take
        bcs     @drop           ; this one starts too soon after
@take:  lda     _vs_y,x
        sta     asgy,y
        tya
        ora     base
        tay                     ; Y: the entry
        lda     _vs_y,x
        sta     p_y,y
        lda     _vs_x,x
        sta     p_x,y
        lda     _vs_ptr,x
        sta     p_ptr,y
        lda     _vs_col,x
        sta     p_col,y
        lda     line
        sta     p_line,y
        lda     _vs_xh,x
        beq     :+
        stx     tmpx
        ldx     m
        lda     real_of,x
        tax
        lda     bits,x
        ldx     tmpx
:       sta     p_msb,y
        inc     m
        bne     @next           ; (always)
@drop:  inc     _mx_drops
@next:  inc     k
        bne     @as             ; (always)

@done:  ldx     base
        lda     m
        sta     p_cnt,x
        cmp     _mx_most
        bcc     :+
        sta     _mx_most
:       lda     _mx_drops       ; somebody went without: another's turn
        beq     :+
        ldx     rot
        inx
        txa
        and     #NV - 1
        sta     rot
:       lda     #1
        sta     pending
        rts

; void mx_wait(void) -- until the frame's first interrupt has run.
_mx_wait:
        lda     _mx_frame
:       cmp     _mx_frame
        beq     :-
        rts

; ---------------------------------------------------------------------------
; The interrupt.

irq:    lda     $D019
        sta     $D019
        lda     phase
        bne     :+
        jmp     top
:       cmp     #1
        bne     :+
        jmp     events
:

        lda     #0              ; the text's background
        sta     $D021
        sta     phase
        lda     #LINE_TOP
        sta     $D012
        jmp     $EA81

events: lda     cdown           ; the corners down?
        bne     :+
        lda     $D012
        cmp     #LINE_CNR - 2
        bcc     :+
        jsr     corners_down
:       ldx     evi
        cpx     evend
        beq     @sched
        lda     p_line,x        ; an event, if its line is here
        sec
        sbc     #2
        cmp     $D012
        bcs     @sched
@e:     txa
        and     #15
        tay
        lda     real_of,y
        tay                     ; Y: the real sprite
        lda     p_ptr,x
        sta     SPRPTR,y
        lda     p_col,x
        sta     $D027,y
        lda     d010
        and     nbits,y
        ora     p_msb,x
        sta     d010
        sta     $D010
        tya
        asl     a
        tay
        lda     p_x,x
        sta     $D000,y
        lda     p_y,x
        sta     $D001,y
        inx
        stx     evi
        jmp     events          ; the next, if it is here already, or the
@sched: jsr     sched           ; one after this
        bcs     :+
        sec
        sbc     #2
        cmp     $D012
        bcc     events
:       jmp     $EA81

; The next interrupt after the top's or an event's: the corners going down,
; or the next event, whichever is first, at phase 1 (carry clear, the line
; in A); else the text's.
sched:  lda     #$FF
        ldx     cdown
        bne     :+
        lda     #LINE_CNR
:       ldx     evi
        cpx     evend
        beq     :+
        cmp     p_line,x
        bcc     :+
        lda     p_line,x
:       cmp     #$FF
        beq     @text
        sta     $D012
        ldx     #1
        stx     phase
        clc
        rts
@text:  lda     #LINE_TEXT
        sta     $D012
        lda     #2
        sta     phase
        sec
        rts

corners_down:
        lda     #CNR_YB
        sta     $D001
        sta     $D003
        lda     #SPR_CORNER + 2
        sta     SPRPTR
        lda     #SPR_CORNER + 3
        sta     SPRPTR + 1
        inc     cdown
        rts

top:    lda     bgcol
        sta     $D021
        lda     pending
        beq     :+
        lda     cur
        eor     #16
        sta     cur
        lda     #0
        sta     pending
:       lda     #CNR_YT         ; the corners at the top
        sta     $D001
        sta     $D003
        lda     #SPR_CORNER
        sta     SPRPTR
        lda     #SPR_CORNER + 1
        sta     SPRPTR + 1
        ldx     cur
        lda     #0
        sta     cdown
        ; The first six, whether the plan has that many or not: the ones
        ; it has not are not enabled.
        .repeat NREAL, S
        lda     p_x + S,x
        sta     $D004 + 2 * S
        lda     p_y + S,x
        sta     $D005 + 2 * S
        lda     p_ptr + S,x
        sta     SPRPTR + 2 + S
        lda     p_col + S,x
        sta     $D029 + S
        .endrepeat
        ldy     p_cnt,x
        cpy     #NREAL + 1
        bcs     @more
        ; Six or fewer: only their bits of $D010 count.
        lda     ena,y
        sta     $D015
        jsr     msbs
        txa                     ; (no events)
        sta     evi
        sta     evend
        bcc     @out            ; (always: msbs clears carry)
@more:  lda     #$FF
        sta     $D015
        jsr     msbs
        tya                     ; the events: entries 6 to count - 1
        clc
        adc     cur
        sta     evend
        txa
        clc
        adc     #NREAL
        sta     evi
@out:   jsr     sched
        inc     _mx_frame
        jsr     ow_tune_irq     ; the music and the effects (ow_tune.s)
        jmp     $EA31

; $D010 for the first six entries of the plan at X, and the right corner's.
; Carry clear.
msbs:   lda     p_msb,x
        ora     p_msb + 1,x
        ora     p_msb + 2,x
        ora     p_msb + 3,x
        ora     p_msb + 4,x
        ora     p_msb + 5,x
        ora     #$02
        sta     d010
        sta     $D010
        clc
        rts

; ---------------------------------------------------------------------------
; void mx_font(void) -- the letters: the ROM's lowercase set, its first 128
; characters, to $4C00, where they are characters $80-$FF. Interrupts off
; while the ROM is where the I/O was.

_mx_font:
        sei
        lda     $01
        pha
        and     #$FB
        sta     $01
        lda     #<$D800
        sta     ptr1
        lda     #>$D800
        sta     ptr1+1
        lda     #<FONT
        sta     ptr2
        lda     #>FONT
        sta     ptr2+1
        ldx     #4
        ldy     #0
:       lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     :-
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     :-
        pla
        sta     $01
        cli
        rts

; void mx_colour(void) -- colour RAM for the field and the black row under
; it, each character's from the field's facts. (The eight cells past the
; row are text, which is coloured as it is written.)

_mx_colour:
        ldx     #0
:       ldy     SCREEN,x
        lda     INFO + FI_COLOUR,y
        sta     $D800,x
        ldy     SCREEN + $100,x
        lda     INFO + FI_COLOUR,y
        sta     $D900,x
        ldy     SCREEN + $200,x
        lda     INFO + FI_COLOUR,y
        sta     $DA00,x
        inx
        bne     :-
        rts

; void mx_start(void) -- the screen at $4000 in VIC bank 1, multicolour
; characters from $4800, the field's colours, every sprite multicolour, and
; the raster interrupt. What was there is remembered for mx_stop().

_mx_start:
        sei
        lda     $D018
        sta     old18
        lda     $D016
        sta     old16
        lda     $D011
        sta     old11
        lda     $DD00
        sta     olddd
        lda     $D020
        sta     old20
        lda     $D021
        sta     old21
        lda     $0314
        sta     oldirq
        lda     $0315
        sta     oldirq+1

        lda     #0
        sta     $D015
        sta     $D01B
        sta     $D010
        sta     $D020
        sta     $D027           ; the corners' black
        sta     $D028
        sta     $D025           ; the sprites' black
        sta     phase
        sta     pending
        sta     cur
        sta     p_cnt
        sta     p_cnt + 16
        lda     #10             ; and light red
        sta     $D026
        lda     #$FC            ; all multicolour but the corners, which
        sta     $D01C           ; are doubled
        lda     #$03
        sta     $D017
        sta     $D01D
        lda     #CNR_XL
        sta     $D000
        lda     #<CNR_XR
        sta     $D002
        lda     INFO + FI_PAL
        sta     bgcol
        sta     $D021
        lda     INFO + FI_PAL + 1
        sta     $D022
        lda     INFO + FI_PAL + 2
        sta     $D023

        lda     $DD00           ; VIC bank 1
        and     #$FC
        ora     #$02
        sta     $DD00
        lda     #$02            ; screen at +$0000, characters at +$0800
        sta     $D018
        lda     #$D8            ; multicolour, 40 columns
        sta     $D016
        lda     #$1B
        sta     $D011

        lda     #$7F            ; the CIA's timer interrupt off ...
        sta     $DC0D
        lda     $DC0D
        lda     #<irq           ; ... and the raster's instead
        sta     $0314
        lda     #>irq
        sta     $0315
        lda     #LINE_TOP
        sta     $D012
        lda     #$01
        sta     $D01A
        sta     $D019
        cli
        rts

; void mx_stop(void) -- the machine as mx_start() found it.

_mx_stop:
        sei
        lda     #0
        sta     $D01A
        sta     $D015
        sta     $D01C
        sta     $D017
        sta     $D01D
        lda     #$FF
        sta     $D019
        lda     oldirq
        sta     $0314
        lda     oldirq+1
        sta     $0315
        lda     #$81            ; the CIA's timer interrupt again
        sta     $DC0D
        lda     olddd
        sta     $DD00
        lda     old18
        sta     $D018
        lda     old16
        sta     $D016
        lda     old11
        and     #$7F
        sta     $D011
        lda     old20
        sta     $D020
        lda     old21
        sta     $D021
        cli
        rts
