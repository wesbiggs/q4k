#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Make a fresh Ubuntu 24.04 machine -- a Claude Code cloud session, or the
# toolchain workflow's runner -- able to build q4k and run its tests:
#
#   cc65 2.19        from Ubuntu's own package, the version CI pins (it
#                    calls itself V2.18, but builds byte for byte the same)
#   VICE             one of the three below
#   python3-pil      for screendump.py --shot
#
# VICE, in order of preference:
#
# 1. The headless VICE 3.10 from the toolchain release, into /opt/vice and
#    linked into /usr/local/bin -- exactly what CI runs. Needs a token that
#    can read this private repo: Q4K_TOOLCHAIN_TOKEN, GH_TOKEN or GITHUB_TOKEN.
# 2. Without one, Ubuntu's own VICE (3.7, the GTK build) plus the C64 and
#    drive ROMs out of VICE 3.10's source tarball. The package leaves the ROMs
#    out as non-free, so they are put where it looks, /usr/share/vice/C64 and
#    /usr/share/vice/DRIVES, and checked against the checksums below. The GTK
#    build wants a display, so /usr/local/bin/x64sc is a wrapper that starts
#    Xvfb on :99 when there is no $DISPLAY. The tarball is from SourceForge;
#    if that is blocked, the ROMs come from the project's GitHub mirror
#    (raw.githubusercontent.com, VICE-Team/svn-mirror), the same bytes.
# 3. Q4K_FROM_SOURCE=1 builds the headless VICE 3.10 from source instead
#    (a few minutes, and flex and bison); the toolchain workflow does that.
#
# Self-contained, so it can be pasted whole into an environment's setup
# script. Safe to run again: what is already there is left alone.
set -eu

VICE_VERSION=3.10
PREFIX=/opt/vice
REPO=wesbiggs/q4k
TAG=toolchain-vice$VICE_VERSION
ASSET=vice-$VICE_VERSION-headless.tgz
TARBALL=https://sourceforge.net/projects/vice-emu/files/releases/vice-$VICE_VERSION.tar.gz/download
MIRROR=https://raw.githubusercontent.com/VICE-Team/svn-mirror/main/vice/data
VICEDATA=/usr/share/vice

# The ROMs the emulated machine boots from, as VICE $VICE_VERSION ships them.
ROMS="
83c60d47047d7beab8e5b7bf6f67f80daa088b7a6a27de0d7e016f6484042721  C64/kernal-901227-03.bin
89878cea0a268734696de11c4bae593eaaa506465d2029d619c0e0cbccdfa62d  C64/basic-901226-01.bin
fd0d53b8480e86163ac98998976c72cc58d5dd8eb824ed7b829774e74213b420  C64/chargen-901225-01.bin
d1d45afb46fd4e2b48d93ca367b889d75654a3b7acf73044e51f6d880c09369e  DRIVES/dos1541-325302-01+901229-05.bin
"

SUDO=
[ "$(id -u)" = 0 ] || SUDO=sudo

say() { echo "cloud-setup: $*"; }

# Try without an update first: it is slow, and the image's lists are
# usually fresh enough.
apt_install() {
    $SUDO apt-get install -y -q --no-install-recommends "$@" >/dev/null 2>&1 \
    || { $SUDO apt-get update -q >/dev/null \
         && $SUDO apt-get install -y -q --no-install-recommends "$@" >/dev/null; }
}

fetch_prebuilt() {
    token=${Q4K_TOOLCHAIN_TOKEN:-${GH_TOKEN:-${GITHUB_TOKEN:-}}}
    [ -n "$token" ] || { say "no GitHub token, so no prebuilt VICE"; return 1; }

    url=$(curl -fsSL -H "Authorization: Bearer $token" \
              "https://api.github.com/repos/$REPO/releases/tags/$TAG" \
          | python3 -c 'import json, sys
print([a["url"] for a in json.load(sys.stdin)["assets"]
       if a["name"] == sys.argv[1]][0])' "$ASSET") \
        || { say "no $ASSET in release $TAG"; return 1; }

    # Into a scratch directory first, so a download cut short leaves no
    # half an install behind to be mistaken for a whole one.
    tmp=$(mktemp -d)
    if curl -fsSL -H "Authorization: Bearer $token" \
            -H "Accept: application/octet-stream" "$url" \
       | tar -xz -C "$tmp"; then
        $SUDO mv "$tmp$PREFIX" "$PREFIX"
        rm -rf "$tmp"
        return 0
    fi
    rm -rf "$tmp"
    say "download of $ASSET failed"
    return 1
}

build_vice() {
    say "building VICE $VICE_VERSION from source (a few minutes)"
    apt_install build-essential pkg-config flex bison dos2unix xa65 texinfo curl
    src=$(mktemp -d)
    curl -fsSL -o "$src/vice.tgz" "$TARBALL"
    tar -xzf "$src/vice.tgz" -C "$src"
    (
        cd "$src/vice-$VICE_VERSION"
        ./configure --prefix="$PREFIX" --enable-headlessui \
            --disable-html-docs --without-pulse --without-alsa \
            --without-png --without-libcurl >/dev/null
        make -j"$(nproc)" >/dev/null
        $SUDO make install >/dev/null
    )
    rm -rf "$src"
}

roms_ok() {
    (cd "$VICEDATA" && printf '%s' "$ROMS" | grep . | sha256sum -c --quiet -) \
        >/dev/null 2>&1
}

# Every C64 and drive ROM in the release's data/, so that any model VICE
# is asked for is there; the four in $ROMS are the ones q4k uses.
fetch_roms() {
    tmp=$(mktemp -d)
    if curl -fsSL -o "$tmp/vice.tgz" "$TARBALL" \
       && tar -xzf "$tmp/vice.tgz" -C "$tmp" --wildcards \
              "vice-$VICE_VERSION/data/C64/*.bin" \
              "vice-$VICE_VERSION/data/DRIVES/*.bin"; then
        say "ROMs from the VICE $VICE_VERSION source tarball"
        for d in C64 DRIVES; do
            $SUDO mkdir -p "$VICEDATA/$d"
            $SUDO cp "$tmp/vice-$VICE_VERSION/data/$d/"*.bin "$VICEDATA/$d/"
        done
    else
        say "no tarball from SourceForge; ROMs from VICE's GitHub mirror"
        printf '%s' "$ROMS" | grep . | while read -r _ rom; do
            $SUDO mkdir -p "$VICEDATA/${rom%/*}"
            curl -fsSL "$MIRROR/$rom" | $SUDO tee "$VICEDATA/$rom" >/dev/null
        done
    fi
    rm -rf "$tmp"
    roms_ok || { say "the ROMs do not match their checksums"; exit 1; }
}

# Ubuntu's x64sc is the GTK build and will not start without a display.
# Started once and left running; exec keeps the pid, so screendump.py's
# kill reaches the emulator itself.
install_wrapper() {
    $SUDO tee /usr/local/bin/x64sc >/dev/null <<'WRAP'
#!/bin/sh
# Ubuntu's x64sc, on a virtual display when there is no real one
# (written by tools/cloud-setup.sh).
if [ -z "${DISPLAY:-}" ]; then
    export DISPLAY=:99
    if [ ! -S /tmp/.X11-unix/X99 ] || ! pgrep -x Xvfb >/dev/null 2>&1; then
        rm -f /tmp/.X99-lock /tmp/.X11-unix/X99
        setsid Xvfb :99 -screen 0 1024x768x24 -nolisten tcp \
            >/dev/null 2>&1 </dev/null &
    fi
    i=0
    while [ ! -S /tmp/.X11-unix/X99 ] && [ $i -lt 100 ]; do
        sleep 0.1; i=$((i + 1))
    done
fi
exec /usr/bin/x64sc "$@"
WRAP
    $SUDO chmod 755 /usr/local/bin/x64sc
}

if ! command -v cl65 >/dev/null 2>&1; then
    say "installing cc65"
    apt_install cc65
fi

if [ ! -x "$PREFIX/bin/x64sc" ]; then
    if [ "${Q4K_FROM_SOURCE:-0}" = 1 ]; then
        build_vice
    elif ! fetch_prebuilt; then
        [ -x /usr/bin/x64sc ] || { say "installing Ubuntu's VICE"; \
            apt_install vice xvfb curl; }
        command -v Xvfb >/dev/null 2>&1 || apt_install xvfb
        roms_ok || fetch_roms
        install_wrapper
    fi
fi

# Linked rather than added to PATH, so every shell sees them however it
# was started. mkfont.py follows the link back to $PREFIX to find the
# character ROM (with Ubuntu's VICE, it finds it in /usr/share/vice).
if [ -x "$PREFIX/bin/x64sc" ]; then
    for tool in x64sc c1541 petcat; do
        $SUDO ln -sf "$PREFIX/bin/$tool" "/usr/local/bin/$tool"
    done
fi

python3 -c 'import PIL' 2>/dev/null || apt_install python3-pil

say "$(cl65 --version 2>&1 | head -1)"
say "$(x64sc --version 2>&1 | grep -m1 -i vice)"
