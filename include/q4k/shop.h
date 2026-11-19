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
   leaves a blade, mail or a bow, of the tiers this land's shops sell, and
   the hero takes it up if it is better than what they have. Says which.
   One in gear_odds does: the roll is made by fightcall.c, which loads the
   shop's engine (shop_entry() with SHOP_GEAR: engine.h) when it comes up. */
extern unsigned char gear_odds;

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
