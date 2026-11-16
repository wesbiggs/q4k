; Quest for the King -- the smooth-scrolling prototype's bootstrap's own: its
; REU check (include/q4k/ow.h). The disk icon it shows while it works is in
; ow_disk.s, which is resident; apart from ow_run.s, the loader, which the
; engines use too.

        .export _ow_reu_detect

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
