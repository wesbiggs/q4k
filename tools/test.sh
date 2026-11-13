#!/bin/sh
# Regression tests: run each case headlessly and compare the C64 screen
# against a golden text dump. Refresh goldens with tools/test.sh --bless,
# and only after reading the diff.
#
# A case is tests/golden/<name>.txt plus an optional <name>.cmd holding the
# screendump arguments. With no .cmd, the case just runs dist/<name>.prg.
set -eu
cd "$(dirname "$0")/.."

bless=0
[ "${1:-}" = "--bless" ] && bless=1
fail=0

# compare <name> <golden> <command...>
compare() {
    name=$1
    golden=$2
    shift 2

    # Capture the failure rather than letting `set -e` abort the whole run:
    # one case that will not boot should be one FAIL, not silence.
    if ! actual=$("$@" 2>/dev/null); then
        echo "FAIL  $name (did not complete)"
        fail=1
        return
    fi

    if [ "$bless" = 1 ]; then
        printf '%s\n' "$actual" > "$golden"
        echo "BLESS $name"
    elif [ "$actual" = "$(cat "$golden" 2>/dev/null)" ]; then
        echo "PASS  $name"
    else
        echo "FAIL  $name"
        printf '%s\n' "$actual" | diff -u "$golden" - || true
        fail=1
    fi
}

# Cheap and worth doing first: a hand-edited overworld can seal a town behind
# the mountains, and no screen test would ever notice.
if tools/checkmap.py >/dev/null 2>&1; then
    echo "PASS  checkmap"
else
    echo "FAIL  checkmap"
    tools/checkmap.py || true
    fail=1
fi

# The continents are generated, so no screen test would notice a square a
# mountain had cut off from the landing -- only searching every one does. The
# model of the generator is searched over every game seed and compass point,
# then the C64's own continents are held up against the model's.
if [ -f dist/isletest.prg ]; then
    if tools/checkisle.py --machine-seeds 16 >/dev/null 2>&1; then
        echo "PASS  checkisle"
    else
        echo "FAIL  checkisle"
        tools/checkisle.py --machine-seeds 16 || true
        fail=1
    fi
fi

# The dialogue gates are decided host-side, and what matters about them --
# that the giver never knows the word, that a guess does not pass -- never
# shows on a screen.
if python3 host/test_npcd.py >/dev/null 2>&1; then
    echo "PASS  npcd"
else
    echo "FAIL  npcd"
    python3 host/test_npcd.py || true
    fail=1
fi

for golden in tests/golden/*.txt; do
    name=$(basename "$golden" .txt)
    if [ -f "tests/golden/$name.cmd" ]; then
        # shellcheck disable=SC2046
        set -- $(cat "tests/golden/$name.cmd")
    else
        set -- "dist/$name.prg"
    fi
    [ -f "$1" ] || { echo "SKIP  $name (no $1 -- run make)"; continue; }
    compare "$name" "$golden" tools/screendump.py "$@"
done

# The game loads its engines from its disk (include/q4k/engine.h), so every
# case that goes past the title screen has it in unit 8: the .cmd files say
# --disk dist/q4k.d64, and the offline cases use dist/offline/q4k.d64.
DISK=dist/q4k.d64

# Saving and restoring writes to that disk. A fresh copy each run, so the
# case does not depend on what a previous run left lying about: walk two
# squares, save, walk two more. The load case then restores from it.
#
# The save case used to press F3 and restore as well, but that half was
# flaky: the screen is captured after a fixed --settle, and the restore
# sometimes had not finished ("Loading..." on screen, no "Restored.", the
# hero still two squares on). It failed that way in CI every time. The load
# case restores from the title screen on a fresh machine, which covers it.
#
# It walks north inside Millford rather than out onto the road. Overworld
# squares roll for wandering monsters, and now that the seed comes from the
# player's own timing the roll differs every run -- any test that walks in
# the open is flaky by construction. Towns never roll.
if [ -f tests/golden-save.txt ] && [ -f dist/q4k.prg ] && [ -f "$DISK" ]; then
    # A full template: BSD mktemp takes -t <prefix>, GNU rejects it.
    base=$(mktemp "${TMPDIR:-/tmp}/q4ksave.XXXXXX")
    saves=$base.d64
    cp "$DISK" "$saves"
    compare save tests/golden-save.txt \
        tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys 'Alaric\rww\x85yww' --settle 8
    # The same disk now holds a save: restore it from the title screen with
    # F3, and start back where it was made rather than in a new morning.
    if [ -f tests/golden-load.txt ]; then
        compare load tests/golden-load.txt \
            tools/screendump.py dist/q4k.prg --disk "$saves" \
            --keys '\x86' --settle 8
    fi
    rm -f "$saves" "$base"

    # Saving twice must replace the first save. Scratching the old file used
    # to fail silently, so the second save said "Saved." and wrote nothing,
    # and a restore came back at the first. No screen shows the difference,
    # so compare the turn counter instead: two saves, two squares apart,
    # then restore on a fresh machine and expect the later turn.
    base=$(mktemp "${TMPDIR:-/tmp}/q4ksave.XXXXXX")
    saves=$base.d64
    cp "$DISK" "$saves"
    turn() { sed -n 's/.*game_turn+0 @ .* = \([0-9]*\) .*/\1/p'; }
    saved=$(tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys 'Alaric\rww\x85yww\x85y' --settle 12 --peek game_turn:2 \
        2>&1 >/dev/null | turn) || true
    restored=$(tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys '\x86' --settle 8 --peek game_turn:2 \
        2>&1 >/dev/null | turn) || true
    if [ -n "$saved" ] && [ "$saved" = "$restored" ]; then
        echo "PASS  resave"
    else
        echo "FAIL  resave (saved at turn ${saved:-?}, restored turn ${restored:-?})"
        fail=1
    fi
    rm -f "$saves" "$base"
fi

# Home is made from the game's seed, so the tests that go somewhere on it walk
# there, with keys worked out from the model of the generator.
SEED=0x5a
keys() { python3 tools/homepath.py "$SEED" "$@"; }

# The dialogue case needs a daemon on the other end of the modem line, in
# --mock mode so the replies are the same every run. With the modem the
# machine runs at its real speed, and screendump.py serves the disk without
# emulating the drive, or every conversation would wait a minute for its
# engine.
if [ -f tests/golden-dialog.txt ] && [ -f dist/q4k.prg ] && [ -f "$DISK" ]; then
    python3 host/npcd.py --mock --port 25299 --quiet >/dev/null 2>&1 &
    daemon=$!
    sleep 1
    compare dialog tests/golden-dialog.txt \
        tools/screendump.py dist/q4k.prg --disk "$DISK" --rs232 25299 \
        --keys 'Alaric\rwwwwwwwwwwwwaaaawaaawhat of the blade\rcould I have a dagger\r' --settle 40
    # Brann -> Vane: sail to Wardlow (the south continent, for seed $5A)
    # holding the Emberstone, with the run's secrets pinned (that seed makes
    # the fishers' word "dead low").
    # Asking for the key is refused; saying the word earns it, announced in
    # the transcript. Saying goodbye then has Vane end the conversation
    # (mock mode's BYE), which waits for a key. Stepping onto a town square
    # never rolls for a monster, so this is not a walk in the open.
    if [ -f tests/golden-tidal.txt ]; then
        compare tidal tests/golden-tidal.txt \
            tools/screendump.py dist/q4k.prg --disk "$DISK" --rs232 25299 \
            --poke quest_stage=3 --poke items=8 --poke lore_seed=$SEED \
            --poke no_encounters=1 \
            --keys 'Alaric\rs<'"$(keys millford port2)$(keys dock story --land 2)"'>wwwwwwwaaaawwwmay I have the key\rdead low\rgoodbye\r' \
            --settle 100
    fi
    # The king, from a new game that walks to the castle: neither his
    # greeting nor a "hello" hands over the Sigil; asking about the quest
    # has him say so, and it arrives in the transcript.
    if [ -f tests/golden-sigil.txt ]; then
        compare sigil tests/golden-sigil.txt \
            tools/screendump.py dist/q4k.prg --disk "$DISK" --rs232 25299 \
            --poke quest_stage=1 --poke lore_seed=$SEED --poke no_encounters=1 \
            --keys 'Alaric\rs<'"$(keys millford castle)"'>wwwwwwwwwwwhello\rwhat is my quest\r' \
            --settle 100
    fi
    # The king after Maldrath is banished (stage 9): the first word pays the
    # 1,000 gold and moves the quest to Q_REWARDED, the next only praises.
    if [ -f tests/golden-reward.txt ]; then
        compare reward tests/golden-reward.txt \
            tools/screendump.py dist/q4k.prg --disk "$DISK" --rs232 25299 \
            --poke quest_stage=9 --poke lore_seed=$SEED --poke no_encounters=1 \
            --keys 'Alaric\rs<'"$(keys millford castle)"'>wwwwwwwwwwwhello\rthank you\r' \
            --settle 100
    fi
    # The inn in Millford, wounded: an ale brings a hint from the daemon's
    # deck (mock mode shuffles it the same way every run), and a bed puts
    # the hero back to full health for 20 marks.
    if [ -f tests/golden-inn.txt ]; then
        compare inn tests/golden-inn.txt \
            tools/screendump.py dist/q4k.prg --disk "$DISK" --rs232 25299 \
            --poke party+4=5 --poke gold=60 --poke lore_seed=$SEED --keys 'Alaric\rwwddddd12' --settle 24
    fi
    kill "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true   # reap quietly; no job-control noise
fi

# A fight, there and back, in either build: the dead rise at the first L on
# the road out of Millford (grave_odds=1), the hero has one point of health
# and only guards (SPACE), so the first blow kills, and a key later they wake
# in Millford. That loads the fight's engine over the map's, and the map's
# back over it, with the map's own state (the scene, the log) intact.
# fight <name> <prg> <disk>
fight() {
    out=$(tools/screendump.py "$2" --disk "$3" \
        --poke quest_stage=1 --poke lore_seed=$SEED --poke grave_odds=1 \
        --poke party+4=1 \
        --keys 'Alaric\rs<l                                                            ' \
        --settle 40 2>/dev/null) || out=
    if printf '%s\n' "$out" | grep -q 'You fall\.' \
            && printf '%s\n' "$out" | grep -q 'You wake in Millford' \
            && ! printf '%s\n' "$out" | grep -q 'Put the game disk back'; then
        echo "PASS  $1"
    else
        echo "FAIL  $1"
        printf '%s\n' "$out"
        fail=1
    fi
}
if [ -f dist/q4k.prg ] && [ -f "$DISK" ]; then
    fight fight dist/q4k.prg "$DISK"
fi

# The offline build (make d64-offline): no modem, and what people say comes
# off the disk. The title screen has no host to call.
OFF=dist/offline/q4k.prg
OFFDISK=dist/offline/q4k.d64
if [ -f "$OFF" ] && [ -f "$OFFDISK" ]; then
    if [ -f tests/golden-offline-title.txt ]; then
        compare offline-title tests/golden-offline-title.txt \
            tools/screendump.py "$OFF"
    fi
    # Garrett at stage 0 has one greeting. Asking for the dagger hands it
    # over in the transcript and takes the topic off the renumbered menu, so
    # 3 is then Maldrath.
    if [ -f tests/golden-offline-talk.txt ]; then
        compare offline-talk tests/golden-offline-talk.txt \
            tools/screendump.py "$OFF" --disk "$OFFDISK" \
            --keys 'Alaric\rwwwwwwwwwwwwaaaawaaa13' --settle 30
    fi
    # With no disk there is no map's engine to load after the title screen
    # (engine.h), so the game asks for the disk back, and keeps asking.
    out=$(tools/screendump.py "$OFF" --keys 'Alaric\rw' --settle 8 \
        2>/dev/null) || out=
    if printf '%s\n' "$out" | grep -q 'Put the game disk back'; then
        echo "PASS  offline-nodisk"
    else
        echo "FAIL  offline-nodisk"
        printf '%s\n' "$out"
        fail=1
    fi
    # The networked build's screen tests, played offline: everything but the
    # title screen is the same. A golden of its own,
    # tests/golden-offline-<name>.txt, wins where the two builds differ.
    for name in q4k fov loot sail sailhome captain treasure treasuremap; do
        golden=tests/golden-offline-$name.txt
        [ -f "$golden" ] || golden=tests/golden/$name.txt
        # shellcheck disable=SC2046
        set -- $(sed "s|dist/q4k\.|dist/offline/q4k.|g" \
            "tests/golden/$name.cmd")
        compare "offline-$name" "$golden" tools/screendump.py "$@"
    done
    fight offline-fight "$OFF" "$OFFDISK"
    # The king at stage 9: his greeting is the line that gives, so the
    # reward comes before anything is asked, and the menu is stage 10's.
    if [ -f tests/golden-offline-reward.txt ]; then
        compare offline-reward tests/golden-offline-reward.txt \
            tools/screendump.py "$OFF" --disk "$OFFDISK" \
            --poke quest_stage=9 --poke lore_seed=$SEED --poke no_encounters=1 \
            --keys 'Alaric\rs<'"$(keys millford castle)"'>wwwwwwwwwww1' \
            --settle 90
    fi
    # Two ales in Millford's inn. Which rumours is random, so this checks
    # only that two were told and the keeper was never "busy" (no file).
    out=$(tools/screendump.py "$OFF" --disk "$OFFDISK" --poke gold=60 \
        --keys 'Alaric\rwwddddd11' --settle 30 2>/dev/null) || out=
    drinks=$(printf '%s\n' "$out" | grep -c 'You drink\.') || true
    if [ "$drinks" = 2 ] && ! printf '%s\n' "$out" | grep -q 'keeper is busy' \
            && printf '%s\n' "$out" | grep -q 'You have 50 marks'; then
        echo "PASS  offline-inn"
    else
        echo "FAIL  offline-inn"
        printf '%s\n' "$out"
        fail=1
    fi
fi

exit $fail
