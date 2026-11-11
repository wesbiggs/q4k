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
   shop's engine (shop_entry() with SHOP_GEAR: engine.h) when it comes up.
   Offline only. */
extern unsigned char gear_odds;

#endif /* Q4K_SHOP_H */
