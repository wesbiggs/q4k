#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Make a fresh Ubuntu 24.04 machine -- a Claude Code cloud session, say --
# able to build q4k and run its tests: cc65 and VICE from
# wesbiggs/c64-cloud-toolchain at the release CI pins (TOOLCHAIN, the same
# tag as .github/workflows/test.yml's), and python3-pil for
# screendump.py --shot. That repository's README says what the script does
# and how it falls back when GitHub's release downloads are blocked.
#
# A checkout of the toolchain beside this one (../c64-cloud-toolchain) is
# used instead of downloading the script, so a change to it can be tried
# here first. Safe to run again.
set -eu

TOOLCHAIN=v1.0.0
URL=https://raw.githubusercontent.com/wesbiggs/c64-cloud-toolchain/$TOOLCHAIN/setup.sh
EXTRAS=python3-pil

SUDO=
[ "$(id -u)" = 0 ] || SUDO=sudo

local=$(cd "$(dirname "$0")/../.." && pwd)/c64-cloud-toolchain/setup.sh
if [ -f "$local" ]; then
    $SUDO sh "$local" $EXTRAS
else
    curl -fsSL "$URL" | $SUDO sh -s -- $EXTRAS
fi
