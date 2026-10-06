#include <string.h>

#include <q4k/charset.h>

/* Once only: afterwards font_image is the town or dungeon map (local.s),
   and a second copy would draw the map as the font. */
void charset_install(void)
{
    static unsigned char done;

    if (!done) {
        /* Not the glyphs in between: those hold data (see cfg/c64-q4k.cfg).
           The shot's block ($C0-$C7) is drawn afresh by sprite.c anyway. */
        memcpy(CHARSET_RAM, font_image, CHARSET_LOW);
        memcpy(CHARSET_RAM + CHARSET_CURSOR, font_image + CHARSET_CURSOR, 8);
        done = 1;
    }
}

void charset_define(unsigned char code, const unsigned char *bitmap)
{
    memcpy(CHARSET_RAM + ((unsigned int)code << 3), bitmap, 8);
}
