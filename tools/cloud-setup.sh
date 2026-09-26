#!/bin/sh
# Make a fresh Ubuntu 24.04 machine -- a Claude Code cloud session, or the
# toolchain workflow's runner -- able to build q4k and run its tests:
#
#   cc65 2.19        from Ubuntu's own package, the version CI pins (it
#                    calls itself V2.18, but builds byte for byte the same)
#   VICE 3.10        headless, in /opt/vice, linked into /usr/local/bin
#   flex, bison      what building VICE needs
#   python3-pil      for screendump.py --shot
#
# Ubuntu's VICE package will not do: it is 3.7, it is not the headless
# build, and it leaves out the C64 ROMs. So VICE comes from the toolchain
# release (.github/workflows/toolchain.yml builds it) when there is a token
# that can read this private repo -- Q4K_TOOLCHAIN_TOKEN, GH_TOKEN or
# GITHUB_TOKEN -- and is built from source when there is not, which takes a
# few minutes. Q4K_FROM_SOURCE=1 forces the build.
#
# Self-contained, so it can be pasted whole into an environment's setup
# script. Safe to run again: what is already there is left alone.
set -eu

VICE_VERSION=3.10
PREFIX=/opt/vice
REPO=wesbiggs/q4k
TAG=toolchain-vice$VICE_VERSION
ASSET=vice-$VICE_VERSION-headless.tgz

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
    curl -fsSL -o "$src/vice.tgz" \
        "https://sourceforge.net/projects/vice-emu/files/releases/vice-$VICE_VERSION.tar.gz/download"
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

if ! command -v cl65 >/dev/null 2>&1; then
    say "installing cc65"
    apt_install cc65
fi

if [ ! -x "$PREFIX/bin/x64sc" ]; then
    if [ "${Q4K_FROM_SOURCE:-0}" = 1 ] || ! fetch_prebuilt; then
        build_vice
    fi
fi

# Linked rather than added to PATH, so every shell sees them however it
# was started. mkfont.py follows the link back to $PREFIX to find the
# character ROM.
for tool in x64sc c1541 petcat; do
    $SUDO ln -sf "$PREFIX/bin/$tool" "/usr/local/bin/$tool"
done

python3 -c 'import PIL' 2>/dev/null || apt_install python3-pil

say "$(cl65 --version 2>&1 | head -1)"
say "$(x64sc --version 2>&1 | head -1)"
