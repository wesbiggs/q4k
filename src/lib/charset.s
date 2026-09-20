; q4k -- install a writable character set.
;
; The VIC-II can only draw glyphs from its current 16K bank, and the C64's
; character generator is ROM. To get custom tiles we copy the ROM font into
; RAM at $3800 and then patch our own glyphs over the codes we don't need,
; which keeps the letters and digits the status panel prints.
;
; Reading the character ROM means banking it in over I/O at $D000, so
; interrupts stay off for the duration -- an IRQ hitting while the CIAs are
; hidden would fetch its vector from character data and crash the machine.

        .export         _charset_install
        .importzp       ptr1, ptr2

CHARROM_LOWERCASE = $D800       ; $D000 = uppercase/graphics set, $D800 = mixed
CHARSET_RAM       = $3800
PAGES             = 8           ; 8 * 256 = 2K = 256 glyphs

.segment "CODE"

; void charset_install (void);
.proc _charset_install

        sei
        lda     $01
        pha                     ; remember the current bank configuration
        lda     #$33            ; RAM everywhere, character ROM at $D000
        sta     $01

        lda     #<CHARROM_LOWERCASE
        sta     ptr1
        lda     #>CHARROM_LOWERCASE
        sta     ptr1+1
        lda     #<CHARSET_RAM
        sta     ptr2
        lda     #>CHARSET_RAM
        sta     ptr2+1

        ldx     #PAGES
page:   ldy     #0
byte:   lda     (ptr1),y
        sta     (ptr2),y
        iny
        bne     byte
        inc     ptr1+1
        inc     ptr2+1
        dex
        bne     page

        pla
        sta     $01             ; I/O back where the rest of the code expects it
        cli
        rts

.endproc
