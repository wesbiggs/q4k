; SPDX-License-Identifier: GPL-3.0-or-later
; The RAM under the ROMs: getting there and back. vault.s has what is kept
; there and the copies in and out; the land generator, the music and the
; smooth-scrolling prototype's engines use these alone, which is why they
; are a module of their own (the engines have no room for the rest).

        .export vault_enter, vault_leave, vault_nmi

BANK    = $34           ; all RAM

        .segment "BSS"

kept:   .res 1          ; $01 as it was

        .segment "HICODE"

; Interrupts off, all RAM. Preserves X and Y.
vault_enter:
        sei
        lda $01
        sta kept
        lda #<vault_nmi         ; before the KERNAL goes, so that an NMI
        sta $FFFA               ; never reads half of one vector and half
        lda #>vault_nmi         ; of another (the writes reach the RAM
        sta $FFFB               ; under it)
        lda #BANK
        sta $01
        rts

vault_leave:
        lda kept
        sta $01
        cli
        rts

; An NMI while the KERNAL is banked out: its vector is read from the RAM
; under it, which points here. Bank the KERNAL and I/O in, as cc65 keeps them
; ($01 = %x110), run the KERNAL's handler through its vector at $0318 as if
; the NMI had gone there, and come back here, by the return address pushed
; for its RTI, to put $01 back. The modem's RS-232 is NMI-driven: without
; this a byte arriving while a tune plays would be lost, or worse. The
; RESTORE key gets the KERNAL's handling too, as it does anywhere else.
vault_nmi:
        pha
        lda $01
        pha
        and #$F8
        ora #$06
        sta $01
        lda #>@back             ; a frame for the handler's RTI to return to
        pha
        lda #<@back
        pha
        php
        jmp ($0318)
@back:  pla
        sta $01
        pla
        rti
