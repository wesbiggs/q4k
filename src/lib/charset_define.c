#include <string.h>

#include <q4k/charset.h>

/* Once only: afterwards font_image is the town or dungeon map (local.s),
   and a second copy would draw the map as the font. */
void charset_install(void)
{
    static unsigned char done;

    if (!done) {
        memcpy(CHARSET_RAM, font_image, CHARSET_BYTES);
        done = 1;
    }
}

void charset_define(unsigned char code, const unsigned char *bitmap)
{
    memcpy(CHARSET_RAM + ((unsigned int)code << 3), bitmap, 8);
}
