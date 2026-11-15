; Quest for the King -- the land and town generators' random numbers: their
; own LFSR, so that neither touches rand() or the encounter seed. A module of
; its own so that the towns' generator (tgen.s) can have it without the
; land's, which the smooth-scrolling prototype's engines leave to their
; bootstrap.

        .export isle_rnd, isle_seed

        .segment "BSS"
isle_seed:      .res 2

        .segment "MAPCODE"           ; the map's engine: engine.h

; A = a random byte: eight bits of a Galois LFSR, taps $B400, whose period is
; 65535. Clobbers X.
isle_rnd:
        ldx #8
@bit:   lsr isle_seed+1
        ror isle_seed
        bcc @no
        lda isle_seed+1
        eor #$B4
        sta isle_seed+1
@no:    dex
        bne @bit
        lda isle_seed
        rts
