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
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/inn.h>
#include <q4k/isle.h>
#include <q4k/loot.h>
#include <q4k/probe.h>
#include <q4k/savegame.h>
#include <q4k/shop.h>
#include <q4k/music.h>
#include <q4k/sail.h>
#include <q4k/sprite.h>
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
#define K_L           0x4C
#define K_I           0x49
#define K_P           0x50
#define K_M           0x4D
#define K_X           0x58
#define K_HELP        0x3F
#define K_F1          0x85   /* save -- S is already "walk south" */
#define K_F3          0x86   /* load */

/* Which stage finding each stone moves the story to. Which stone that is
   lives in game.c as dungeon_prize[], because the generator needs it too. */
static const unsigned char boss_stage[NUM_DUNGEONS] = {
    Q_EMBER, Q_TEAR, Q_HEART,
};

/* Test instrumentation: how many encounter rolls have happened and how many
   became a fight. Deliberately not static, so a headless run can read them
   by name with screendump's --peek. Reading them off the probe page is not
   enough -- that only updates between turns, so it misses every fight that
   is still in progress, which is exactly how an encounter rate of 7 percent
   once looked like zero. */
unsigned char enc_rolls, enc_fights;

/* Nonzero and nothing wanders out at you: --poke no_encounters=1 lets a
   screen test walk the overworld or a dungeon without a fight turning up
   on a different square every run. Nothing in the game sets it. */
unsigned char no_encounters;

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
    if (!home_built()) {
        view_message("The land takes shape...");
    }
    respawn();
    view_restore();
    view_message("You wake in Millford, lighter of purse.");
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
        case 0: view_message("You take the Emberstone."); break;
        case 1: view_message("You take the Silver Tear."); break;
        default: view_message("You take the Dragon's Heart."); break;
    }
}

/* Who the player is talking to, for quest_after_talk. */
static unsigned char talking_to;

/* Hand over quest items the conversation has earned. `granted` is what the
   host has decided so far, or GRANT_OPEN when there was no host to ask --
   the game stays winnable without the modem. Called after every answer, so
   each case must hand its item over once only. Returns the line announcing
   the gift, or 0. */
static const char *quest_after_talk(unsigned char granted)
{
    switch (talking_to) {
        case NPC_KING:
            if (quest_stage < Q_QUESTED
                && (granted == GRANT_SIGIL || granted == GRANT_OPEN)) {
                quest_stage = Q_QUESTED;
                give(IT_SIGIL);
                return "The king gives you the Iron Sigil.";
            }
            break;

        case NPC_VANE:
            if (has(IT_EMBER) && !has(IT_TIDAL)
                && (granted == GRANT_TIDAL || granted == GRANT_OPEN)) {
                quest_stage = Q_TIDAL;
                give(IT_TIDAL);
                return "Vane gives you the Tidal Key.";
            }
            break;

        case NPC_ASH:
            if (has(IT_TEAR) && !has(IT_ASHEN)) {
                quest_stage = Q_ASHEN;
                give(IT_ASHEN);
                return "Mother Ash gives you the Ashen Key.";
            }
            break;

        case NPC_SMITH:
            if (quest_stage == Q_START) {
                if (granted == GRANT_DAGGER || granted == GRANT_OPEN) {
                    quest_stage = Q_SUMMONED;
                    weapon_lv   = 1;
                    return "Garrett gives you a dagger.";
                }
                break;
            }
            if (has(IT_EMBER) && has(IT_TEAR) && has(IT_HEART)
                && !has(IT_BLADE)) {
                quest_stage = Q_FORGED;
                give(IT_BLADE);
                weapon_lv = NUM_WEAPONS - 1;
                return "Garrett reforges Dragonbane.";
            }
            break;

        default:
            break;
    }
    return 0;
}

static void interact(unsigned char index)
{
    const TownEnt *e = &here_ents[index];
    const char *note;

    if (e->kind == ENT_VILLAGER) {
        view_message("They have nothing to say to you.");
        return;
    }
    if (e->kind == ENT_SHOP) {
        shop_open(e->param);
        back_to_map();
        return;
    }
    if (e->kind == ENT_INN) {
        inn_open(e->param);
        back_to_map();
        return;
    }

    /* Gifts are announced in the conversation. With a dead line or no
       daemon there was none, so the gift is ungated and told on the map. */
    talking_to = e->param;
    note = 0;
    if (dialog_talk(npc_ids[e->param], npc_names[e->param],
                    quest_stage, lore_seed, quest_after_talk) != DIALOG_OK) {
        note = quest_after_talk(GRANT_OPEN);
    }
    back_to_map();
    if (note) {
        view_message(note);
    }
    ++game_turn;
}

/* The entity standing on (x,y), or -1. */
static signed char entity_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    if (scene != SCENE_TOWN) {
        return -1;
    }
    for (i = 0; i < here_n; ++i) {
        if (here_ents[i].x == x && here_ents[i].y == y) {
            return (signed char)i;
        }
    }
    return -1;
}

static void enter_dungeon(unsigned char index)
{
    if (!has(dungeon_key[index])) {
        view_message("The way in is sealed.");
        return;
    }
    enter_local();
    scene = SCENE_DUNGEON;
    place = index;
    depth = 1;
    dungeon_build(index, depth);
    fov_enter(index, depth);
    reseed();            /* dungeon_build seeds for repeatable levels */
    hero.x = dun_up_x;
    hero.y = dun_up_y;
    view_message(dungeon_names[index]);
}

static void descend(void)
{
    ++depth;
    dungeon_build(place, depth);
    fov_enter(place, depth);
    reseed();
    hero.x = dun_up_x;
    hero.y = dun_up_y;
    view_message("You go down.");
}

static void ascend(void)
{
    if (depth <= 1) {
        leave_local();
        view_message("Daylight.");
        return;
    }
    --depth;
    dungeon_build(place, depth);
    fov_enter(place, depth);
    reseed();
    hero.x = dun_dn_x;
    hero.y = dun_dn_y;
    view_message("You climb back up.");
}

static void face_the_dragon(void)
{
    if (!has(IT_BLADE)) {
        view_message("The heat drives you back.");
        return;
    }
    if (battle_dragon() == BATTLE_DEAD) {
        died();
        return;
    }
    quest_stage = Q_VICTORY;
    back_to_map();
    view_message("Maldrath is banished. Keldmarch is free.");
}

/* Stepping onto something on the overworld. */
static void arrive(unsigned char code, unsigned char x, unsigned char y)
{
    unsigned char i, slot;

    if (code == T_PORT) {
        if (scene == SCENE_ISLE) {
            sail_home();                /* the landing */
        } else {
            sail_out();
        }
        return;
    }
    if (code == T_LAIR) {
        face_the_dragon();
        return;
    }

    i = place_at(x, y);
    if (i == PLACE_NONE) {
        return;
    }
    if (land == ISLE_HOME) {
        switch (i) {
            case 0: i = TOWN_MILLFORD; break;
            case 1: i = TOWN_CASTLE;   break;
            case 2: enter_dungeon(0);  return;
            default: i = TOWN_PROC;    break;
        }
    } else if (i == 0) {
        slot = slot_of(land);
        if (slot & 1) {
            enter_dungeon(1 + (slot >> 1));
            return;
        }
        i = TOWN_WARDLOW + (slot >> 1);
    } else {
        i = TOWN_PROC;
    }
    enter_town(i);
    view_message(town_name());
}

static void wander_check(void)
{
    if (scene == SCENE_TOWN || no_encounters) {
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

static void try_move(signed char dx, signed char dy, unsigned char dir)
{
    int nx = (int)hero.x + dx;
    int ny = (int)hero.y + dy;
    unsigned char code, flag;
    signed char ent;

    /* Turn first: bumping into a wall or a person still faces them. */
    sprite_face(dir);

    if (nx < 0 || ny < 0 || nx >= local_w || ny >= local_h) {
        view_message("Blocked.");
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
        sprite_walk(dir);
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
        view_message("Blocked.");
        return;
    }
    if (flag == TF_WATER) {
        view_message("You cannot swim.");
        return;
    }

    /* The step is shown before anything it leads to -- a fight, a town,
       the stairs -- takes the screen away. */
    sfx_step(code);
    sprite_walk(dir);
    hero.x = (unsigned char)nx;
    hero.y = (unsigned char)ny;
    ++game_turn;

    if (scene == SCENE_WORLD || scene == SCENE_ISLE) {
        if (flag == TF_ENTER) {
            arrive(code, hero.x, hero.y);
        } else {
            wander_check();
        }
        return;
    }

    if (scene == SCENE_TOWN) {
        if (hero.x == here_ex && hero.y == here_ey) {
            if (quest_stage == Q_START) {
                /* Nobody leaves Millford before Garrett has talked to them. */
                hero.y = here_ey - 1;
                view_message("It's not safe out there without a weapon.");
                return;
            }
            if (!home_built()) {
                view_message("The land takes shape...");
            }
            leave_local();
            view_message("You take the road.");
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
        view_message("You have no draughts.");
        return;
    }
    if (!drink_potion()) {
        view_message("You are unhurt.");
        return;
    }
    view_message("You drink a draught.");
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
        view_message("There is nobody here.");
        return;
    }
    interact((unsigned char)e);
}

int main(void)
{
    unsigned char key;
    unsigned char seeded = 0;
    unsigned char restored;
    /* PETSCII throughout: hero.name and the literals are both what conio
       expects, so they concatenate straight together. */
    static char greeting[SCR_W + 1];

    /* First of all: game_init() unpacks Millford into local[], which is the
       font image until the charset has been copied out of it. */
    tiles_install();
    game_init();                       /* seeds hero.name with the default */
    /* No keyboard drain here: the title screen already discarded the
       launcher's RUN before it accepted a name, and draining again would
       swallow whatever the player typed ahead. */
    restored = title_screen(hero.name, NAME_LEN);
    view_init();
    music_start();

    view_message("Press ? for help.");
    if (restored) {
        strcpy(greeting, "Welcome back, ");
        strcat(greeting, hero.name);
        strcat(greeting, ".");
        view_message(greeting);
    } else {
        /* The story is on the title screen now; this is just the morning. */
        strcpy(greeting, "Hail, ");
        strcat(greeting, hero.name);
        strcat(greeting, "!");
        view_message(greeting);
        view_message("A new day dawns in Millford.");
        view_message("Walk into someone to speak with them.");
    }

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
        if (!lore_seed) {
            /* A new game's secrets, drawn after the player's own timing has
               stirred the generator. A restored game brings its own. */
            lore_seed = (unsigned char)(rand() % 255) + 1;
        }
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;            /* fold SHIFTed letters onto unshifted */
        }

        switch (key) {
            case K_W: case K_CRSR_UP:    try_move( 0, -1, DIR_UP);    break;
            case K_S: case K_CRSR_DOWN:  try_move( 0,  1, DIR_DOWN);  break;
            case K_A: case K_CRSR_LEFT:  try_move(-1,  0, DIR_LEFT);  break;
            case K_D: case K_CRSR_RIGHT: try_move( 1,  0, DIR_RIGHT); break;

            case K_T:
                try_talk();
                break;

            case K_P:
                quaff();
                break;

            case K_L:
                look();
                break;

            case K_I:
                treasure_maps();
                break;

            case K_M:
                view_message(music_toggle() ? "Music on." : "Music off.");
                break;

            case K_X:
                view_message(sfx_toggle() ? "Effects on." : "Effects off.");
                break;

            case K_F1:
                view_message("Saving...");
                switch (save_game()) {
                    case SAVE_OK: view_message("Saved."); break;
                    default:      view_message("The drive would not."); break;
                }
                break;

            case K_F3:
                view_message("Loading...");
                switch (load_game()) {
                    case SAVE_OK:
                        back_to_map();
                        view_message("Restored.");
                        break;
                    case SAVE_EFORMAT:
                        view_message("That is not a saved game.");
                        break;
                    default:
                        view_message("No saved game found.");
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
                sprite_hide();
                VIC.addr = 0x15;    /* hand the ROM charset back to BASIC */
                clrscr();
                return 0;

            default:
                break;
        }
    }
}
