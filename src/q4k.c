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
#include <q4k/engine.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/gift.h>
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
#include <q4k/stick.h>
#include <q4k/tiles.h>
#include <q4k/title.h>
#include <q4k/town_gen.h>
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
#define K_LT          0x3C   /* < */
#define K_GT          0x3E   /* > */
#define K_Y           0x59
#define K_F1          0x85   /* save -- S is already "walk south" */
#define K_F3          0x86   /* load */

/* In CHDATA2 because MAIN is full; see CLAUDE.md. */
#pragma rodata-name (push, "CHDATA2")
static const char msg_villager[] = "The villager nods politely.";
#pragma rodata-name (pop)

/* Said on a square that has something to go through. */
#define HINT_ENTER    "> to enter."
#define HINT_EXIT     "< to exit."
#define HINT_DESCEND  "> to descend."
#define HINT_ASCEND   "< to ascend."

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
    unsigned char key = stick_key();    /* the joystick in port 2 as well */

    rng_stir(stick_spins);
    return key;
}

/* Ask, and only Y (either case) says yes. */
static unsigned char confirm(const char *question)
{
    unsigned char key;

    view_message(question);
    key = wait_key();
    if (key == K_Y || key == K_Y + 0x80) {
        return 1;
    }
    view_message("Cancelled.");
    return 0;
}

static void back_to_map(void)
{
    view_restore();
}

static void died(void)
{
    gold /= 2;
    party_rest();
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

static void interact(unsigned char index)
{
    const TownEnt *e = &here_ents[index];
    const char *note;

    if (e->kind == ENT_VILLAGER) {
        view_message(msg_villager);
        return;
    }
    if (e->kind == ENT_SHOP) {
        eng_a = e->param;
        engine_call(ENG_SHOP, shop_entry);
        back_to_map();
        return;
    }
    if (e->kind == ENT_INN) {
        /* The inns' names are the map's: the talk's engine is told. */
        strcpy(eng_text, e->param < NUM_INNS ? inn_names[e->param]
                                              : town_gen_inn());
        eng_a = TALK_INN;
        eng_b = e->param;
        engine_call(ENG_TALK, talk_entry);
        back_to_map();
        return;
    }

    /* Gifts are announced in the conversation. With nobody to talk to (no
       host on the line, or no file on the disk) there was none, so the gift
       is ungated and told on the map: the talk's engine (talk_entry()) hands
       the line back in eng_text. */
    talking_to = e->param;
    note = 0;
    strcpy(eng_text, npc_names[e->param]);
    eng_a = TALK_NPC;
    eng_b = e->param;
    if (engine_call(ENG_TALK, talk_entry)) {
        note = eng_text;
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
    lair_clear();
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
}

/* After a fight that began away from the map's own turn: dying wakes you in
   Millford; anything else goes back to the map. */
static void fought(unsigned char result)
{
    if (result == BATTLE_DEAD) {
        died();
    } else {
        back_to_map();
    }
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
    fought(battle_wander());
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
        if (code == T_PORT || code == T_LAIR) {
            arrive(code, hero.x, hero.y);
        } else if (flag == TF_ENTER) {
            view_message(HINT_ENTER);
        } else {
            wander_check();
        }
        return;
    }

    if (scene == SCENE_TOWN) {
        if (hero.x == here_ex && hero.y == here_ey) {
            view_message(HINT_EXIT);
        }
        return;
    }

    if (code == T_STAIRDN) {
      view_message(HINT_DESCEND);
    }

    if (code == T_STAIRUP) {
        view_message(HINT_ASCEND);
    }
    wander_check();
}

/* > goes in at a town or a dungeon and down a stair; < comes out and goes
   up. Nothing happens by walking onto them, bar the ports and the lair. */
static void use_stairs(unsigned char down)
{
    unsigned char code = map_at(hero.x, hero.y);

    if (scene == SCENE_DUNGEON) {
        if (code == (down ? T_STAIRDN : T_STAIRUP)) {
            if (down) {
                descend();
            } else {
                ascend();
            }
            return;
        }
    } else if (down) {
        if (scene != SCENE_TOWN && tile_flags[code - TILE_BASE] == TF_ENTER) {
            arrive(code, hero.x, hero.y);
            return;
        }
    } else if (scene == SCENE_TOWN && hero.x == here_ex && hero.y == here_ey) {
        if (quest_stage == Q_START) {
            /* Nobody leaves Millford before Garrett has talked to them. */
            view_message("It's not safe out there without a weapon.");
            return;
        }
        if (!home_built()) {
            view_message("The land takes shape...");
        }
        leave_local();
        return;
    }
    view_message("Nothing here.");
}

/* A draught is worth drinking before a fight as well as during one: the
   worst hurt of the party has it. */
static void quaff(void)
{
    unsigned char who = most_hurt();

    if (!potions) {
        view_message("You have no draughts.");
        return;
    }
    if (who == 0xFF || !drink_potion(who)) {
        view_message("Nobody is hurt.");
        return;
    }
    strcpy(eng_text, party[who].name);
    strcat(eng_text, " drinks a draught.");
    view_message(eng_text);
    ++game_turn;
}

/* Talk to whoever is next to you. */
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

/* main() is in boot.c, which runs the title screen and then this, in the
   map's engine (engine.h), for good. */
void map_entry(unsigned char restored)
{
    unsigned char key;
    unsigned char seeded = 0;
    unsigned char told;
    unsigned int  ticked;
    /* PETSCII throughout: hero.name and the literals are both what conio
       expects, so they concatenate straight together. */
    static char greeting[SCR_W + 1];

    view_place_fn = view_place;     /* the panel's "where you are" is ours */
    if (restored) {
        scene_restore();
    } else {
        game_start();
    }
    if (restored) {
        view_message("Press ? for help.");
        strcpy(greeting, "Welcome back, ");
        strcat(greeting, hero.name);
        strcat(greeting, ".");
        view_message(greeting);
    } else {
        /* Everyone the letter brought, by name. */
        strcpy(greeting, "Hail, ");
        for (key = 0; key < party_count; ++key) {
            if (key) {
                strcat(greeting, key + 1 == party_count ? " and " : ", ");
            }
            strcat(greeting, party[key].name);
        }
        if (party_count > 1) {
            strcat(greeting, "!");
            view_message(greeting);
            view_message("Press ? for help.");
        } else {
            strcat(greeting, "! Press ? for help.");
            view_message(greeting);
        }
        quest_hint();
    }

    told = quest_stage;
    ticked = game_turn;
    for (;;) {
        if (game_turn != ticked) {
            ticked = game_turn;
            party_tick();
        }
        if (quest_stage != told) {
            told = quest_stage;
            quest_hint();
        }
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

            case K_GT: use_stairs(1); break;
            case K_LT: use_stairs(0); break;
            case STICK_FIRE:
                /* The button is whichever of > and < means something
                   here: out of a town, up a stair going up, otherwise in
                   or down. */
                use_stairs(scene != SCENE_TOWN
                           && map_at(hero.x, hero.y) != T_STAIRUP);
                break;

            case K_P:
                quaff();
                break;

            case K_L:
                key = look();
                if (key != LOOK_NOTHING) {
                    fought(key);
                }
                break;

            case K_I:
                /* The party and what they carry (the shop's engine), and
                   then the treasure maps, if they asked for them. */
                eng_a = SHOP_PARTY;
                if (engine_call(ENG_SHOP, shop_entry)) {
                    treasure_maps();
                }
                back_to_map();
                break;

            case K_M:
                view_message(music_toggle() ? "Music on." : "Music off.");
                break;

            case K_X:
                view_message(sfx_toggle() ? "Effects on." : "Effects off.");
                break;

            case K_F1:
                if (!confirm("Overwrite save? (Y/N)")) {
                    break;
                }
                view_message("Saving...");
                switch (save_game()) {
                    case SAVE_OK: view_message("Saved."); break;
                    default:      view_message("The drive would not."); break;
                }
                break;

            case K_F3:
                if (!confirm("Load game? (Y/N)")) {
                    break;
                }
                view_message("Loading...");
                switch (load_game()) {
                    case SAVE_OK:
                        scene_restore();
                        told = quest_stage;
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
                eng_a = SHOP_HELP;
                engine_call(ENG_SHOP, shop_entry);
                back_to_map();
                break;

            case K_Q:
                if (!confirm("Quit? (Y/N)")) {
                    break;
                }
                music_stop();       /* else the last note drones on in BASIC */
                *(unsigned char *)0xD418 = 0;   /* master volume off: all voices */
                sprite_hide();
                VIC.addr = 0x15;    /* hand the ROM charset back to BASIC */
                clrscr();
                asm("sei");
                ((void (*)(void))0xFCE2)();     /* SYS 64738: the machine's reset */
                return;

            default:
                break;
        }
    }
}
