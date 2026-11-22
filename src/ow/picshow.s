; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the talk's portrait: vic2orious's v2i.inc and v2t.inc
; (assets/vic2/lib) for src/ow/talkpic.c.
;
; The picture is in VIC bank 1 ($4000-$7FFF: its screen, sprite blocks from
; $4400, the bitmap at $6000), which cfg/c64-owtalk.cfg leaves to it once
; the talk has started; the rest of the screen is the talk's words, drawn
; into the same bitmap by v2t.inc with the character ROM's letters
; (src/ow/vic2/font.inc, tools/mkv2tfont.py). While it is shown the KERNAL is out
; and v2i.inc's raster interrupt runs the picture's kernel, then the music
; (ow_play.s's owt_music_frame); keys come from the keyboard buffer or the
; matrix (pt_key).
;
; Zero page: the kernels write $02, which is why owtalk's cc65 zero page
; starts at $03 (cfg/c64-owtalk.cfg); v2i.inc and v2t.inc have 13 bytes
; each after it, in BASIC's, which nothing else here uses.
;
; Assembled -t none: the libraries compare the files' magic with ASCII.
; Linked into PICCODE, which the file brings at $4000 and owtalk.c copies
; up to where it runs, out of the bank.

V2I_BANK = 1
V2I_ZP   = $22
V2T_ZP   = $2F

        .export _pic_show, _pic_end, _pt_line, _pt_clear, _pt_key
        .export _pic_cols, _pt_fontw
        .export _pt_x, _pt_y, _pt_cols, _pt_color, _pt_col, _pt_row, _pt_rows
        .import _owt_music_frame

        .segment "PICCODE"

; unsigned char __fastcall__ pic_show(const void *v2i): the machine taken
; and the picture up, 0; or the machine given back and v2i.inc's error.
_pic_show:
        sta     pic
        stx     pic+1
        lda     #V2I_BANK
        sta     v2i_bank
        jsr     v2i_init
        lda     pic
        ldy     pic+1
        jsr     v2i_show
        bcs     @bad
        lda     #<_owt_music_frame
        sta     v2i_hook
        lda     #>_owt_music_frame
        sta     v2i_hook+1
        lda     #0              ; lines a character row apart, so that no
        sta     v2t_lead        ; two share a row's colours
        jsr     v2i_start
        lda     #0
        tax
        rts
@bad:   pha
        jsr     v2i_exit
        pla
        ldx     #0
        rts

; void pic_end(void): the machine back as pic_show() found it.
_pic_end:
        jsr     v2i_exit
        lda     #<v2i_rts
        sta     v2i_hook
        lda     #>v2i_rts
        sta     v2i_hook+1
        rts

; void __fastcall__ pt_line(const char *ascii): v2t_line at pt_x, pt_y.
_pt_line:
        pha
        txa
        tay
        pla
        jmp     v2t_line

; void __fastcall__ pt_clear(unsigned char colour): v2t_clear.
_pt_clear = v2t_clear

; unsigned char pt_key(void): a key, as cgetc() would give it, from the
; KERNAL's keyboard buffer (where the tests type) or else the matrix, which
; is read only for what the talk asks for: 1-9, 0 and RUN/STOP. A key is
; taken once it is down after none was.
_pt_key:
@wait:  lda     $C6             ; the buffer first
        beq     @scan
        ldx     $0277
        ldy     #0
@shift: lda     $0278,y
        sta     $0277,y
        iny
        cpy     $C6
        bne     @shift
        dec     $C6
        txa
        ldx     #0
        rts
@scan:  lda     v2i_ticks       ; once a frame
@tick:  cmp     v2i_ticks
        beq     @tick
        ldy     #KEYS - 1
@key:   lda     kcol,y
        sta     $DC00
        lda     $DC01
        and     krow,y
        beq     @down
        dey
        bpl     @key
        lda     #0              ; none down
        sta     held
        beq     @wait
@down:  lda     held
        bne     @wait           ; still the last one
        inc     held
        lda     kchr,y
        ldx     #0
        rts

KEYS = 11
kcol:   .byte $7F, $7F, $FD, $FD, $FB, $FB, $F7, $F7, $EF, $EF, $7F
krow:   .byte $01, $08, $01, $08, $01, $08, $01, $08, $01, $08, $80
kchr:   .byte $31, $32, $33, $34, $35, $36, $37, $38, $39, $30, $03
held:   .byte 1                 ; (as if one were down: none is taken until
pic:    .word 0                 ; all are up)

_pic_cols  = v2i_cols
_pt_fontw  = v2t_font_w
_pt_x      = v2t_x
_pt_y      = v2t_y
_pt_cols   = v2t_cols
_pt_color  = v2t_color
_pt_col    = v2t_col
_pt_row    = v2t_row
_pt_rows   = v2t_rows

        .include "v2i.inc"
        .include "v2t.inc"
