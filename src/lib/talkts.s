; SPDX-License-Identifier: GPL-3.0-or-later
; q4k -- the game's names for the text screen's talk (q4k/talkscr.h).
;
; talk.c calls ts_*; in the game they are talkscr.c's tsc_*, and nothing
; else. A module of their own so that owtalk, which defines ts_* itself and
; calls tsc_* for a person with no portrait, links talkscr.o without these.

        .export _ts_open, _ts_ready, _ts_ask, _ts_say, _ts_topic, _ts_key
        .export _ts_close
        .import _tsc_open, _tsc_ask, _tsc_say, _tsc_topic, _tsc_key

        .segment "TALKCODE"

_ts_open    = _tsc_open
_ts_ask     = _tsc_ask
_ts_say     = _tsc_say
_ts_topic   = _tsc_topic
_ts_key     = _tsc_key

_ts_ready:                      ; (unsigned char): nothing to do
_ts_close:
        rts
