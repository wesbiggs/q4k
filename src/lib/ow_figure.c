/*
 * Quest for the King -- a member's figure on a screen of text (q4k/owfig.h):
 * owtown's party screen and owparty's.
 */

#include <c64.h>
#include <conio.h>
#include <peekpoke.h>

#include <q4k/actor.h>
#include <q4k/owfig.h>
#include <q4k/owgame.h>

extern const unsigned char ow_figs[];

static unsigned char fig_bob;

void fig_colours(unsigned char who)
{
    VIC.spr0_color = OWG_DRESS(G->tunic[who]);
    VIC.spr1_color = OWG_SKIN(G->tunic[who]);
}

void fig_show(unsigned char who, unsigned char col, unsigned char row)
{
    unsigned int x = 24 + col * 8;

    VIC.spr0_x      = (unsigned char)x;
    VIC.spr0_y      = 50 + row * 8;
    VIC.spr1_x      = VIC.spr0_x;       /* their skin, hi-res, over it */
    VIC.spr1_y      = VIC.spr0_y;
    VIC.spr_hi_x    = x > 255 ? 3 : 0;
    fig_colours(who);
    VIC.spr_mcolor0 = COLOR_GRAY1;      /* the outline: black on the map and
                                           in a fight, but this is black */
    VIC.spr_mcolor  = 1;
    VIC.spr_exp_x   = 3;
    VIC.spr_exp_y   = 3;
    fig_bob = 0xFF;                     /* (fig_key() sets the frame) */
    VIC.spr_ena     = 3;
}

unsigned char fig_key(unsigned char who)
{
    unsigned char b;

    while (!kbhit()) {
        b = (PEEK(0xA2) >> 4) & 1;
        if (b != fig_bob) {
            fig_bob = b;
            b += (unsigned char)((unsigned int)ow_figs >> 6)
                 + (party[who].cls << 1);
            POKE(0x07F8, b);
            POKE(0x07F9, b + 6);        /* (the skins follow the figures) */
        }
    }
    return cgetc();
}

unsigned char fig_recolour(unsigned char who, unsigned char key)
{
    unsigned char t = G->tunic[who], c = OWG_DRESS(t);

    if (key == K_CRSR_DOWN) {
        c = c == 15 ? 1 : c + 1;
    } else if (key == K_CRSR_UP) {
        c = c <= 1 ? 15 : c - 1;
    } else if ((key & 0x7F) == 'k') {
        c = OWG_SKIN(t);
        c = c == 15 ? 1 : c + 1;
        G->tunic[who] = (c << 4) | OWG_DRESS(t);
        fig_colours(who);
        return 1;
    } else {
        return 0;
    }
    G->tunic[who] = (t & 0xF0) | c;
    fig_colours(who);
    return 1;
}

void fig_off(void)
{
    VIC.spr_ena = 0;            /* (the disk's icon, ow_disk.s, sets the
                                   rest of the VIC's sprites as it wants) */
}
