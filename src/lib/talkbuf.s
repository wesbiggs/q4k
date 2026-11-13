; The talk's buffer, at the end of the talk's engine (see overlay.s): the file
; of one person's lines, or one inn's rumours, which the offline build loads
; from the disk (src/lib/talk.c). TALK_SIZE comes from the Makefile, which
; tools/mktalk.py checks every file against. The networked build's dialogue
; keeps its reply in the core instead (line.c), because the title screen asks
; the host before any engine is loaded, so it does not link this.

        .export _talk

        .segment "TALKBSS"
_talk:  .res TALK_SIZE
