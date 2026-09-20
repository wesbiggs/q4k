/* Quest for the King -- buying things. Takes over the screen; the caller
   redraws. `kind` is one of the SHOP_* constants from town_data.h. */

#ifndef Q4K_SHOP_H
#define Q4K_SHOP_H

#define POTION_COST 10

void shop_open(unsigned char kind);

#endif /* Q4K_SHOP_H */
