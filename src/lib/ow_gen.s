; SPDX-License-Identifier: GPL-3.0-or-later
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
        .export gen_on, gen_preempt, gen_d, gen_leave
        .import _ov_gen, _sm_frame
        .importzp sreg, ptr1, ptr2, ptr4, tmp1, tmp3

ZP_N    = 18                    ; sreg, regsave, ptr1-4, tmp1-4
GEN_SP  = $7F                   ; the thread's stack is $0100-$017F
SPLIT0  = $D6                   ; gen_run() starts nothing from here (a few
SPLIT1  = $E6                   ; lines before smooth.s's SPLITLINE, $DC) to
                                ; the text's interrupt (TEXTLINE, $E4) and
                                ; the line it waits for to pass ($E5)

        .segment "BSS"
_gen_state: .res 1              ; 0 nothing, 1 building, 3 copying into the
                                ; slot, 2 there
gen_on:     .res 1              ; the thread is the one running
gen_d:      .res 1
gen_sp:     .res 1
main_sp:    .res 1
gen_zp:     .res ZP_N

        .segment "CODE"

; void __fastcall__ gen_start(unsigned char d): begin on continent d (0-3),
; dropping whatever build was under way. Nothing is built until gen_run().
_gen_start:
        sta     gen_d
        lda     #>_ov_gen       ; a frame for gen_run() to return through:
        sta     $0100 + GEN_SP  ; Y, X, A, P (interrupts on), and where to
        lda     #<_ov_gen       ; (the build, in the world's overlay)
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
        lda     $D012           ; nothing now if the text window's split is
        cmp     #SPLIT0         ; near (smooth.s): the switch below keeps
        bcc     :+              ; interrupts off for four lines, which held
        cmp     #SPLIT1         ; the text's interrupt back past where it
        bcc     back            ; works, and a raster line of the map showed
:       sei                     ; over the text (the callers come again)
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
gen_leave:                      ; (the build's end: genbuild.s)
        lda     #0
        sta     gen_on
        ldx     main_sp
        txs
unlock: cli
back:   rts

; The thread itself, the build, is in the world's overlay: genbuild.s.
