; Quest for the King -- the smooth-scrolling prototype's loader and REU
; check (include/q4k/ow.h). The bootstrap, src/openworld.c, uses both; an
; engine only ow_run(), to go back to the bootstrap for another world.

        .export _ow_run, _ow_reu_detect
        .importzp ptr1

SETLFS  = $FFBA
SETNAM  = $FFBD
LOAD    = $FFD5
STUB    = $0334                 ; the tape buffer, which nothing else is using
                                ; by then (an engine's C stack, but it never
                                ; returns to that)

REU_STATUS = $DF00
REU_CMD    = $DF01
REU_C64    = $DF02
REU_ADDR   = $DF04
REU_LEN    = $DF07
REU_ACTL   = $DF0A
STASH      = $90
FETCH      = $91
REU_TEST   = $0800              ; where in the REU the test goes

        .segment "BSS"
r_c64:      .res 2
probe_buf:  .res 8

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
        jmp     $080D           ; the program's start, past its BASIC line
fail:   inc     $D020
        jmp     fail
namelen: .byte  0
name:   .res    16
        .reloc
stub_end:

; unsigned char ow_reu_detect(void) -- OW_REU_OK if there is an REU and a
; transfer into colour RAM lands there; OW_NO_REU if there is none;
; OW_REU_BLIND if there is one but colour RAM did not take the transfer
; (half an REU path is worse than none). The test bytes are colour RAM's
; last 24, past the screen's thousand, where nothing shows.
_ow_reu_detect:
        ldx     #5              ; registers that read back what was written?
:       txa
        eor     #$A5
        sta     REU_STATUS,x
        dex
        cpx     #1
        bne     :-
        ldx     #2
:       txa
        eor     #$A5
        cmp     REU_STATUS,x
        bne     @none
        inx
        cpx     #6
        bne     :-
        sei
        ldx     #7              ; a pattern through it and back
:       txa
        eor     #$5A
        sta     probe_buf,x
        dex
        bpl     :-
        jsr     to_buf
        lda     #STASH
        jsr     reu_go
        ldx     #7
        lda     #0
:       sta     probe_buf,x
        dex
        bpl     :-
        jsr     to_buf
        lda     #FETCH
        jsr     reu_go
        cli
        ldx     #7
:       txa
        eor     #$5A
        cmp     probe_buf,x
        bne     @none
        dex
        bpl     :-

        sei                     ; now into colour RAM: 3, 12, 5, 10, ...
        ldx     #7
:       lda     ctest,x
        sta     probe_buf,x
        lda     #0
        sta     $DBE8,x
        dex
        bpl     :-
        jsr     to_buf
        lda     #STASH
        jsr     reu_go
        lda     #<$DBE8
        sta     r_c64
        lda     #>$DBE8
        sta     r_c64+1
        lda     #FETCH
        jsr     reu_go
        cli
        ldx     #7
:       lda     $DBE8,x
        and     #$0F
        cmp     ctest,x
        bne     @nocol
        dex
        bpl     :-
        lda     #1
        ldx     #0
        rts
@nocol: lda     #2
        ldx     #0
        rts
@none:  lda     #0
        tax
        rts

to_buf: lda     #<probe_buf
        sta     r_c64
        lda     #>probe_buf
        sta     r_c64+1
        rts

; Eight bytes between r_c64 and the REU's test address, the command in A.
reu_go: ldx     r_c64
        stx     REU_C64
        ldx     r_c64+1
        stx     REU_C64+1
        ldx     #<REU_TEST
        stx     REU_ADDR
        ldx     #>REU_TEST
        stx     REU_ADDR+1
        ldx     #0
        stx     REU_ADDR+2
        stx     REU_ACTL
        stx     REU_LEN+1
        ldx     #8
        stx     REU_LEN
        sta     REU_CMD
        rts

        .segment "RODATA"
ctest:  .byte   3, 12, 5, 10, 15, 0, 9, 6
