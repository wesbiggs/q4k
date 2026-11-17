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
        .export _ov_idle, _ov_look
        .import __SLOTW_START__, __SLOTT_START__, __SLOTD_START__
        .assert __SLOTW_START__ = __SLOTT_START__, lderror, "ovl: the overlays must share the slot"
        .assert __SLOTW_START__ = __SLOTD_START__, lderror, "ovl: the overlays must share the slot"

        .import _w_enter, _w_can_step, _w_arrived, _w_after, _w_idle
        .import _t_enter, _t_arrived, _t_bump, _t_can_step, _place_step, _t_idle
        .import _w_bump, _w_look, _t_look, _d_look
        .import _d_enter, _d_arrived, _d_bump, _d_idle

_ov_enter       = __SLOTW_START__ + 0
_ov_can_step    = __SLOTW_START__ + 3
_ov_arrived     = __SLOTW_START__ + 6
_ov_bump        = __SLOTW_START__ + 9
_ov_after       = __SLOTW_START__ + 12
_ov_idle        = __SLOTW_START__ + 15
_ov_look        = __SLOTW_START__ + 18

        .import __VICTOP_START__
        .segment "OWHIADDR"     ; the engine's code past the screens: %O.hi
        .word   __VICTOP_START__

        .segment "OVWADDR"
        .word   __SLOTW_START__
        .segment "OVWHEAD"
        jmp     _w_enter
        jmp     _w_can_step
        jmp     _w_arrived
        jmp     _w_bump
        jmp     _w_after
        jmp     _w_idle
        jmp     _w_look

        .segment "OVTADDR"
        .word   __SLOTW_START__
        .segment "OVTHEAD"
        jmp     _t_enter
        jmp     _t_can_step
        jmp     _t_arrived
        jmp     _t_bump
        jmp     t_nothing       ; no window to stream: never called
        jmp     _t_idle         ; the people idle
        jmp     _t_look
t_nothing:
        rts

        .segment "OVDADDR"
        .word   __SLOTW_START__
        .segment "OVDHEAD"
        jmp     _d_enter
        jmp     _place_step
        jmp     _d_arrived
        jmp     _d_bump         ; a monster, or the keeper
        jmp     d_nothing       ; (never called: as the town's)
        jmp     _d_idle         ; the monsters
        jmp     _d_look
d_nothing:
        rts
