; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the smooth-scrolling prototype's disk icon: a yellow
; floppy in the middle of the screen while a program loads (ow_run.s calls
; it before every LOAD; the bootstrap calls it while the splash waits).
;
; It is resident, whatever program is loaded: the bootstrap copies it once to
; where no program reaches. The code, its interrupt and its blink counter are
; in $CF14-$CFFF (OWDISK, cfg/c64-owboot.cfg), which the probe page and the
; engines' BSS do not use; the talk program's BSS2 starts there, but nothing
; in it writes its fov_lit while it runs, and no LOAD writes BSS. The sprite
; itself is at $0380 (OWDISKSPR), block 14, which is the only place the VIC
; sees in bank 0 that no program's code is at (the tape buffer; the loader
; stub, ow_run.s, must end before it). The engines' C stack is in the tape
; buffer too, down from $0400, and does not reach it.

        .export _ow_disk_on, _ow_disk_irq

DISK_SPR   = $0380              ; sprite block 14, as the sprite pointer says
DISK_BLOCK = DISK_SPR / 64
DISK_COL   = 7                  ; yellow

; Two entries at fixed addresses, which ow_run.s calls: $CF14 shows the disk,
; $CF17 turns it off and the KERNAL's interrupt back on (once a load is done).
        .segment "OWDISK"

_ow_disk_on:
        jmp     disk_show
disk_stop_entry:
        jmp     disk_stop
        .assert disk_stop_entry = $CF17, error, "ow_run.s calls the disk at $CF14 and $CF17"

; void ow_disk_on(void) -- sprite 0 shows the disk, blinking about twice a
; second, until disk_stop (after a load) or the program that is loaded takes
; its interrupt and sprites. It stands still while the lands are built, with
; interrupts off (vault.s).
disk_show:
        sei
        ldx     #DISK_SPR_LEN - 1
:       lda     disk_spr,x
        sta     DISK_SPR,x
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
        lda     #$5B            ; extended colour and multicolour at once,
        sta     $D011           ; which the VIC draws as black, sprites and
        lda     #$D8            ; all: whatever the screen holds is hidden
        sta     $D016           ; (the engine's file loads code into it)
        lda     #1
        sta     $D015
        lda     #120            ; about thirty seconds: see _ow_disk_irq
        sta     disk_left
        lda     #<_ow_disk_irq
        sta     $0314
        lda     #>_ow_disk_irq
        sta     $0315
        cli
        rts

; The KERNAL's interrupt again and the sprite off: the load is done. In the
; bottom border: the screen's mode changes at once, but turning it off only
; takes from the next frame, so mid-frame the rest of this one would show
; the text screen, with whatever was loaded there, for a flash (from a talk
; or a fight back to the engine, whose file starts in it).
disk_stop:
        sei
:       bit     $D011           ; line 256 on: below the screen, PAL and NTSC
        bpl     :-
        jsr     disk_off
        lda     $D011           ; the program just loaded starts with the screen
        and     #$EF            ; off (DEN): it turns it on when it is ready
        sta     $D011
        cli
        rts

; The interrupt back to the KERNAL's, and the sprite off (no SEI: this runs
; in the interrupt too, from _ow_disk_irq).
disk_off:
        lda     $D011           ; the screen as the KERNAL has it again
        and     #$BF
        sta     $D011
        lda     #$C8
        sta     $D016
        lda     #$31
        sta     $0314
        lda     #$EA
        sta     $0315
        lda     #0
        sta     $D015
        rts

; The KERNAL's interrupt, with the sprite blinking every sixteenth one, on or
; off. It chains to the KERNAL's, which does the keyboard and the clock.
; It does not run forever: a soft reset during a load leaves the vector
; here, and the sprite would blink at READY. Every sixteenth interrupt
; counts down disk_left (120, about thirty seconds), and at zero it turns
; itself off.
_ow_disk_irq:
        inc     disk_n
        lda     disk_n
        and     #$0F
        bne     :+
        dec     disk_left
        beq     @off
        lda     $D015
        eor     #1
        sta     $D015
:       jmp     $EA31
@off:   jsr     disk_off
        jmp     $EA31

disk_n:    .byte 0
disk_left: .byte 0

; The picture: a 5.25" floppy (vic2orious's, from its asm/diskshow.s).
disk_spr:
        .byte $1f,$ff,$f8,$3f,$ff,$fc,$3f,$ff,$fc,$3f,$ff,$fc,$3f,$ff,$f8,$3f,$ff,$f8
        .byte $3f,$c3,$f8,$3f,$81,$fc,$3f,$81,$fc,$3f,$81,$fc,$3f,$81,$fc,$39,$c3,$fc
        .byte $39,$ff,$fc,$3f,$e7,$fc,$3f,$c3,$fc,$3f,$c3,$fc,$3f,$c3,$fc,$3f,$c3,$fc
        .byte $3f,$e7,$fc,$1f,$ff,$f8,$00,$00,$00
DISK_SPR_LEN = * - disk_spr

; The resident code must fit below the probe page's last byte, and the sprite
; in its block.
        .assert ow_disk_end - _ow_disk_on <= $CFFF - $CF14 + 1, error, "the disk icon runs into the probe page"
        .assert DISK_SPR_LEN = 63, error, "the disk icon's picture is 63 bytes"
ow_disk_end:
