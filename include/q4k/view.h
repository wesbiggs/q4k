/*
 * q4k -- screen layout and drawing.
 *
 *   cols  0-20, rows 0-18   map viewport, hero always at the centre
 *   col   21,   rows 0-18   separator
 *   cols 22-39, rows 0-18   status panel
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

void view_init(void);

/* Draw whichever scene the player is standing in. */
void view_draw(void);

/* Redraw the frame after another screen (the dialogue view) took over. */
void view_chrome(void);
void view_restore(void);
void view_draw_map(void);
void view_draw_panel(void);

/* Set while the town on screen is the town as it stands. A town is one
   viewport and never scrolls, and the hero is a sprite, so view_draw()
   leaves it alone until something clears this: view_restore(), drawing any
   other scene, or map_set(). Redrawing it every step had the ground under
   each person on screen for a frame before the person went over it. */
extern unsigned char town_drawn;

/* Set for the length of a fight: view_draw_panel() then leaves rows 10 to
   12, where the foe is named, alone. */
extern unsigned char view_fighting;

/* Append a line to the log, scrolling the oldest one off the top. */
void view_message(const char *text);

#endif /* Q4K_VIEW_H */
