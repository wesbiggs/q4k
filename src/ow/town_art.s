; GENERATED from assets/town_tiles.txt and castle_tiles.txt by tools/mkoverworld.py -- do not edit.

; The places' own people (src/ow/town.c): 64 bytes a frame, in the
; town overlay, which is in the engine's VIC bank 1
; (cfg/c64-openworld.cfg's OVTART, aligned to a block), and the
; sprite pointer of each one's first frame.

        .export _king_frame
        .segment "OVTCODE"
_king_frame: .byte <((town_art - $4000) / 64 + 0)
        .segment "OVTART"
town_art:
        .byte $82,$82,$00,$AA,$AA,$00,$15,$54,$00,$1F,$F4,$00,$07,$D0,$00,$1A,$A4,$00,$6A,$A9,$00,$7A,$AD,$00,$6A,$A9,$00,$55,$55,$00,$6A,$A9,$00,$6A,$A9,$00,$6A,$A9,$00,$6A,$A9,$00,$15,$54,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00   ; king
        .byte $00,$00,$00,$82,$82,$00,$AA,$AA,$00,$15,$54,$00,$1F,$F4,$00,$07,$D0,$00,$1A,$A4,$00,$6A,$A9,$00,$7A,$AD,$00,$6A,$A9,$00,$55,$55,$00,$6A,$A9,$00,$6A,$A9,$00,$6A,$A9,$00,$6A,$A9,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00   ; king bobbing

; The hero's skin's blocks are at $7E80, the corners' at $7F00.
        .assert * <= $7E80, lderror, "town_art: over the skin's and the corners' sprites"
