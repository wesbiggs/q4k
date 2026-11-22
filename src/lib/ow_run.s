; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the smooth-scrolling prototype's loader
; (include/q4k/ow.h). The bootstrap, src/openworld.c, uses it to start an
; engine, and an engine to go back to the bootstrap for another world. What
; only the bootstrap uses (the disk on the screen, the REU check) is in
; ow_boot.s, so that the engines, which have no room, do not carry it.

        .export _ow_run
        .import owc_look, owc_xr, owc_ent, owc_slot
        .importzp ptr1

; The resident disk icon (ow_disk.s, in the bootstrap's OWDISK segment): the
; bootstrap copies it there once, and no program loaded over it reaches it.
DISK_ON  = $CF14
DISK_OFF = $CF17

SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
STUB    = $0334                 ; the tape buffer, which nothing else is using
                                ; by then (an engine's C stack, but it never
                                ; returns to that)
DISK_SPR = $0380                ; ow_boot.s's sprite, also in the tape buffer
CSTUB   = $0100                 ; the bottom of the stack page, free by then
                                ; too (the stub's stack starts at the top, and
                                ; never comes near): the REU cache's part
                                ; (ow_cache.s), and the name
FETCH   = $91
STASH   = $90

        .segment "CODE"

; void __fastcall__ ow_run(const char *name) -- load a program over this
; one and start it. The code that does it has to be somewhere the load does
; not reach, so it is copied into the tape buffer first, with the name.
; With the REU cache (ow_cache.s) the program is looked up here, while this
; one is still in memory, and the stub is told what to do: fetch it from
; the REU, or load it and stash it there (and mark it there), or just load.
_ow_run:
        sta     ptr1
        stx     ptr1+1
        jsr     owc_look        ; C: the cache has a place for it
        lda     #0
        bcc     :+
        lda     owc_ent+7
        and     #1              ; 1 there: fetch it
        eor     #1
        asl     a               ; 2 not yet: load and stash it
        bne     :+
        lda     #1
:       pha
        sei
        ldx     #stub_end - stub_src - 1
:       lda     stub_src,x
        sta     STUB,x
        dex
        bpl     :-
        ldx     #cstub_end - cstub_src - 1
:       lda     cstub_src,x
        sta     CSTUB,x
        dex
        bpl     :-
        pla
        sta     cmode
        ldx     #6              ; the program's transfer
:       lda     owc_xr,x
        sta     cxr,x
        dex
        bpl     :-
        lda     owc_ent+7       ; and its mark, once it is there
        ora     #1
        sta     cmark
        lda     owc_slot
        ora     #7
        sta     cxr+9
        lda     owc_slot+1
        sta     cxr+10
        ldy     #0
:       lda     (ptr1),y
        beq     :+
        sta     name,y
        iny
        cpy     #16
        bne     :-
:       sty     namelen
        lda     #<nostop        ; RUN/STOP cannot break a load from here on
        sta     $0328           ; (the KERNAL's STOP vector: see nostop)
        lda     #>nostop
        sta     $0329
        jmp     stub

stub_src:
        .org    STUB
stub:   ldx     #$FF            ; nothing returns here: a fresh stack
        txs
        lda     #$00            ; MEMSIZ up to the probe page, as the game
        sta     $0283           ; has it before it touches the drive
        lda     #$CF
        sta     $0284
        cli
        jsr     DISK_ON         ; the disk icon, while it loads (ow_disk.s)
        jsr     cpre            ; in the REU (ow_cache.s): fetched, and on
retry:  lda     #1
        ldx     #8
        ldy     #1              ; to the file's own address
        jsr     SETLFS
        lda     namelen
        ldx     #<name
        ldy     #>name
        jsr     SETNAM
        lda     #0              ; load, not verify
        jsr     LOAD
        bcs     fail
        jsr     cpost           ; not yet in the REU: put there, at once
go:     jsr     DISK_OFF        ; the KERNAL's interrupt, and no disk
        jmp     $080D           ; the program's start, past its BASIC line
fail:   inc     $D020           ; (no disk: try again until there is)
        jmp     retry
; The KERNAL's STOP, as every program after this one has it: never pressed.
; LOAD gives up when it sees RUN/STOP held ($91), with no drive error, and
; the key that leaves a talk or a shop can still be down, or pressed again,
; while the next program loads. The open world reads RUN/STOP as a key
; ($03 from GETIN), which this does not touch.
nostop: lda     #1              ; Z clear: not pressed
        rts
        .reloc
stub_end:

        .assert stub_end - stub_src <= DISK_SPR - STUB, error, "the loader runs into the disk's sprite"

; The cache's part. cpre: a program in the REU fetched and started.
cstub_src:
        .org    CSTUB
cpre:   lda     cmode
        lsr     a
        bcc     back
        lda     #FETCH
        ldx     #0
        jsr     cgo
        jmp     go
; cpost: a program just loaded, and not yet in the REU, put there and
; marked there.
cpost:  lda     cmode
        beq     back
        lda     #STASH
        ldx     #0
        jsr     cgo
        lda     #STASH
        ldx     #7
; An REU transfer, A the command and X which (0 the
; program, 7 its mark), with interrupts off (a program's raster interrupt
; may still be the one running: the disk icon's chains to the KERNAL's).
; Everything it moves is below $A000, so the ROMs need not go.
cgo:    sei
        pha
        ldy     #0
:       lda     cxr,x
        sta     $DF02,y
        inx
        iny
        cpy     #7
        bne     :-
        lda     #0
        sta     $DF09
        sta     $DF0A
        pla
        sta     $DF01
        cli
back:   rts
namelen: .byte  0               ; (the name here: the tape buffer is full)
name:   .res    16
cmode:  .byte   0               ; 0 load, 1 fetch, 2 load and stash
cmark:  .byte   0
cxr:    .res    7               ; the program: C64, REU (3), length
        .word   cmark           ; its mark, from cmark to its entry's last
        .res    2               ; byte (bank 1), 1 byte
        .byte   1
        .word   1
        .reloc
cstub_end:
        .assert cstub_end - cstub_src <= $80, error, "the cache's loader runs into the stack"
