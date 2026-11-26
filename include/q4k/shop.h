/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Quest for the King -- buying things. Takes over the screen; the caller
   redraws. `kind` is one of the SHOP_* constants from town_data.h. */

#ifndef Q4K_SHOP_H
#define Q4K_SHOP_H

/* A draught costs ten marks a level.
 *
 * What one restores scales with your health, so what it costs scales too.
 * At level seven the eleven or so needed to see off Maldrath come to about
 * what the best armour costs, which makes the last shopping trip a real
 * decision rather than small change. */
unsigned int potion_price(void);

void shop_open(unsigned char kind);

/* After a fight won against something that carries arms: now and then it
   leaves a blade, mail or a bow, of the tiers this land's shops sell (one
   in gear_odds: the roll is ow_fight.c's), and whoever in the party it
   would be better for takes it up, asked if that is more than one of them.
   gear_drop() says so from row y (the asking there too), `r` a roll for
   which of the three and which tier, and returns the row after; for the
   open world's spoils (src/lib/ow_tmap.c). */
extern unsigned char gear_odds;
unsigned char __fastcall__ gear_drop(unsigned char r, unsigned char y);

/* The chest at the end of a sea cave found at level `lv` (OWB_HOARD: the
   cave has fallen in behind the party, src/ow/dungeon.c's hoard()): lv x
   50 marks, and something nobody in the party has yet that one of them
   could use -- a blade, mail or a bow of the tier of lands that level is
   for, or a spellbook a wizard can follow -- taken up as gear_drop()'s is.
   A screen of its own, until a key. */
void __fastcall__ cave_hoard(unsigned char lv);

/* The party screen's parts (I), for the open world's own (src/owtown.c),
   which shows one member at a time with their figure. The game's own is
   party_draw() and is reached through shop_entry() (SHOP_PARTY).
   party_head(): the screen cleared and its title; member_rows(): member i's
   rows from row y, returning the row after the blank one below them;
   party_totals(): the level, the purse, the draughts and arrows, two rows
   from y; party_prepare(): one key of preparing wizard `who`'s spells, over
   rows 20-24, 1 while they go on (redraw and call again); party_spells():
   how many spells member `who` knows, and so whether preparing is worth
   offering (more than one). */
void party_head(void);
unsigned char member_rows(unsigned char i, unsigned char y);
void __fastcall__ party_totals(unsigned char y);
void party_draw(void);
unsigned char __fastcall__ party_prepare(unsigned char who);
unsigned char __fastcall__ party_spells(unsigned char who);

#endif /* Q4K_SHOP_H */
