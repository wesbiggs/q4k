; Quest for the King -- the smooth-scrolling prototype's loader
; (include/q4k/ow.h). The bootstrap, src/openworld.c, uses it to start an
; engine, and an engine to go back to the bootstrap for another world. What
; only the bootstrap uses (the disk on the screen, the REU check) is in
; ow_boot.s, so that the engines, which have no room, do not carry it.

        .export _ow_run
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

        .segment "CODE"

; void __fastcall__ ow_run(const char *name) -- load a program over this
; one and start it. The code that does it has to be somewhere the load does
; not reach, so it is copied into the tape buffer first, with the name.
_ow_run:
        sta     ptr1
        stx     ptr1+1
        sei
        ldx     #stub_end - stub_src - 1
:       lda     stub_src,x
        sta     STUB,x
        dex
        bpl     :-
        ldy     #0
:       lda     (ptr1),y
        beq     :+
        sta     name,y
        iny
        cpy     #16
        bne     :-
:       sty     namelen
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
        lda     #1
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
        jsr     DISK_OFF        ; the KERNAL's interrupt, and no disk
        jmp     $080D           ; the program's start, past its BASIC line
fail:   inc     $D020
        jmp     fail
namelen: .byte  0
name:   .res    16
        .reloc
stub_end:

        .assert stub_end - stub_src <= DISK_SPR - STUB, error, "the loader runs into the disk's sprite"
