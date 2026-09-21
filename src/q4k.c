/*
 * Quest for the King -- a grid RPG roguelike for the Commodore 64.
 *
 * Three scenes share one movement loop: the wrapping overworld, one-screen
 * towns, and generated dungeon levels. The plot is deliberately the oldest
 * one there is -- a letter, a king, three sealed places, a dragon -- because
 * the new thing here is who you talk to, not what you are asked to do.
 */

#include <conio.h>
#include <c64.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/probe.h>
#include <q4k/savegame.h>
#include <q4k/shop.h>
#include <q4k/tiles.h>
#include <q4k/title.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* PETSCII key codes, spelled numerically on purpose: cc65 translates C
   character literals to PETSCII at compile time, and relying on that for
   comparisons against raw key codes is a good way to get a subtle bug. */
#define K_CRSR_DOWN   0x11
#define K_CRSR_RIGHT  0x1D
#define K_CRSR_UP     0x91
#define K_CRSR_LEFT   0x9D
#define K_SPACE       0x20
#define K_W           0x57
#define K_A           0x41
#define K_S           0x53
#define K_D           0x44
#define K_Q           0x51
#define K_T           0x54
#define K_P           0x50
#define K_HELP        0x3F
#define K_F1          0x85   /* save -- S is already "walk south" */
#define K_F3          0x86   /* load */

/* Which stage finding each stone moves the story to. Which stone that is
   lives in game.c as dungeon_prize[], because the generator needs it too. */
static const unsigned char boss_stage[NUM_DUNGEONS] = {
    Q_EMBER, Q_TEAR, Q_HEART,
};

/* Test instrumentation: how many encounter rolls have happened and how many
   turned into a fight. Published on the probe page so a headless run can say
   something about the encounter rate. */
/* Test instrumentation: how many encounter rolls have happened and how many
   became a fight. Deliberately not static, so a headless run can read them
   by name with screendump's --peek. Reading them off the probe page is not
   enough -- that only updates between turns, so it misses every fight that
   is still in progress, which is exactly how an encounter rate of 7 percent
   once looked like zero. */
unsigned char enc_rolls, enc_fights;

/* Mirror progress into the probe page so the headless harness can assert on
   it. Stage doubles as a turn counter, which is what lets a test say "walk
   three squares, then show me the screen". */
static void publish(void)
{
    PROBE_SCRATCH[0] = hero.x;
    PROBE_SCRATCH[1] = hero.y;
    PROBE_SCRATCH[2] = scene;
    PROBE_SCRATCH[4] = quest_stage;
    PROBE_SCRATCH[5] = items;
    PROBE_SCRATCH[6] = hero.hp;
    PROBE_SCRATCH[7] = depth;
    PROBE_SCRATCH[8] = enc_rolls;
    PROBE_SCRATCH[9] = enc_fights;

    PROBE_COUNTER    = (unsigned char)game_turn;
    probe_stage(game_turn < 254 ? (unsigned char)game_turn + 1 : 255);
}

/* Wait for a key, and time the wait.
 *
 * The spin count is how long the player took to decide, which is the only
 * unrepeatable thing the machine has to offer -- see rng_stir(). */
static unsigned char wait_key(void)
{
    unsigned int spins = 0;

    while (!kbhit()) {
        ++spins;
    }
    rng_stir(spins);
    return cgetc();
}

static void back_to_map(void)
{
    view_restore();
}

static void died(void)
{
    gold /= 2;
    heal_full();
    enter_town(TOWN_MILLFORD);
    view_restore();
    view_message("YOU WAKE IN MILLFORD, LIGHTER OF PURSE.");
}

/* A keeper defeated: take the stone and move the story on. */
static void keeper_beaten(void)
{
    give(dungeon_prize[place]);
    if (quest_stage < boss_stage[place]) {
        quest_stage = boss_stage[place];
    }
    map_set(hero.x, hero.y, T_FLOOR);

    switch (place) {
        case 0: view_message("YOU TAKE THE EMBERSTONE."); break;
        case 1: view_message("YOU TAKE THE SILVER TEAR."); break;
        default: view_message("YOU TAKE THE DRAGON'S HEART."); break;
    }
}

/* Hand over quest items after the conversation has happened. */
static void quest_after_talk(unsigned char npc)
{
    switch (npc) {
        case NPC_KING:
            if (quest_stage < Q_QUESTED) {
                quest_stage = Q_QUESTED;
                give(IT_SIGIL);
                view_message("THE KING GIVES YOU THE IRON SIGIL.");
            }
            break;

        case NPC_VANE:
            if (has(IT_EMBER) && !has(IT_TIDAL)) {
                quest_stage = Q_TIDAL;
                give(IT_TIDAL);
                view_message("VANE GIVES YOU THE TIDAL KEY.");
            }
            break;

        case NPC_ASH:
            if (has(IT_TEAR) && !has(IT_ASHEN)) {
                quest_stage = Q_ASHEN;
                give(IT_ASHEN);
                view_message("MOTHER ASH GIVES YOU THE ASHEN KEY.");
            }
            break;

        case NPC_SMITH:
            if (has(IT_EMBER) && has(IT_TEAR) && has(IT_HEART)
                && !has(IT_BLADE)) {
                quest_stage = Q_FORGED;
                give(IT_BLADE);
                weapon_lv = NUM_WEAPONS - 1;
                view_message("GARRETT REFORGES DRAGONBANE.");
            }
            break;

        default:
            break;
    }
}

static void interact(unsigned char index)
{
    const TownEnt *e = &town_ents[index];

    if (e->kind == ENT_SHOP) {
        shop_open(e->param);
        back_to_map();
        return;
    }

    dialog_talk(npc_ids[e->param], npc_names[e->param], quest_stage);
    back_to_map();
    quest_after_talk(e->param);
    ++game_turn;
}

/* The entity standing on (x,y), or -1. */
static signed char entity_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    if (scene != SCENE_TOWN) {
        return -1;
    }
    for (i = town_ent_first[place]; i < town_ent_first[place + 1]; ++i) {
        if (town_ents[i].x == x && town_ents[i].y == y) {
            return (signed char)i;
        }
    }
    return -1;
}

static void enter_dungeon(unsigned char index)
{
    if (!has(dungeon_key[index])) {
        view_message("THE WAY IN IS SEALED.");
        return;
    }
    scene = SCENE_DUNGEON;
    place = index;
    depth = 1;
    dungeon_build(index, depth);
    fov_reset();
    reseed();            /* dungeon_build seeds for repeatable levels */
    hero.x = dun_up_x;
    hero.y = dun_up_y;
    view_message(dungeon_names[index]);
}

static void descend(void)
{
    ++depth;
    dungeon_build(place, depth);
    fov_reset();
    reseed();
    hero.x = dun_up_x;
    hero.y = dun_up_y;
    view_message("YOU GO DOWN.");
}

static void ascend(void)
{
    if (depth <= 1) {
        leave_local();
        view_message("DAYLIGHT.");
        return;
    }
    --depth;
    dungeon_build(place, depth);
    fov_reset();
    reseed();
    hero.x = dun_dn_x;
    hero.y = dun_dn_y;
    view_message("YOU CLIMB BACK UP.");
}

static void face_the_dragon(void)
{
    if (!has(IT_BLADE)) {
        view_message("THE HEAT DRIVES YOU BACK.");
        return;
    }
    if (battle_dragon() == BATTLE_DEAD) {
        died();
        return;
    }
    quest_stage = Q_VICTORY;
    back_to_map();
    view_message("MALDRATH IS BANISHED. KELDMARCH IS FREE.");
}

/* Stepping onto something on the overworld. */
static void arrive(unsigned char code, unsigned char x, unsigned char y)
{
    signed char i;

    if (code == T_LAIR) {
        face_the_dragon();
        return;
    }

    i = town_at(x, y);
    if (i >= 0) {
        /* Leaving home for the first time is the story starting. */
        enter_town((unsigned char)i);
        view_message(town_names[place]);
        return;
    }

    i = dungeon_at(x, y);
    if (i >= 0) {
        enter_dungeon((unsigned char)i);
    }
}

static void wander_check(void)
{
    if (scene == SCENE_TOWN) {
        return;
    }
    ++enc_rolls;
    if (!encounter_roll(map_at(hero.x, hero.y))) {
        return;
    }
    ++enc_fights;
    if (battle_wander() == BATTLE_DEAD) {
        died();
    } else {
        back_to_map();
    }
}

static void try_move(signed char dx, signed char dy)
{
    int nx = (int)hero.x + dx;
    int ny = (int)hero.y + dy;
    unsigned char code, flag;
    signed char ent;

    if (scene == SCENE_WORLD) {
        nx &= WORLD_MASK;
        ny &= WORLD_MASK;
    } else if (nx < 0 || ny < 0 || nx >= LOCAL_W || ny >= LOCAL_H) {
        view_message("BLOCKED.");
        return;
    }

    ent = entity_at((unsigned char)nx, (unsigned char)ny);
    if (ent >= 0) {
        interact((unsigned char)ent);
        return;
    }

    code = map_at((unsigned char)nx, (unsigned char)ny);

    /* The keeper does not step aside. */
    if (code == T_MONSTER) {
        hero.x = (unsigned char)nx;
        hero.y = (unsigned char)ny;
        if (battle_boss(place) == BATTLE_DEAD) {
            died();
        } else {
            back_to_map();
            keeper_beaten();
        }
        ++game_turn;
        return;
    }

    flag = tile_flags[code - TILE_BASE];
    if (flag == TF_BLOCK) {
        /* Bumping into terrain costs no turn -- roguelike convention. */
        view_message("BLOCKED.");
        return;
    }
    if (flag == TF_WATER) {
        view_message("THOU CANST NOT SWIM.");
        return;
    }

    hero.x = (unsigned char)nx;
    hero.y = (unsigned char)ny;
    ++game_turn;

    if (scene == SCENE_WORLD) {
        if (flag == TF_ENTER) {
            arrive(code, hero.x, hero.y);
        } else {
            wander_check();
        }
        return;
    }

    if (scene == SCENE_TOWN) {
        if (hero.x == town_ex[place] && hero.y == town_ey[place]) {
            if (place == TOWN_MILLFORD && quest_stage == Q_START) {
                quest_stage = Q_SUMMONED;
            }
            leave_local();
            view_message("YOU TAKE THE ROAD.");
        }
        return;
    }

    if (code == T_STAIRDN) {
        descend();
    } else if (code == T_STAIRUP) {
        ascend();
    } else {
        wander_check();
    }
}

/* Talk to whoever is next to you. */
/* A draught is worth drinking before a fight as well as during one. */
static void quaff(void)
{
    if (!potions) {
        view_message("YOU HAVE NO DRAUGHTS.");
        return;
    }
    if (!drink_potion()) {
        view_message("YOU ARE UNHURT.");
        return;
    }
    view_message("YOU DRINK A DRAUGHT.");
    ++game_turn;
}

static void try_talk(void)
{
    signed char e;

    e = entity_at(hero.x, hero.y - 1);
    if (e < 0) { e = entity_at(hero.x, hero.y + 1); }
    if (e < 0) { e = entity_at(hero.x - 1, hero.y); }
    if (e < 0) { e = entity_at(hero.x + 1, hero.y); }

    if (e < 0) {
        view_message("THERE IS NOBODY HERE.");
        return;
    }
    interact((unsigned char)e);
}

int main(void)
{
    unsigned char key;
    unsigned char seeded = 0;
    /* PETSCII throughout: hero.name and the literals are both what conio
       expects, so they concatenate straight together. */
    static char greeting[SCR_W + 1];

    game_init();                       /* seeds hero.name with the default */
    /* No keyboard drain here: the title screen already discarded the
       launcher's RUN before it accepted a name, and draining again would
       swallow whatever the player typed ahead. */
    title_screen(hero.name, NAME_LEN);
    view_init();

    /* The story is on the title screen now; this is just the morning. */
    strcpy(greeting, "HAIL, ");
    strcat(greeting, hero.name);
    strcat(greeting, "!");
    view_message(greeting);
    view_message("A NEW DAY DAWNS IN MILLFORD.");
    view_message("WALK INTO SOMEONE TO SPEAK WITH THEM.");

    for (;;) {
        if (scene == SCENE_DUNGEON) {
            fov_update(hero.x, hero.y);
        }
        view_draw();
        view_draw_panel();
        publish();

        key = wait_key();
        if (!seeded) {
            /* First input: entropy has something in it now, so start the
               wandering-monster sequence from it. */
            reseed();
            seeded = 1;
        }
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;            /* fold SHIFTed letters onto unshifted */
        }

        switch (key) {
            case K_W: case K_CRSR_UP:    try_move( 0, -1); break;
            case K_S: case K_CRSR_DOWN:  try_move( 0,  1); break;
            case K_A: case K_CRSR_LEFT:  try_move(-1,  0); break;
            case K_D: case K_CRSR_RIGHT: try_move( 1,  0); break;

            case K_T:
                try_talk();
                break;

            case K_P:
                quaff();
                break;

            case K_F1:
                switch (save_game()) {
                    case SAVE_OK: view_message("SAVED."); break;
                    default:      view_message("THE DRIVE WOULD NOT."); break;
                }
                break;

            case K_F3:
                switch (load_game()) {
                    case SAVE_OK:
                        back_to_map();
                        view_message("RESTORED.");
                        break;
                    case SAVE_EFORMAT:
                        view_message("THAT IS NOT A SAVED GAME.");
                        break;
                    default:
                        view_message("NO SAVED GAME FOUND.");
                        break;
                }
                break;

            case K_HELP:
                help_screen();
                back_to_map();
                break;

            case K_SPACE:
                ++game_turn;
                wander_check();
                break;

            case K_Q:
                VIC.addr = 0x15;    /* hand the ROM charset back to BASIC */
                clrscr();
                return 0;

            default:
                break;
        }
    }
}
