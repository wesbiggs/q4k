#include <conio.h>
#include <c64.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dialog.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/inn.h>
#include <q4k/panel.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

/* Read-only data goes in the charset's spare glyphs (CHDATA3 in the cfgs). */
#pragma rodata-name ("CHDATA3")

#define TITLE_Y   0
#define LIST_Y    3
#define PURSE_Y   6
#define TALK_Y    8     /* what happens, scrolling */
#define TALK_H   11
#define HINT_Y   20

/* Spelled out again in the menu in inn_open(); change both. */
#define ALE_PRICE    5
#define ROOM_PRICE  20

static char line[SCR_W + 1];

static void purse(void)
{
    char *p;

    p = str_lit(line, "You have ");
    p = str_num(p, gold);
    p = str_lit(p, " marks.");
    text_clear(0, PURSE_Y, SCR_W);
    text_put(0, PURSE_Y, line, COLOR_YELLOW);
}

/* Take the price, or say why not. */
static unsigned char pay(unsigned char price)
{
    if (gold < price) {
        panel_lit("You cannot afford that.", COLOR_LIGHTRED);
        return 0;
    }
    gold -= price;
    purse();
    return 1;
}

/* The hint comes from the dialogue daemon, or offline from the disk. Without
   a daemon you still get the ale -- it is only the talk that needs the line
   -- and the title screen has already said there is nobody to talk to. */
static void ale(unsigned char inn)
{
    if (!pay(ALE_PRICE)) {
        return;
    }
    panel_lit("You drink.", COLOR_GRAY2);
    if (dialog_hint(inn) != DIALOG_OK) {
        panel_lit("The keeper is busy.", COLOR_GRAY2);
    }
}

/* A bed for the whole party: everyone mended, and a wizard's magic back. */
static void room(void)
{
    unsigned char i;

    /* Checked before the price, so a rested party is never charged. */
    for (i = 0; i < party_count; ++i) {
        if (party[i].hp < party[i].hp_max || party[i].mp < party[i].mp_max) {
            break;
        }
    }
    if (i == party_count) {
        panel_lit("You are not weary.", COLOR_GRAY2);
        return;
    }
    if (pay(ROOM_PRICE)) {
        party_rest();
        panel_lit("You sleep, and wake mended.", COLOR_LIGHTGREEN);
    }
}

void inn_open(unsigned char inn)
{
    unsigned char key;

    dialog_forget();     /* the buffer may have been something else since */
    sprite_hide();
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(0, TITLE_Y, eng_text);      /* the map knows its name: engine.h */
    textcolor(COLOR_GRAY1);
    chlinexy(0, TITLE_Y + 1, SCR_W);
    chlinexy(0, HINT_Y - 1, SCR_W);

    textcolor(COLOR_WHITE);
    cputsxy(0, LIST_Y,     "  1  A mug of ale         5 marks");
    cputsxy(0, LIST_Y + 1, "  2  Beds for the night 20 marks");
    textcolor(COLOR_GRAY2);
    cputsxy(0, HINT_Y, "Number to buy, RUN/STOP leaves");

    panel_begin(TALK_Y, TALK_H);

    for (;;) {
        purse();

        key = cgetc();
        if (key == 0x03) {
            return;
        }
        if (key == 0x31) {
            ale(inn);
        } else if (key == 0x32) {
            room();
        }
    }
}
