; Quest for the King -- the smooth-scrolling prototype's bootstrap's own:
; the disk it shows while it works, and its REU check (include/q4k/ow.h).
; Apart from ow_run.s, the loader, which the engines use too.

        .export _ow_reu_detect, _ow_disk_on

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

; void ow_disk_on(void) -- a yellow floppy disk in the middle of the screen
; (sprite 0; the picture is vic2orious's, from its asm/diskshow.s), blinking
; about twice a second, to say that the machine is busy: the bootstrap shows
; it from the splash screen's key until the engine has the screen. It has to
; outlast the bootstrap, since the engine is loaded over that, so the sprite
; and the interrupt that blinks it are in the tape buffer, past the loader
; (ow_run.s): the KERNAL's own interrupt, which goes on through a LOAD, with a
; few instructions in front. It stands still while the lands are built,
; when interrupts are off (vault.s). An engine's own interrupt takes the
; vector and the sprites when it starts, and that is the end of it.
DISK_SPR   = $0380              ; sprite block 14
DISK_IRQ   = $03C0
DISK_BLOCK = DISK_SPR / 64
DISK_COL   = 7                  ; yellow

_ow_disk_on:
        sei
        ldx     #62
:       lda     disk_spr,x
        sta     DISK_SPR,x
        dex
        bpl     :-
        ldx     #disk_irq_end - disk_irq_src - 1
:       lda     disk_irq_src,x
        sta     DISK_IRQ,x
        dex
        bpl     :-
        lda     #DISK_BLOCK
        sta     $07F8           ; sprite 0's pointer, on the text screen
        lda     #172            ; centred: the 20x20 picture is at columns 2-21
        sta     $D000
        lda     #141
        sta     $D001
        lda     #0              ; low X, unexpanded, hires, in front
        sta     $D010
        sta     $D017
        sta     $D01B
        sta     $D01C
        sta     $D01D
        lda     #DISK_COL
        sta     $D027
        lda     #1
        sta     $D015
        lda     #<DISK_IRQ
        sta     $0314
        lda     #>DISK_IRQ
        sta     $0315
        cli
        rts

disk_irq_src:
        .org    DISK_IRQ
        inc     disk_n          ; every sixteenth interrupt, on or off
        lda     disk_n
        and     #$0F
        bne     :+
        lda     $D015
        eor     #1
        sta     $D015
:       jmp     $EA31
disk_n: .byte   0
        .reloc
disk_irq_end:

disk_spr:                       ; a 5.25" floppy
        .byte $1f,$ff,$f8,$3f,$ff,$fc,$3f,$ff,$fc,$3f,$ff,$fc,$3f,$ff,$f8,$3f,$ff,$f8
        .byte $3f,$c3,$f8,$3f,$81,$fc,$3f,$81,$fc,$3f,$81,$fc,$3f,$81,$fc,$39,$c3,$fc
        .byte $39,$ff,$fc,$3f,$e7,$fc,$3f,$c3,$fc,$3f,$c3,$fc,$3f,$c3,$fc,$3f,$c3,$fc
        .byte $3f,$e7,$fc,$1f,$ff,$f8,$00,$00,$00

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
