; The map of the town or dungeon level you are in.
;
; It has no storage of its own. The font image is 2K of HIDATA that is read
; exactly once, when charset_install() copies it to $3800, and never looked
; at again -- so after that it is the biggest free buffer in the machine,
; and big enough for a dungeon level that scrolls. BSS had no room for one.
;
; The price is an ordering rule: charset_install() before anything writes a
; map, and never again afterwards, or the map is drawn as the font.

        .export _local
        .import _font_image

_local = _font_image
