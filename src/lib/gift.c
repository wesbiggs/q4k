/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * q4k -- what people hand over when you talk to them, and the talk's engine's
 * entry.
 *
 * Moved out of src/q4k.c so that it is with the conversations, in the talk's
 * engine on the disk (engine.h), where the conversation that calls it is.
 * Both builds' conversations call it the same way (dialog.h).
 */

#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dialog.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/gift.h>
#include <q4k/inn.h>
#include <q4k/town_data.h>

/* In CHDATA2 because MAIN is full; see CLAUDE.md. */
#pragma rodata-name (push, "CHDATA2")
static const char msg_reward[]   = "The king rewards you 1,000 gold!";
static const char msg_praise[]   = "The king praises your valour.";
#pragma rodata-name (pop)

/* Who the player is talking to, for quest_after_talk. */
unsigned char talking_to;

/* What the smith hands each of the party who answered the letter: a dagger
   for a warrior, a hunting bow (and a quiver's worth) for an archer, a
   staff for a wizard. With one of them, he says which. */
static const char *const armed[NUM_CLASSES + 1] = {
    "Garrett gives you a dagger.",
    "Garrett gives you a hunting bow.",
    "Garrett gives you a staff.",
    "Garrett arms each of you.",
};

static const char *arm_party(void)
{
    unsigned char i;
    Actor *a;

    for (i = 0; i < party_count; ++i) {
        a = &party[i];
        switch (a->cls) {
            case CL_WARRIOR: a->weapon = WPN_DAGGER;  break;
            case CL_ARCHER:  a->bow    = BOW_HUNTING; arrows = ARROW_FREE; break;
            default:         a->weapon = WPN_STAFF;   break;
        }
    }
    return armed[party_count > 1 ? NUM_CLASSES : hero.cls];
}

/* Dragonbane is a warrior's blade: the first of them takes it, or, with no
   warrior in the party, whoever leads. */
static void bear_dragonbane(void)
{
    unsigned char i;

    for (i = 0; i < party_count && party[i].cls != CL_WARRIOR; ++i) {
    }
    party[i < party_count ? i : 0].weapon = DRAGONBANE;
}

/* Hand over quest items the conversation has earned: `granted` is one gift
   (GRANT_*) or GRANT_OPEN, whatever the quest needs now. Each case must hand
   its item over once only. Returns the line announcing the gift, or 0. */
const char *quest_after_talk(unsigned char granted)
{
    switch (talking_to) {
        case NPC_KING:
	  if (quest_stage == Q_VICTORY) {
	    quest_stage = Q_REWARDED;
	    gold += 1000;
	    return msg_reward;
	  } else if (quest_stage == Q_REWARDED) {
	    return msg_praise;
	  } else if (quest_stage < Q_QUESTED
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
            if (has(IT_TEAR) && !has(IT_ASHEN)
                && (granted == GRANT_ASHEN || granted == GRANT_OPEN)) {
                quest_stage = Q_ASHEN;
                give(IT_ASHEN);
                return "Mother Ash gives you the Ashen Key.";
            }
            break;

        case NPC_SMITH:
            if (quest_stage == Q_START) {
                if (granted == GRANT_DAGGER || granted == GRANT_OPEN) {
                    quest_stage = Q_SUMMONED;
                    return arm_party();
                }
                break;
            }
            if (has(IT_EMBER) && has(IT_TEAR) && has(IT_HEART)
                && !has(IT_BLADE)
                && (granted == GRANT_FORGE || granted == GRANT_OPEN)) {
                quest_stage = Q_FORGED;
                give(IT_BLADE);
                bear_dragonbane();
                return "Garrett reforges Dragonbane.";
            }
            break;

        default:
            break;
    }
    return 0;
}

/* The talk's engine (engine.h): a person, or an inn. With nobody to talk to,
   or a host gone silent, the person still hands over what they would have,
   on the map: the line saying so goes back in eng_text, which came in with
   their name. */
unsigned char talk_entry(void)
{
    unsigned char r;
    const char *note;

    if (eng_a == TALK_INN) {
        inn_open(eng_b);
        return 0;
    }
    r = dialog_talk(eng_b, eng_text, quest_after_talk);
    if (r == DIALOG_OK) {
        return 0;
    }
    note = quest_after_talk(GRANT_OPEN);
    if (note) {
        strcpy(eng_text, note);
    } else if (r == DIALOG_ENOLINE) {
        strcat(eng_text, " has nothing for you.");
    } else {
        return 0;
    }
    return 1;
}
