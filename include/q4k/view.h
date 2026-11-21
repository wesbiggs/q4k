/*
 * q4k -- the text screen's layout, as the screens of text use it.
 *
 *   cols  0-20, rows 0-18   once the map's viewport
 *   col   21,   rows 0-18   separator
 *   cols 22-39, rows 0-18   panel
 *   row  19                 rule
 *   rows 20-24              message log
 */

#ifndef Q4K_VIEW_H
#define Q4K_VIEW_H

#define SCR_W       40
#define SCR_H       25
#define SCREEN_RAM  ((unsigned char *)0x0400)

#define VIEW_W      21
#define VIEW_H      19
#define VIEW_CX     10  /* hero column within the viewport */
#define VIEW_CY      9  /* hero row within the viewport    */

#define SEP_X       21
#define PANEL_X     22
#define PANEL_W     18

#define RULE_Y      19
#define MSG_Y       20
#define MSG_H        5

/* A line for the map's log, which the screens of text have none of
   (src/ow/owstand.c drops it). */
void view_message(const char *text);

#endif /* Q4K_VIEW_H */
