; The fight's scratch arrays, and the title screen's home.
;
; The title screen is code that runs once, before any fight, and the fight's
; arrays hold nothing from one fight to the next, so they can be the same
; bytes: the OVL memory area at the bottom of HIGH (cfg/c64-q4k.cfg), loaded
; with the title and then used as scratch. The title would otherwise cost
; two kilobytes of HIGH that nothing could use afterwards.
;
; The sizes are battle.c's, and OVL's is their sum (__OVLSIZE__ in the cfg):
;
;       field    21 * 19 =  399
;       wall     23 * 21 =  483
;       roomy    23 * 21 =  483
;       seen     23 * 21 =  483
;       queue_x             256
;       queue_y             256
;                         -----
;                          2360 = $0938
;
; The rule that goes with it: nothing here may be touched until the title
; screen is done, and the title screen may not run again.

        .export _field, _wall, _roomy, _seen, _queue_x, _queue_y
        .import __OVL_START__

_field   = __OVL_START__
_wall    = __OVL_START__ + 399
_roomy   = __OVL_START__ + 399 + 483
_seen    = __OVL_START__ + 399 + 483 + 483
_queue_x = __OVL_START__ + 399 + 483 + 483 + 483
_queue_y = __OVL_START__ + 399 + 483 + 483 + 483 + 256
