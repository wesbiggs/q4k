; SPDX-License-Identifier: GPL-3.0-or-later
; Quest for the King -- the smooth-scrolling prototype's loading icon: a
; sprite in the middle of a black screen while a program loads (ow_run.s
; calls it before every LOAD; the bootstrap calls it while it loads its
; files and builds the lands). It is one of three things, which whoever
; asks for it sets beforehand (include/q4k/ow.h's OW_ICON_*):
;
;  - a yellow floppy, blinking about twice a second: a load (OW_ICON_DISK,
;    what it always goes back to)
;  - a white twenty-sided die, rolling: the world being built (OW_ICON_ROLL)
;  - someone's own sprite, doubled, blinking in their colour: the monster a
;    fight is with, or the person a talk is with, out of the VIC bank the
;    engine leaves as it is until the LOAD (OW_ICON_FLASH)
;
; It is resident, whatever program is loaded: the bootstrap copies it once to
; where no program reaches. The code, its interrupt and its blink counter are
; in $CF14-$CFFF (OWDISK, cfg/c64-owboot.cfg), which the probe page and the
; engines' BSS do not use; the talk program's BSS2 starts there, but nothing
; in it writes its fov_lit while it runs, and no LOAD writes BSS. The disk's
; and the die's pictures are under the KERNAL, at $FF00 (OW-ICON, from
; assets/icons.txt), where nothing else is, which the bootstrap loads once.
; The icon copies two blocks from where it is told to sprite blocks 14 and 15
; ($0380-$03FF, OWDISKSPR), which are the only place the VIC sees in bank 0
; that no program's code is at (the tape buffer; the loader stub, ow_run.s,
; must end before it). The engines' C stack is in the tape buffer too, down
; from $0400, over block 15: but by the time the stub calls the icon, the
; program that had that stack is done with it, and the bootstrap's is
; elsewhere (at its MAIN's top).

        .export _ow_disk_on, _ow_disk_irq

DISK_SPR   = $0380              ; sprite block 14, as the sprite pointer says,
DISK_BLOCK = DISK_SPR / 64      ; and 15 after it: the die's other face
DISK_COL   = 7                  ; yellow
ICON_ART   = $FF00              ; OW-ICON: the disk, then the die's two faces
ICON_FLASH = $01                ; ow.h's OW_ICON_*: sprite 0 multicolour,
ICON_ROLL  = $80                ; and doubled; the two blocks in turn

; Two entries at fixed addresses, which ow_run.s calls: $CF14 shows the icon,
; $CF17 turns it off and the KERNAL's interrupt back on (once a load is done).
; Then what to show, at fixed addresses too (ow.h): what the icon is, the
; sprite's colour, and (in the copy's LDA) where its blocks are. disk_stop
; puts them back as they were, the disk, after every load.
        .segment "OWDISK"

_ow_disk_on:
        jmp     disk_show
disk_stop_entry:
        jmp     disk_stop
        .assert disk_stop_entry = $CF17, error, "ow_run.s calls the disk at $CF14 and $CF17"
icon_mode:
        .byte   0               ; OW_ICON_MODE: 0, ICON_FLASH or ICON_ROLL
icon_col:
        .byte   DISK_COL        ; OW_ICON_COL
        .assert icon_mode = $CF1A, error, "ow.h has the icon's mode at $CF1A"
        .assert icon_col = $CF1B, error, "ow.h has the icon's colour at $CF1B"

; void ow_disk_on(void) -- sprite 0 shows the icon, until disk_stop (after a
; load) or the program that is loaded takes its interrupt and sprites. It
; stands still while the lands are built with interrupts off (vault.s). The
; two blocks are copied with the KERNAL out ($01 = $35): the disk and the
; die are under it. (So are the CPU's vectors, but interrupts are off.)
disk_show:
        sei
        lda     $01
        pha
        lda     #$35
        sta     $01
        ldx     #127
icon_copy:
        lda     ICON_ART,x      ; (OW_ICON_SRC: this operand)
        sta     DISK_SPR,x
        dex
        bpl     icon_copy

        pla
        sta     $01
        lda     #DISK_BLOCK
        sta     $07F8           ; sprite 0's pointer, on the text screen
        ldx     #172            ; centred: the 20x20 picture is at columns 2-21
        ldy     #141
        lda     icon_mode       ; multicolour or not (bit 7 is sprite 7's:
        sta     $D01C           ; nothing), and doubled with it
        and     #ICON_FLASH
        sta     $D017
        sta     $D01D
        beq     :+
        ldx     #168            ; someone, 8 by 16 at the block's top left,
        ldy     #134            ; doubled: 32 by 32, centred
:       stx     $D000
        sty     $D001
        lda     #0              ; low X, in front
        sta     $D010
        sta     $D01B
        lda     icon_col
        sta     $D027
        lda     #$5B            ; extended colour and multicolour at once,
        sta     $D011           ; which the VIC draws as black, sprites and
        lda     #$D8            ; all: whatever the screen holds is hidden
        sta     $D016           ; (the engine's file loads code into it)
        lda     #1
        sta     $D015
        lda     #7              ; about thirty seconds: see _ow_disk_irq
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
        lda     #0              ; and the next load shows the disk
        sta     icon_mode
        sta     icon_copy + 1   ; (ICON_ART's low byte)
        lda     #>ICON_ART
        sta     icon_copy + 2
        lda     #DISK_COL
        sta     icon_col
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
; off, or with the die, the other face every eighth. It chains to the
; KERNAL's, which does the keyboard and the clock. It does not run forever:
; a soft reset during a load leaves the vector here, and the sprite would
; blink at READY. Every 256th interrupt counts down disk_left (7, about
; thirty seconds), and at zero it turns itself off.
_ow_disk_irq:
        inc     disk_n
        bne     :+
        dec     disk_left
        beq     @off
:       lda     disk_n
        ldx     icon_mode
        bpl     :+
        asl     a               ; the die: twice as often
:       and     #$0F
        bne     @kernal
        txa
        bmi     @roll
        lda     $D015           ; the disk, or someone: on or off
        eor     #1
        sta     $D015
@kernal:
        jmp     $EA31
@roll:  lda     $07F8           ; the die: blocks 14 and 15 in turn
        eor     #1
        sta     $07F8
        bpl     @kernal         ; (always)
@off:   jsr     disk_off
        jmp     $EA31

disk_n:    .byte 0
disk_left: .byte 0

; The resident code must fit below the probe page's last byte.
        .assert ow_disk_end - _ow_disk_on <= $CFFF - $CF14 + 1, error, "the disk icon runs into the probe page"
ow_disk_end:
