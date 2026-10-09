; Start-up code's home, and scratch arrays once it is done.
;
; The title screen, and what draws the game's character set, are code that
; runs once, before any fight or conversation; the fight's arrays hold nothing
; from one fight to the next and the dialogue's reply nothing from one turn to
; the next. So they can all be the same bytes: the OVL memory area at the
; bottom of HIGH (cfg/c64-q4k.cfg), loaded with the start-up code and then
; used as scratch. Otherwise it would cost two and a half kilobytes of HIGH
; and MAIN that nothing could use afterwards.
;
; Segments for it: TITLECODE and TITLEDATA (the Makefile puts title.o and
; charset_define.o in them; tools/mktiles.py marks the glyphs in tiles_data.c).
; Their total is 2524 bytes, and the arrays below add to 2540. The sizes are
; battle.c's and dialog.c's, and OVL's is their sum (__OVLSIZE__ in the cfg):
;
;       field    21 * 19 =  399
;       wall     23 * 21 =  483
;       roomy    23 * 21 =  483
;       seen     23 * 21 =  483
;       queue_x             256
;       queue_y             256
;       reply               180
;                         -----
;                          2540 = $09EC
;
; The rule that goes with it: nothing here may be touched until the start-up
; (main() up to the map's first draw) is done, and none of it may run again.
; The cfg will not link if start-up grows past the arrays: then grow one.

        .export _field, _wall, _roomy, _seen, _queue_x, _queue_y, _reply
        .import __OVL_START__

_field   = __OVL_START__
_wall    = __OVL_START__ + 399
_roomy   = __OVL_START__ + 399 + 483
_seen    = __OVL_START__ + 399 + 483 + 483
_queue_x = __OVL_START__ + 399 + 483 + 483 + 483
_queue_y = __OVL_START__ + 399 + 483 + 483 + 483 + 256
_reply   = __OVL_START__ + 2360
