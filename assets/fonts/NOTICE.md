# Fonts

Nothing in this directory is checked in. `*.bin` is ignored on purpose: a
character set is somebody's typeface, and most of the interesting ones are
not ours to redistribute.

The game builds its font with `tools/mkfont.py`, which reads either the
machine's own character ROM or a C64 charset binary you supply here. The
committed default is the ROM:

    make assets FONT_SOURCE=rom FONT_STYLE=serif

## aniron.bin — not included, and why

A C64 adaptation by [Patrick Mollohan](https://github.com/patrickmollohan/c64-fonts)
of the **Aniron** typeface, © Pete Klassen, 2004. Klassen's terms:

> You are allowed to distribute this font free of charge only, and only with
> the complete contents of the original archive file included. You are
> allowed to use this font for private use only. Commercial use and/or
> modification of this font are not allowed.

Three things follow, and they are why it is not in this repository:

1. **Modification is not permitted**, and redrawing a typeface as an 8x8
   bitmap is a modification. So is baking that bitmap into a C array, which
   is what `mkfont.py` does.
2. **Private use only.** A game shipped with it could not be sold, and
   arguably could not be published at all.
3. **Distribution must carry the complete original archive.** A 2K derived
   binary is not that.

If you want it for your own private use, fetch it yourself — it will be
picked up and ignored by git:

    curl -sSLo assets/fonts/aniron.bin \
      https://raw.githubusercontent.com/patrickmollohan/c64-fonts/master/custom/aniron.bin
    make assets FONT_SOURCE=assets/fonts/aniron.bin

It is a far better face than anything the ROM styles produce, so it is worth
having locally. It is just not worth shipping.
