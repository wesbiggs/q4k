;
; Open world: the overlays' jump tables (engine.h).
;
; The world's overlay (world.c), the town's (town.c) and the dungeon's
; (dungeon.c) take turns in one slot (cfg/c64-openworld.cfg's SLOT). Each starts with a table of jumps in
; the same order, and the core calls an overlay only through the labels
; below, at those addresses, so it reaches whichever one is loaded. Each
; overlay is a file of its own (ld65 writes them beside the engine), whose
; first two bytes are its load address, as the KERNAL's LOAD wants.
;

        .export _ov_enter, _ov_can_step, _ov_arrived, _ov_bump, _ov_after
        .export _ov_idle
        .import __SLOTW_START__, __SLOTT_START__, __SLOTD_START__
        .assert __SLOTW_START__ = __SLOTT_START__, lderror, "ovl: the overlays must share the slot"
        .assert __SLOTW_START__ = __SLOTD_START__, lderror, "ovl: the overlays must share the slot"

        .import _w_enter, _w_can_step, _w_arrived, _w_after, _w_idle
        .import _t_enter, _t_arrived, _t_bump, _place_step
        .import _d_enter, _d_arrived

_ov_enter       = __SLOTW_START__ + 0
_ov_can_step    = __SLOTW_START__ + 3
_ov_arrived     = __SLOTW_START__ + 6
_ov_bump        = __SLOTW_START__ + 9
_ov_after       = __SLOTW_START__ + 12
_ov_idle        = __SLOTW_START__ + 15

        .import __VICTOP_START__
        .segment "OWHIADDR"     ; the engine's code past the screens: %O.hi
        .word   __VICTOP_START__

        .segment "OVWADDR"
        .word   __SLOTW_START__
        .segment "OVWHEAD"
        jmp     _w_enter
        jmp     _w_can_step
        jmp     _w_arrived
        jmp     w_nothing       ; a step into anything is only a wall
        jmp     _w_after
        jmp     _w_idle
w_nothing:
        rts

        .segment "OVTADDR"
        .word   __SLOTW_START__
        .segment "OVTHEAD"
        jmp     _t_enter
        jmp     _place_step
        jmp     _t_arrived
        jmp     _t_bump
        jmp     t_nothing       ; no window to stream: never called
t_nothing:
        rts                     ; (ov_idle: nor is this)

        .segment "OVDADDR"
        .word   __SLOTW_START__
        .segment "OVDHEAD"
        jmp     _d_enter
        jmp     _place_step
        jmp     _d_arrived
        jmp     d_nothing       ; a step into anything is only a wall
        jmp     d_nothing       ; (never called: as the town's)
d_nothing:
        rts
