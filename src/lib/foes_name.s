; SPDX-License-Identifier: GPL-3.0-or-later
;
; q4k -- a monster's name for however many there are: "A goblin" for one,
; "5 goblins" for five. The bestiary has each name once, with its article
; ("A wolf", "An ogre"); the plural is that without the article, after the
; count, with the few rules of English its names need: wolf, wolves; leech,
; leeches (but wraith, wraiths); the rest an s. A name with no article (a keeper's) never comes in
; numbers, and is given back as it is.
;
; For the fight module's opening line and owtown's screen after a fight
; (src/fight/fight.c, src/lib/ow_fight.c). In C it came to 269 bytes, more
; than the fight's autoplayer build had free.
;
; The names are cc65's PETSCII, so the letters here are numbers:
; 'A' $C1, the lowercase $41-$5A.
;
; void __fastcall__ foes_name(char *out, const char *name, unsigned char n);

        .export _foes_name
        .import popax
        .importzp ptr1, ptr2, tmp1

_foes_name:
        sta tmp1                ; n
        jsr popax
        sta ptr2                ; name
        stx ptr2+1
        jsr popax
        sta ptr1                ; out
        stx ptr1+1
        ldy #0
        lda tmp1
        cmp #2
        bcc copy                ; one: as it is
        lda (ptr2),y
        cmp #$C1                ; 'A'
        bne copy                ; (a keeper's name)
        lda tmp1
        ora #$30                ; the count, one digit (MAX_FOES)
        sta (ptr1),y
        inc ptr1
        bne :+
        inc ptr1+1
:       iny                     ; past "A" to the space ...
        lda (ptr2),y
        cmp #$20
        beq :+
        iny                     ; ... or past "An"
:       tya
        clc
        adc ptr2
        sta ptr2
        bcc :+
        inc ptr2+1
:       jsr copy                ; " goblin"
        dey
        lda (ptr1),y            ; its last letter
        cmp #$46                ; 'f': wolves
        bne :+
        lda #$56                ; 'v'
        sta (ptr1),y
        bne es
:       cmp #$53                ; 's'
        beq es
        cmp #$48                ; 'h' after 'c' or 's': leeches (wraiths)
        bne s
        dey
        lda (ptr1),y
        iny
        cmp #$43                ; 'c'
        beq es
        cmp #$53                ; 's'
        beq es
s:      iny
        lda #$53                ; 's'
        bne last
es:     iny
        lda #$45                ; 'e'
        sta (ptr1),y
        iny
        lda #$53                ; 's'
last:   sta (ptr1),y
        iny
        lda #0
        sta (ptr1),y
        rts

; name to out with its terminator; Y is left on the terminator.
copy:   ldy #0
:       lda (ptr2),y
        sta (ptr1),y
        beq :+
        iny
        bne :-
:       rts
