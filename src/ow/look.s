; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the open world's engine: L, looking about the map.
;
; The view moves as the hero would, a square a step, but the party stays
; where it stood, drawn as the last of the people (after the overworld's
; ship, or a dungeon's monsters), and no time passes: nothing wanders out,
; nothing comes, nothing more is seen (the overlays' fog and monsters wait
; while `looking`), and what has not been seen is still cloud or dark. SPACE
; puts the view back on the party, RUN/STOP (or L again) ends the look there.
; The overlay says whether it may start (OVU_LOOK), moves the overworld's
; view (OVU_STEP: wx, wy and the streaming) and puts its window back
; (OVU_HOME, OVU_END); the rest is here, in TAIL, past BSS: a file of its
; own (ow-cpu.tl, ow-reu.tl) that main() loads after the .hi. In assembly:
; in C it was twice the room TAIL has.

        .export _look_start, _look_step, _look_after, _look_key, _look_can
        .export _look_far
        .import _looking, _look_how, _wx, _wy, _hx, _hy, _dir
        .import _area, _area_w, _area_h, _dirdx, _dirdy, _boat
        .import _sm_npc_n, _sm_npc_x, _sm_npc_y
        .import _sm_npc_ptr, _sm_npc_col, _sm_tunic, _sm_npcs, _sm_ink
        .import _ov_use, _hero_frame, _sm_say, _st_step, _pre_dir
        .import _place_camera, _sm_draw_all, _frame, _sm_frame
        .import pusha

AREA_HOME = 0                   ; (owgame.h)
BIAS      = 32                  ; (engine.h)
NONE      = $FF
OVU_LOOK  = 2                   ; (engine.h)
OVU_STEP  = 3
OVU_HOME  = 4
OVU_END   = 5
HERO_Y    = 138                 ; the hero's sprites' line (sm_init)

        .segment "TAILCODE"     ; (RAM, loaded: variables here too)

lk_wx:  .res 2                  ; where the party is
lk_wy:  .res 1
lk_hx:  .res 1
lk_hy:  .res 1
lk_dir: .res 1
lk_n:   .res 1                  ; the people there were
_look_far: .res 1               ; squares the view has gone, to 255
n:      .res 1
k:      .res 1

; unsigned char __fastcall__ look_can(unsigned char d) -- whether the view
; may go that way: anywhere on the overworld, which wraps; within a place's
; map.
_look_can:
        tax
        lda     _area
        beq     @yes            ; (AREA_HOME)
        lda     _hx
        clc
        adc     _dirdx,x
        sec
        sbc     #BIAS
        cmp     _area_w
        bcs     @no
        lda     _hy
        clc
        adc     _dirdy,x
        sec
        sbc     #BIAS
        cmp     _area_h
        bcs     @no
@yes:   ldx     #0
        lda     #1              ; (last: cc65 tests the flags A leaves)
        rts
@no:    lda     #0
        tax
        rts

; The party's figure, the last of the people, if the view is near enough
; to show it: the engine's squares repeat every 128 of the world's, and the
; world is 384 by 256.
mark:   ldy     lk_n
        lda     _area
        bne     @put
        ldy     _sm_npc_n
        lda     _look_far       ; (farther, and the party might be drawn
        cmp     #100            ; 128 squares off, where the engine's
        bcc     @put            ; squares repeat)
        jmp     _sm_npcs
@put:   sty     n
        lda     lk_hx
        sta     _sm_npc_x,y
        lda     lk_hy
        sta     _sm_npc_y,y
        lda     lk_dir          ; (never off the square: sm_npc_ox, oy 0)
        jsr     pusha
        lda     #0
        jsr     _hero_frame     ; (the ship's, in one)
        ldy     n
        clc
        adc     #64
        sta     _sm_npc_ptr,y
        lda     _sm_tunic
        ldx     _boat
        beq     :+
        lda     #9              ; (COLOR_BROWN, the ship)
:       sta     _sm_npc_col,y
        iny
        sty     _sm_npc_n
@done:  jmp     _sm_npcs

; void look_start(void) -- L: if the overlay says so (not with seven people
; in a town, who leave no sprite for the party; not while a dungeon's
; monster comes for a fight).
_look_start:
        lda     #OVU_LOOK
        jsr     _ov_use
        tax
        beq     @no
        .repeat 2, i            ; wx, wy, hx, hy, dir: lk_wx ...
        lda     _wx+i
        sta     lk_wx+i
        .endrep
        lda     _wy
        sta     lk_wy
        lda     _hx
        sta     lk_hx
        lda     _hy
        sta     lk_hy
        lda     _dir
        sta     lk_dir
        lda     _sm_npc_n
        sta     lk_n
        lda     #0
        sta     _look_far
        sta     $D001           ; the hero's sprites off the screen
        sta     $D00F
        lda     #1
        sta     _looking
        jsr     mark
        lda     #1              ; (COLOR_WHITE)
        sta     _sm_ink
        lda     #<_look_how
        ldx     #>_look_how
        jmp     _sm_say
@no:    rts

; void look_step(void) -- the view a square on (the core has moved hx, hy
; in a place; the overworld's moves wx, wy).
_look_step:
        lda     #OVU_STEP
        jsr     _ov_use
        inc     _look_far
        bne     :+
        dec     _look_far
:       rts

; void look_after(void) -- the frame after a step: the overworld's people
; are shown again (w_after()), the party among them.
_look_after:
        lda     _looking
        beq     :+
        jmp     mark
:       rts

; void __fastcall__ look_key(unsigned char k) -- a key while looking (the
; directions are steps): SPACE the view back on the party, RUN/STOP or L
; that and the look done.
_look_key:
        ldx     #OVU_HOME
        cmp     #$20            ; SPACE
        beq     back
        ldx     #OVU_END
        cmp     #$03            ; RUN/STOP
        beq     back
        cmp     #$4C            ; L (PETSCII)
        beq     back
        rts
back:   stx     k
        .repeat 2, i
        lda     lk_wx+i
        sta     _wx+i
        .endrep
        lda     lk_wy
        sta     _wy
        lda     lk_hx
        sta     _hx
        lda     lk_hy
        sta     _hy
        lda     lk_dir
        sta     _dir
        lda     k
        jsr     _ov_use
        lda     #0
        sta     _look_far
        lda     k
        cmp     #OVU_END
        bne     @home
        lda     #0
        sta     _looking
        lda     #HERO_Y
        sta     $D001
        sta     $D00F
        lda     _area
        beq     :+
        lda     lk_n
        sta     _sm_npc_n
:       jsr     _st_step        ; the party's lines back
        jmp     @draw
@home:  jsr     mark
@draw:  lda     #NONE
        sta     _pre_dir
        jsr     _place_camera
        jsr     _sm_draw_all
        jsr     _sm_npcs
        lda     _sm_frame
        sta     _frame
        rts
