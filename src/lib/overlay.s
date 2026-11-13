; Scratch at the end of two engines: the fight's arrays here, and the talk's
; buffer in talkbuf.s (apart, so that a program with a fight and no talk
; links no buffer).
;
; Each engine is loaded into the slot (include/q4k/engine.h,
; cfg/c64-q4k.cfg), and while it runs nothing else is there, so whatever it
; needs only while it runs can follow its code: FIGHTBSS and TALKBSS. None of
; it holds anything from one fight, or one conversation, to the next.
;
;       field    21 * 19 =  399
;       wall     23 * 21 =  483
;       roomy    23 * 21 =  483
;       seen     23 * 21 =  483
;       queue_x             256
;       queue_y             256
;

        .export _field, _wall, _roomy, _seen, _queue_x, _queue_y

        .segment "FIGHTBSS"
_field:   .res 399
_wall:    .res 483
_roomy:   .res 483
_seen:    .res 483
_queue_x: .res 256
_queue_y: .res 256
