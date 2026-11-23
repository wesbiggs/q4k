; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the open world's REU load cache (include/q4k/ow.h).
;
; A file is loaded from the disk the first time it is wanted, stashed in the
; REU at once, and fetched from there by DMA every time after: a 33K program
; in about 33 ms. Every program of the open world's that loads links this,
; and loads through ow_cload() (or ow_run.s, which uses owc_look) everything
; but OW-SAVE, which changes.
;
; Where things are in the REU is the build's decision (tools/mkowcache.py):
; bank 1 holds a directory of 256 entries of 8 bytes, at 8 times a hash of
; the name -- the REU address of the file's room (3 bytes), its length (2),
; where it loads (2), and a check byte from the name whose bit 0 says the
; file is there -- then the magic and the build's stamp at $0800, and the
; files' rooms from $0900. The bootstrap lays it out (ow_cinit.s) when the
; REU does not carry this build's stamp; here only the magic is looked at,
; and with no REU it is never there. Bank 0 is the engine's scrolling's.
; A name the build does not know (OW-SAVE, say) finds an entry whose check
; is not its own, and loads as it always did.
;
; A transfer has interrupts off from its setting up to its end, since the
; engine's raster interrupt uses the REU too (smooth.s); a long one is the
; caller's business (the screen is off, or the disk icon is up, for every
; load). A stash reads with the KERNAL banked out ($01 = $35), or it would
; read the ROM over a tune at $E7C0; the NMI's vector, read from the RAM
; under the KERNAL then, points at an RTI meanwhile.

        .export _ow_cload, _ow_cached, _cache_hits, _cache_misses
        .export owc_look, owc_xfer, owc_xr, owc_ent, owc_slot, owc_rti
        .import popax, __oserror
        .importzp ptr1

        .include "q4k/owcache.inc"

SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
REU_C64 = $DF02                 ; then the REU address, the length, the
REU_CMD = $DF01                 ; interrupt mask and the address control
FETCH   = $91                   ; execute now, no $FF00 trigger: REU to C64
STASH   = $90                   ; C64 to REU
OWC_BANK = 1
OWC_HDR = $0800

        .segment "BSS"
_cache_hits:    .res 2          ; (for --peek)
_cache_misses:  .res 2

        .segment "OWCBSS"       ; scratch: in an engine, past OW-DATA
at:     .res 2                  ; where the caller wants it, or 0
nlen:   .res 1                  ; the name's length
h:      .res 1                  ; its entry
ck:     .res 1                  ; its check
endp:   .res 2
owc_slot: .res 2                ; the entry, in bank 1
owc_ent: .res 8                 ; REU address, length, C64 address, check
magic:  .res 2
owc_xr: .res 9                  ; a transfer: C64, REU (3), length, mask,
                                ; control (both 0)

        .segment "CODE"

; unsigned int __fastcall__ ow_cload(const char *name, void *at): as
; cbm_load(name, 8, at), at 0 for the file's own address: the end address,
; or 0 and _oserror if it would not load.
_ow_cload:
        sta     at
        stx     at+1
        jsr     popax
        sta     ptr1
        stx     ptr1+1
        jsr     owc_look
        bcs     :+
        jmp     plain
:       lda     at              ; the transfer: to where it is wanted
        ora     at+1
        beq     :+
        lda     at
        sta     owc_xr
        lda     at+1
        sta     owc_xr+1
:       lda     owc_ent+7
        lsr     a
        bcs     :+
        jmp     miss
:       lda     #FETCH          ; there: fetched
        jsr     owc_xfer
        inc     _cache_hits
        bne     :+
        inc     _cache_hits+1
:       clc                     ; the end, as LOAD gives it
        lda     owc_xr
        adc     owc_xr+5
        tay
        lda     owc_xr+1
        adc     owc_xr+6
        tax
        tya
        rts

miss:   jsr     kload           ; not yet: loaded, and stashed at once,
        bcs     fail            ; before anything changes it
        stx     endp
        sty     endp+1
        lda     #STASH
        jsr     owc_xfer
        inc     owc_ent+7       ; and there from now on
        lda     #<(owc_ent+7)
        sta     owc_xr
        lda     #>(owc_ent+7)
        sta     owc_xr+1
        lda     owc_slot
        ora     #7
        sta     owc_xr+2
        lda     owc_slot+1
        sta     owc_xr+3
        lda     #OWC_BANK
        sta     owc_xr+4
        lda     #1
        sta     owc_xr+5
        lda     #0
        sta     owc_xr+6
        lda     #STASH
        jsr     owc_xfer
        inc     _cache_misses
        bne     done
        inc     _cache_misses+1
        bne     done

plain:  jsr     kload
        bcs     fail
        stx     endp
        sty     endp+1
done:   lda     endp
        ldx     endp+1
        rts
fail:   sta     __oserror
        lda     #0
        tax
        rts

; The KERNAL's LOAD, as cbm_load() calls it: C set and the error in A, or
; the end in X and Y.
kload:  lda     at
        ora     at+1
        cmp     #1              ; C: an address of its own
        lda     #0
        rol     a
        eor     #1              ; secondary address 1: the file's own
        tay
        lda     #0
        ldx     #8
        jsr     SETLFS
        lda     nlen
        ldx     ptr1
        ldy     ptr1+1
        jsr     SETNAM
        lda     #0
        ldx     at
        ldy     at+1
        jmp     LOAD

; unsigned char __fastcall__ ow_cached(const char *name): 1 if ow_cload()
; would fetch it from the REU, 0 if it would go to the disk.
_ow_cached:
        sta     ptr1
        stx     ptr1+1
        jsr     owc_look
        lda     #0
        tax
        bcc     :+
        lda     owc_ent+7
        and     #1
:       rts

; owc_look: the name at ptr1 hashed (nlen its length), the magic and its
; entry fetched, and the transfer that would fetch it set up in owc_xr. C
; set if the cache has a place for it; then bit 0 of owc_ent+7 says it is
; there. (tools/mkowcache.py's hashes() is this.)
owc_look:
        ldy     #0
        sty     h
        sty     ck
@name:  lda     (ptr1),y
        beq     @end
        tax
        clc
        adc     ck
        sta     ck
        lda     h               ; rotated a bit left
        asl     a
        adc     #0
        eor     (ptr1),y
        clc
        adc     #OWC_SALT
        sta     h
        iny
        bne     @name
@end:   sty     nlen
        lda     #0
        sta     magic           ; (with no REU nothing comes)
        sta     owc_slot+1
        lda     h               ; the entry: 8 bytes each
        asl     a
        rol     owc_slot+1
        asl     a
        rol     owc_slot+1
        asl     a
        rol     owc_slot+1
        sta     owc_slot
        ldx     #<magic
        ldy     #>magic
        lda     #<OWC_HDR
        sta     owc_xr+2
        lda     #>OWC_HDR
        sta     owc_xr+3
        lda     #2
        jsr     small
        ldx     #<owc_ent
        ldy     #>owc_ent
        lda     owc_slot
        sta     owc_xr+2
        lda     owc_slot+1
        sta     owc_xr+3
        lda     #8
        jsr     small
        ldx     #4              ; the file's: from its room, its length,
:       lda     owc_ent,x
        sta     owc_xr+2,x
        dex
        bpl     :-
        lda     owc_ent+5       ; to where it loads
        sta     owc_xr
        lda     owc_ent+6
        sta     owc_xr+1
        lda     magic
        cmp     #$51
        bne     @no
        lda     magic+1
        cmp     #$43
        bne     @no
        lda     ck
        and     #$7E
        ora     #$80
        eor     owc_ent+7
        and     #$FE            ; the same but for bit 0
        bne     @no
        sec
        rts
@no:    clc
        rts

; A bytes from bank 1 at owc_xr+2/3 to Y:X.
small:  stx     owc_xr
        sty     owc_xr+1
        sta     owc_xr+5
        lda     #OWC_BANK
        sta     owc_xr+4
        lda     #0
        sta     owc_xr+6
        lda     #FETCH
        ; (on into owc_xfer)

; owc_xfer: the transfer owc_xr describes, A the command (FETCH, STASH).
owc_xfer:
        php
        sei
        tay
        lda     #0
        sta     owc_xr+7
        sta     owc_xr+8
        ldx     #8
:       lda     owc_xr,x
        sta     REU_C64,x
        dex
        bpl     :-
        lda     $01
        pha
        lda     #$35            ; the RAM under the ROMs, the I/O kept
        sta     $01
        lda     $FFFA           ; (the NMI's vector, from that RAM)
        pha
        lda     $FFFB
        pha
        ; The vector is written with the KERNAL in, its two bytes never
        ; half and half while the CPU could read it: an NMI can come at
        ; any time (the engine's corners), and one between them once
        ; jumped through owc_rti's low byte and vault_nmi's high.
        lda     #$36            ; (the writes reach the RAM under it)
        sta     $01
        lda     #<owc_rti
        sta     $FFFA
        lda     #>owc_rti
        sta     $FFFB
        lda     #$35
        sta     $01
        sty     REU_CMD
        lda     #$36
        sta     $01
        pla
        sta     $FFFB
        pla
        sta     $FFFA
        pla
        sta     $01
        plp
        rts
owc_rti:
        rti                     ; an NMI while the KERNAL is out
