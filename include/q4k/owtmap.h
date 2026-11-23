/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's spoils and treasure maps
 * (src/lib/ow_tmap.c), for owtown.
 */

#ifndef Q4K_OWTMAP_H
#define Q4K_OWTMAP_H

/* What a fight won left (OWB_SPOILS: OWB->arg's OWS_*, OWB->npc a roll):
   arms, offered to the party, and a treasure map of the land, on a screen
   of text, until a key. */
void ow_spoils(void);

/* The party screen's M: the treasure maps carried, until a key. */
void tmap_list(void);

#endif /* Q4K_OWTMAP_H */
