; Quest for the King -- the smooth-scrolling prototype's continents, built
; when they are wanted.
;
; The RAM under the KERNAL has room for one continent ($E000-$E7BF, vault.s);
; the rest of it is the music's. So an engine keeps the continent nearest
; the hero there and builds another with the game's generator (isle_gen.s)
; as it comes nearer: into local[], which nothing uses on the overworld, and
; then into the slot, a row at a time.
;
; Building takes a couple of seconds of 6502, in pieces of a tenth of a
; second, and a frame has a few milliseconds to spare: so it runs as a
; second thread. The main loop calls gen_run() while it waits for the next
; frame; that carries on with the build where it stopped, and the frame's
; interrupt takes the machine away from it again, so the scrolling never
; waits for it. If a continent is wanted before it is ready, the caller calls
; gen_run() until it is, and the world stands still that long.
;
; The thread has its own piece of the stack, the bottom half of page 1
; (nothing else goes that deep), and its own copy of what it uses of cc65's
; scratch zero page -- the generator keeps pointers there from one
; instruction to the next -- put away and brought back with it. It does not use the
; C stack, nor anything else of the main program's.
;
; How the interrupt takes over: it is the engine's raster interrupt, which
; ends in the KERNAL's ($EA31) and that in an RTI. With gen_on set, it
; pushes one more frame for that RTI first (gen_preempt, smooth.s jumps to
; it): the RTI then lands in gen_yield with interrupts off and the thread's
; own frame still on its stack, which is put away until the next gen_run().
; Before an engine has its interrupt nothing takes over, and a build runs
; through in one call.

        .export _gen_start, _gen_run, _gen_state
        .export gen_on, gen_preempt
        .import _isle_begin, _isle_work, _local
        .import vault_enter, vault_leave, _sm_frame
        .importzp sreg, ptr1, ptr2, ptr4, tmp1, tmp3

SLOT    = $E000                 ; the one continent kept (vault.s's slot 0)
ROWS    = 31                    ; ISLE_H
ZP_N    = 18                    ; sreg, regsave, ptr1-4, tmp1-4
GEN_SP  = $7F                   ; the thread's stack is $0100-$017F

        .segment "BSS"
_gen_state: .res 1              ; 0 nothing, 1 building, 3 copying into the
                                ; slot, 2 there
gen_on:     .res 1              ; the thread is the one running
gen_d:      .res 1
gen_sp:     .res 1
main_sp:    .res 1
gen_zp:     .res ZP_N
rows:       .res 1

        .segment "CODE"

        .assert <_local = 0, lderror, "ow_gen: local[] must start on a page"

; void __fastcall__ gen_start(unsigned char d): begin on continent d (0-3),
; dropping whatever build was under way. Nothing is built until gen_run().
_gen_start:
        sta     gen_d
        lda     #>entry         ; a frame for gen_run() to return through:
        sta     $0100 + GEN_SP  ; Y, X, A, P (interrupts on), and where to
        lda     #<entry
        sta     $0100 + GEN_SP - 1
        lda     #$20
        sta     $0100 + GEN_SP - 2
        lda     #GEN_SP - 6
        sta     gen_sp
        lda     #1
        sta     _gen_state
        rts

; void __fastcall__ gen_run(unsigned char frame): carry on with the build
; until the next frame's interrupt, or until it is done; nothing if there
; is none, or if the frame is no longer `frame` (sm_frame, as the caller
; last saw it): the interrupt has been since, the caller has a frame's work
; waiting, and a build begun now would run for the whole of that frame.
; Looked at with interrupts off, so there is no gap for it to come in.
_gen_run:
        tax
        lda     _gen_state
        lsr     a
        bcc     back
        sei
        cpx     _sm_frame
        bne     unlock
        tsx
        stx     main_sp
        ldx     #ZP_N - 1
:       lda     gen_zp,x
        sta     sreg,x
        dex
        bpl     :-
        ldx     gen_sp
        txs
        inc     gen_on
        pla
        tay
        pla
        tax
        pla
        rti

; From the interrupt, in place of its jump to $EA31, while gen_on is set.
gen_preempt:
        lda     #>gen_yield
        pha
        lda     #<gen_yield
        pha
        lda     #$24            ; interrupts off when it gets there
        pha
        pha                     ; (A, X and Y for $EA31 to pull: the thread's
        pha                     ; own are under this frame)
        pha
        jmp     $EA31

; Every frame the thread has run in pays for this, at its start, and on NTSC
; with the CPU scrolling a frame has nothing to spare: so only the ten bytes
; the generator uses (isle_gen.s: sreg, ptr1, ptr2, ptr4, tmp1, tmp3), and
; not in a loop.
.macro  keep    zp
        lda     zp
        sta     gen_zp + zp - sreg
        lda     zp+1
        sta     gen_zp + zp - sreg + 1
.endmacro

gen_yield:
        keep    sreg
        keep    ptr1
        keep    ptr2
        keep    ptr4
        lda     tmp1
        sta     gen_zp + tmp1 - sreg
        lda     tmp3
        sta     gen_zp + tmp3 - sreg
        tsx
        stx     gen_sp
leave:  lda     #0
        sta     gen_on
        ldx     main_sp
        txs
unlock: cli
back:   rts

        .segment "HICODE"       ; (split so that both halves fit an engine)

; The thread.
entry:  lda     gen_d
        jsr     _isle_begin
:       jsr     _isle_work
        bne     :-
        lda     #3              ; the slot is neither land from here on
        sta     _gen_state
        lda     #<_local
        sta     ptr1
        lda     #>_local
        sta     ptr1+1
        lda     #<SLOT
        sta     ptr2
        lda     #>SLOT
        sta     ptr2+1
        lda     #ROWS
        sta     rows
@row:   jsr     vault_enter     ; interrupts off for a row: half a millisecond
        ldy     #63
:       lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     :-
        jsr     vault_leave
        lda     ptr1
        clc
        adc     #64
        sta     ptr1
        sta     ptr2            ; (both start on a page)
        bcc     :+
        inc     ptr1+1
        inc     ptr2+1
:       dec     rows
        bne     @row
        sei
        lda     #2
        sta     _gen_state
        jmp     leave
