#include <string.h>

#include <q4k/charset.h>

void charset_install(void)
{
    memcpy(CHARSET_RAM, font_image, CHARSET_BYTES);
}

void charset_define(unsigned char code, const unsigned char *bitmap)
{
    memcpy(CHARSET_RAM + ((unsigned int)code << 3), bitmap, 8);
}
