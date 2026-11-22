; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the open world's REU load cache, laid out
; (ow_cache.s has the design). The bootstrap's alone: it calls
; ow_cinit() once it knows there is an REU, before its first load.
;
; If the REU's directory has the magic and this build's stamp (owc_sig, from
; tools/mkowcache.py, as everything else here), it is kept: N and F3 come
; back through the bootstrap, and a preloaded image (screendump.py
; --reucache) starts full. Otherwise it is laid out again, empty: the REU's
; size found (a byte written to the last address of each bank, highest
; first, then read back until a bank does not hold its own number: a
; smaller REU repeats itself), the directory cleared, each file given its
; entry and its room in the build's order while there is room, and the magic
; and the stamp written last.

        .export _ow_cinit
        .import owc_xfer, owc_xr, owc_sig, owc_n, owc_tab
        .importzp ptr1

FETCH   = $91
STASH   = $90
OW_REU  = $02A8                 ; OW->reu (q4k/ow.h): 0 none
OWC_BANK = 1
SIGLEN  = 6

        .segment "BSS"
sig:    .res SIGLEN
banks:  .res 1
probe:  .res 1
ent:    .res 8
nxt:    .res 3
left:   .res 1

        .segment "CODE"

_ow_cinit:
        lda     OW_REU
        bne     :+
        rts
:       ldx     #<sig           ; the directory's magic and stamp
        ldy     #>sig
        lda     #SIGLEN
        jsr     hdr
        lda     #FETCH
        jsr     owc_xfer
        ldx     #SIGLEN - 1
:       lda     sig,x
        cmp     owc_sig,x
        bne     anew
        dex
        bpl     :-
        rts                     ; this build's: kept

anew:   lda     #0              ; no magic until it is all there
        sta     sig
        sta     sig+1
        ldx     #<sig
        ldy     #>sig
        lda     #2
        jsr     hdr
        lda     #STASH
        jsr     owc_xfer

        ldx     #$FF            ; the size: each bank's number in it,
@put:   stx     probe           ; highest first
        jsr     at_top
        lda     #STASH
        jsr     owc_xfer
        ldx     probe
        dex
        cpx     #$FF
        bne     @put
        ldx     #0              ; then up from 0 until one does not hold its
@get:   stx     banks           ; own (or all 256 do)
        jsr     at_top
        lda     #FETCH
        jsr     owc_xfer
        ldx     banks
        cpx     probe
        bne     @size
        inx
        bne     @get
        dex                     ; (16M: 255 banks will do)
        stx     banks
@size:  lda     banks
        cmp     #OWC_BANK + 1   ; the directory's bank at least
        bcs     :+
        rts
:
        lda     #0              ; the directory cleared: 2K from one 0
        sta     probe
        lda     #<probe
        sta     owc_xr
        lda     #>probe
        sta     owc_xr+1
        lda     #0
        sta     owc_xr+2
        sta     owc_xr+3
        sta     owc_xr+5
        lda     #OWC_BANK
        sta     owc_xr+4
        lda     #$08
        sta     owc_xr+6
        lda     #$80            ; (the C64's address fixed)
        jsr     xfer_ctl

        lda     #$00            ; the rooms, from $0900 of the bank
        sta     nxt
        lda     #$09
        sta     nxt+1
        lda     #OWC_BANK
        sta     nxt+2
        lda     #<owc_tab
        sta     ptr1
        lda     #>owc_tab
        sta     ptr1+1
        lda     owc_n
        sta     left
@file:  ldy     #4              ; does it fit?
        lda     nxt
        clc
        adc     (ptr1),y
        iny
        lda     nxt+1
        adc     (ptr1),y
        lda     nxt+2
        adc     #0
        cmp     banks
        bcc     :+
        jmp     @next           ; (no: it loads from the disk every time)
:
        lda     nxt             ; its entry: its room, length, address,
        sta     ent             ; check (absent)
        lda     nxt+1
        sta     ent+1
        lda     nxt+2
        sta     ent+2
        ldy     #4
        lda     (ptr1),y
        sta     ent+3
        iny
        lda     (ptr1),y
        sta     ent+4
        ldy     #2
        lda     (ptr1),y
        sta     ent+5
        iny
        lda     (ptr1),y
        sta     ent+6
        ldy     #1
        lda     (ptr1),y
        sta     ent+7
        lda     #<ent
        sta     owc_xr
        lda     #>ent
        sta     owc_xr+1
        dey
        lda     (ptr1),y        ; at 8 times its hash
        ldx     #0
        stx     owc_xr+3
        asl     a
        rol     owc_xr+3
        asl     a
        rol     owc_xr+3
        asl     a
        rol     owc_xr+3
        sta     owc_xr+2
        lda     #OWC_BANK
        sta     owc_xr+4
        lda     #8
        sta     owc_xr+5
        lda     #0
        sta     owc_xr+6
        lda     #STASH
        jsr     owc_xfer
        clc                     ; the next room past it
        lda     nxt
        adc     ent+3
        sta     nxt
        lda     nxt+1
        adc     ent+4
        sta     nxt+1
        bcc     @next
        inc     nxt+2
@next:  lda     ptr1
        clc
        adc     #6
        sta     ptr1
        bcc     @dec
        inc     ptr1+1
@dec:   dec     left
        beq     :+
        jmp     @file
:

        ldx     #<owc_sig       ; and the magic and the stamp: ready
        ldy     #>owc_sig
        lda     #SIGLEN
        jsr     hdr
        lda     #STASH
        jmp     owc_xfer

; owc_xr: A bytes between Y:X and the header ($0800 of bank 1).
hdr:    stx     owc_xr
        sty     owc_xr+1
        sta     owc_xr+5
        lda     #$00
        sta     owc_xr+2
        sta     owc_xr+6
        lda     #$08
        sta     owc_xr+3
        lda     #OWC_BANK
        sta     owc_xr+4
        rts

; owc_xr: the byte `probe` and the last address of bank X.
at_top: lda     #<probe
        sta     owc_xr
        lda     #>probe
        sta     owc_xr+1
        lda     #$FF
        sta     owc_xr+2
        sta     owc_xr+3
        stx     owc_xr+4
        lda     #1
        sta     owc_xr+5
        lda     #0
        sta     owc_xr+6
        rts

; A stash with the address control A ($80: the C64's address fixed).
xfer_ctl:
        tay
        php
        sei
        ldx     #6
:       lda     owc_xr,x
        sta     $DF02,x
        dex
        bpl     :-
        lda     #0
        sta     $DF09
        sty     $DF0A
        lda     #STASH
        sta     $DF01
        lda     #0
        sta     $DF0A
        plp
        rts
