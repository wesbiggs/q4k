/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * The open world's fights: the multicolour fight module (q4k/fight.h,
 * src/fight/), called from owtown (src/owtown.c) when the engine asks for
 * one (OWB_FIGHT).
 *
 * What turns up is the game's choice (battle.c's battle_wander(): the
 * party's level, a dungeon's depth, and over the sea a quarter of the time
 * the land's own), on the field for the ground it happened on; a keeper
 * is fought in a dungeon room and Maldrath in his lair, with no gap in the
 * sides; the dead, when L wakes them, as battle_undead() has them. The pay, the levels, the stones and waking in Millford are the
 * game's too (battle.c's award(), q4k.c's keeper_beaten() and died()).
 *
 * The module is loaded at $5400 and its field and sprites below it, over
 * owtown's own code and constants up there (cfg/c64-owtown.cfg's HIGH): so
 * everything that runs after the module is here, in CODE, which is below
 * $3800, and reads nothing of HIGH's. What the fight needs of the bestiary
 * (HIGH too) is copied out before the module is loaded.
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/fight.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/shop.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* The monster, out of the bestiary before the module is loaded over it. */
static Monster foe;
static char foe_name[FIGHT_NAME];
static unsigned char foes;

/* battle.c's: the experience each level wants, and what a level gives. */
static const unsigned int level_at[] = {
    0, 20, 50, 100, 200, 400, 700, 1200, 1800, 2500,
};
#define MAX_LEVEL (sizeof(level_at) / sizeof(level_at[0]))
static const unsigned char hp_base[NUM_CLASSES] = { 22, 18, 16 };
static const unsigned char hp_step[NUM_CLASSES] = {  7,  5,  5 };

static const unsigned char boss_stage[NUM_DUNGEONS] = {
    Q_EMBER, Q_TEAR, Q_HEART,
};
static const unsigned char prize[NUM_DUNGEONS] = {
    IT_EMBER, IT_TEAR, IT_HEART,
};
static const char *const prize_names[NUM_DUNGEONS] = {
    "the Emberstone", "the Silver Tear", "the Dragon's Heart",
};

/* The choosing runs before the module is loaded, so it can be up in
   HIGH with the bestiary: MAIN is full. */
#pragma code-name (push, "HICODE")

static void pick(const Monster *m, unsigned char n)
{
    foe = *m;
    strncpy(foe_name, m->name, FIGHT_NAME - 1);
    foes = n;
}

/* battle_wander(), as far as choosing goes: or, for a dungeon's monster
   (`kind` not NONE), the kind it was placed as (owgame.h's OWG_FOE_KIND),
   with its numbers rolled now. */
#define NONE 0xFF

static void pick_wanderer(unsigned char in_dungeon, unsigned char kind)
{
    unsigned char top, i, most, lvl, s;

    if (kind == NONE ? land != ISLE_HOME && !(rand() & 3)
                     : kind >= OWG_NATIVE) {
        pick(&natives[kind == NONE ? OW->ibiome[land] * 2 + (rand() & 1)
                                   : kind - OWG_NATIVE],
             1 + (rand() & 1));
        for (s = slot_of(land); s; --s) {
            foe.hp   += 10;
            foe.atk  += 3;
            foe.def  += 1;
            foe.xp   += 10;
            foe.gold += 8;
            if (foe.reach) {
                foe.shot += 3;
            }
        }
        return;
    }
    lvl = party_level + (in_dungeon ? G->depth - 1 : 0);
    if (kind == NONE) {
        for (top = 0; top < num_wanderers && wanderers[top].from <= lvl;
             ++top) {
        }
        i = (unsigned char)(rand() % top);
    } else {
        i = kind;
    }
    most = (lvl + party_count - 1) / wanderers[i].from;
    if (!most) {
        most = 1;               /* (placed before a level was lost: never) */
    } else if (most > MAX_FOES) {
        most = MAX_FOES;
    }
    pick(&wanderers[i].m, (unsigned char)(1 + rand() % most));
}

#pragma code-name (pop)

/* The field for the ground, as the arena names them (tools/mkfields.py). */
static const char *field_of(unsigned char tile)
{
    switch (tile) {
    case T_FOREST: return "f.forest";
    case T_SWAMP:  return "f.swamp";
    case T_SHORE:  return "f.shore";
    case T_FLOOR:  return "f.floor";
    case T_LAIR:   return "f.lair";
    }
    return "f.grass";
}

/* Whose sprites, by the glyph the game draws the monster as (owarena.c's
   sprite_of()). */
static const char *sprite_of(unsigned char glyph)
{
    switch (glyph) {
    case T_RAT:      return "s.rat";
    case T_GOBLIN:   return "s.goblin";
    case T_WOLF:     return "s.wolf";
    case T_WASP:     return "s.wasp";
    case T_BANDIT:   return "s.bandit";
    case T_SKELETON: return "s.skeleton";
    case T_OGRE:     return "s.ogre";
    case T_WRAITH:   return "s.wraith";
    case T_DRAKE:    return "s.drake";
    case T_DRAGONTL: return "s.dragon";
    }
    return "s.keeper";
}

static void line(unsigned char y, unsigned char colour, const char *s)
{
    textcolor(colour);
    cputsxy(1, y, s);
}

static char num[8];

static void say_num(unsigned int n, const char *after)
{
    utoa(n, num, 10);
    cputs(num);
    cputs(after);
}

/* The pay for `foes` of them, and a level for every threshold crossed:
   everyone grows with it, mended, and a wizard's magic comes back. */
static unsigned char award(unsigned char y)
{
    unsigned int xp = foe.xp * foes, pay = foe.gold * foes;
    unsigned char i;
    Actor *a;

    party_xp += xp;
    gold += pay;
    gotoxy(1, y);
    textcolor(COLOR_WHITE);
    cputs("You gain ");
    say_num(xp, " experience and ");
    say_num(pay, " marks.");
    while (party_level < MAX_LEVEL && party_xp >= level_at[party_level]) {
        ++party_level;
        for (i = 0; i < party_count; ++i) {
            a = &party[i];
            a->hp_max = hp_base[a->cls] + hp_step[a->cls] * (party_level - 1);
            a->mp_max = a->cls == CL_WIZARD ? 6 + 3 * (party_level - 1) : 0;
            a->hp = a->hp_max;
            a->mp = a->mp_max;
        }
        gotoxy(1, ++y);
        textcolor(COLOR_LIGHTGREEN);
        cputs("The party reaches level ");
        say_num(party_level, ".");
    }
    return y + 2;
}

/* One fight won on the overworld in this many leaves a treasure map, as
   the game's treasure_drop() has it (the gear's odds are shop.c's
   gear_odds). Not constant, so that a test can pin it. */
unsigned char treasure_odds = 10;

/* What the foe left, if anything (OWS_*), for owtown's spoils
   (src/lib/ow_tmap.c), which it loads itself again for: the shop's code
   that hands gear out was up in HIGH, under the module. As the game's
   won() and gear_drop() have it: arms from what goes armed (goblins,
   bandits, skeletons and ogres, and whatever of the continents' own is
   drawn as one), and a map from anything beaten out on the land, while
   there is a hand free for it. */
static unsigned char spoils(unsigned char kind, unsigned char always)
{
    unsigned char s = 0, i;

    if (kind != OWF_KEEPER && kind != OWF_DRAGON
        && (foe.glyph == T_GOBLIN
            || (unsigned char)(foe.glyph - T_BANDIT) <= T_OGRE - T_BANDIT)
        && (always || !(rand() % gear_odds))) {
        s = OWS_GEAR;
    }
    if (G->area == AREA_HOME
        && (kind == OWF_WANDER || kind == OWF_UNDEAD)
        && (always || !(rand() % treasure_odds))) {
        for (i = 0; i < OWG_MAPS; ++i) {
            if (!G->tmap[i]) {
                s |= OWS_MAP;
                break;
            }
        }
    }
    return s;
}

/* The music the engine left paused (src/lib/ow_play.s, owtown's). */
void owt_music_on(void);

/* The fight's scene's song, for the fight module to play (fight.h): the
   engine's paused one carried on if it is the same, the file loaded under
   the KERNAL if it is another. Before the module is loaded, which goes
   over the table. */
static unsigned char song, carry;

static void fight_music(unsigned char boss)
{
    unsigned char s = boss ? MUSIC_BOSS : MUSIC_FIGHT;
    unsigned char file = tune_file[s];

    song = carry = 0;
    if (file != OW->tune) {
        OW->tune = OW_NO_TUNE;
        OWB->song = 0;              /* (nothing to carry on with) */
        if (ow_cload(tune_name[file], 0)) {
            OW->tune = file;
        }
    }
    if (OW->tune == file) {
        song = tune_song[s] + 1;
        carry = OWB->song == song;
    }
}

void __fastcall__ ow_fight(unsigned char kind, unsigned char arg)
{
    Fight *f = FIGHT;
    unsigned char i, r, y, ground, always = kind & OWF_LOOT;

    kind &= ~OWF_LOOT;
    switch (kind) {
    case OWF_KEEPER:
        pick(&keepers[arg], 1);
        ground = T_FLOOR;
        break;
    case OWF_DRAGON:
        pick(&maldrath, 1);
        ground = T_LAIR;
        break;
    case OWF_UNDEAD:
        /* battle_undead(): the skeletons, or past level 5 a wraith (fixed
           places in wanderers[]), woken by a grave robber. */
        if (party_level >= 6) {
            pick(&wanderers[9].m, 1);
        } else {
            pick(&wanderers[6].m, (unsigned char)(1 + rand() % 2));
        }
        ground = G->area == AREA_DUNGEON ? T_FLOOR : arg;
        break;
    case OWF_FOE:
        pick_wanderer(1, OWG_FOE_KIND(G->foe_a[arg], G->foe_b[arg]));
        if ((G->fled & OWG_FLED_WHO) == arg + 1) {
            foes = G->fled >> 5;    /* what was left when the party fled */
        }
        ground = T_FLOOR;
        break;
    default:
        pick_wanderer(G->area == AREA_DUNGEON, NONE);
        ground = G->area == AREA_DUNGEON ? T_FLOOR : arg;
        break;
    }

    fight_music(kind == OWF_KEEPER || kind == OWF_DRAGON);

    /* From here the module is over everything above $4000. */
    clrscr();
    while (!ow_cload(FIGHT_FILE, 0)) {
        cputsxy(1, 12, "Put the disk back and press a key.");
        cgetc();
    }
    memcpy(f->party, party, sizeof(party));
    f->party_count = party_count;
    f->party_level = party_level;
    f->potions     = potions;
    f->arrows      = arrows;
    for (i = 0; i < party_count; ++i) {
        f->tunic[i] = G->tunic[i];  /* (the party screen's to change) */
    }
    strcpy(f->name, foe_name);
    f->hp     = foe.hp;
    f->atk    = foe.atk;
    f->def    = foe.def;
    f->colour = foe.color;
    f->speed  = foe.speed;
    f->size   = foe.size;
    f->shot   = foe.shot;
    f->reach  = foe.reach;
    f->kind   = foe.kind;
    f->count  = foes;
    f->flee   = kind == OWF_WANDER || kind == OWF_UNDEAD || kind == OWF_FOE;
    f->seed   = rand();
    strcpy(f->field, field_of(ground));
    strcpy(f->sprite, sprite_of(foe.glyph));
    /* The field and the sprites through the REU cache (ow_cache.s), which
       the module does not link: an empty sprite name tells it they are in. */
    while (!ow_cload(f->field, 0) || !ow_cload(f->sprite, 0)) {
        cputsxy(1, 12, "Put the disk back and press a key.");
        cgetc();
    }
    f->sprite[0] = 0;
    f->song   = song;
    f->carry  = carry;
    f->quiet  = G->quiet;
    f->xp_each  = foe.xp;
    f->to_level = party_level < MAX_LEVEL
                ? level_at[party_level] - party_xp : 0;

    r = fight_call();

    /* The music paused where the fight left it, for the engine to carry
       on with (and this screen meanwhile), and M and X as they were left. */
    OWB->song = f->carry ? f->song : 0;
    G->quiet = (G->quiet & ~3) | (f->quiet & 3);

    memcpy(party, f->party, sizeof(party));
    potions = f->potions;
    arrows  = f->arrows;
    foes    = f->foes;

    /* What came of it, in words: the engine has none. */
    clrscr();
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_YELLOW);
    /* "5 goblins", as many as came (the module is done with its block). */
    foes_name(f->name, foe_name, f->count);
    cputsxy(1, 2, f->name);
    y = 5;
    switch (r) {
    case BATTLE_WON:
        line(4, COLOR_GRAY3, "Won.");
        y = award(y);
        if (kind == OWF_KEEPER) {
            items |= prize[arg];
            if (quest_stage < boss_stage[arg]) {
                quest_stage = boss_stage[arg];
            }
            gotoxy(1, y);
            textcolor(COLOR_YELLOW);
            cputs("You take ");
            cputs(prize_names[arg]);
            cputs(".");
        } else if (kind == OWF_FOE) {
            G->foe_a[arg] |= 0xC0;  /* it stays dead on this level, its */
            G->foe_b[arg] |= 0xE0;  /* blood where it fell (OWG_FOE_DEAD) */
            if ((G->fled & OWG_FLED_WHO) == arg + 1) {
                G->fled = 0;
            }
        } else if (kind == OWF_DRAGON) {
            quest_stage = Q_VICTORY;
            line(y, COLOR_YELLOW, "Maldrath is banished.");
            line(y + 1, COLOR_YELLOW, "Keldmarch is free.");
        }
        i = spoils(kind, always);
        if (i) {
            /* owtown again, for what it left (owtown.c, ow_back()) */
            OWB->mode = OWB_SPOILS;
            OWB->arg  = i;
            OWB->npc  = rand();
            line(y, COLOR_LIGHTGREEN, "It left something behind.");
        }
        break;
    case BATTLE_FLED:
        line(4, COLOR_GRAY3, "You get away.");
        if (kind == OWF_FOE) {
            /* It stays on the map, and so do those of it still standing
               (the module says how many: fq_foes). */
            G->fled = (arg + 1) | (foes << 5);
        }
        break;
    case BATTLE_DEAD:
        /* q4k.c's died(): half the purse, mended, in Millford. */
        gold /= 2;
        for (i = 0; i < party_count; ++i) {
            party[i].hp = party[i].hp_max;
            party[i].mp = party[i].mp_max;
        }
        line(4, COLOR_LIGHTRED, "The party falls.");
        line(6, COLOR_GRAY3, "You wake in Millford, lighter of purse.");
        G->area  = AREA_TOWN;
        G->place = TOWN_MILLFORD;
        G->land  = ISLE_HOME;
        G->x     = OWG_GATE;
        G->wx    = 128 + OW->home_fx[0];
        G->wy    = 64 + OW->home_fy[0];
        G->boat  = 0;
        break;
    default:
        line(4, COLOR_LIGHTRED, "The fight is not on the disk.");
        break;
    }
    if (r == BATTLE_WON || r == BATTLE_FLED) {
        /* Whoever fell gets up again (battle.c's get_up()). */
        for (i = 0; i < party_count; ++i) {
            if (!party[i].hp) {
                party[i].hp = 1;
            }
        }
    }
    owt_music_on();
    line(22, COLOR_GRAY2, "Press a key.");
    while (kbhit()) {
        cgetc();
    }
    cgetc();
}
