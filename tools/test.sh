#!/bin/sh
# Regression tests: run each case headlessly and compare the C64 screen
# against a golden text dump. Refresh goldens with tools/test.sh --bless,
# and only after reading the diff.
#
# A golden case is tests/golden/<name>.txt plus an optional <name>.cmd
# holding the screendump arguments. With no .cmd, the case just runs
# dist/<name>.prg. The rest are units of their own, below.
#
#   tools/test.sh [--bless] [-j N] [--group G] [NAME...]
#
# Every unit is its own VICE (or none), so they run side by side, N at a
# time (-j, default the number of CPUs; -j 1 for one after another). Each
# prints its result when it ends, with how long it took; a summary of the
# slowest and of any failures comes last. --group runs one of three thirds
# (game, openworld, online: group_of() below), which is how CI splits the
# suite over three machines; NAMEs run just those units; --list prints them.
set -eu
cd "$(dirname "$0")/.."
self=tools/test.sh

bless=0
jobs=
group=
one=
list=0
names=
while [ $# -gt 0 ]; do
    case $1 in
        --bless) bless=1 ;;
        -j) jobs=$2; shift ;;
        -j*) jobs=${1#-j} ;;
        --group) group=$2; shift ;;
        --one) one=$2; shift ;;
        --list) list=1 ;;
        -*) echo "test.sh: unknown option $1" >&2; exit 2 ;;
        *) names="$names $1" ;;
    esac
    shift
done
[ -n "$jobs" ] || jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

# The game loads its engines from its disk (include/q4k/engine.h), so every
# case that goes past the title screen has it in unit 8: the .cmd files say
# --disk dist/q4k.d64, and the experimental networked build's cases use
# dist/online/q4k.d64.
DISK=dist/q4k.d64
ON=dist/online/q4k.prg
ONDISK=dist/online/q4k.d64

# Home is made from the game's seed, so the tests that go somewhere on it walk
# there, with keys worked out from the model of the generator.
SEED=0x5a
keys() { python3 tools/homepath.py "$SEED" "$@"; }

# The game's screen tests that the networked build plays as well.
ONLINE_GOLDENS="q4k fov loot sail sailhome captain treasure treasuremap"

# --- reporting ----------------------------------------------------------------

t0=$(date +%s)

# result PASS|FAIL|SKIP|BLESS <name> [why]: one line, with the seconds since
# the unit started.
result() {
    printf '%-5s %s  [%ss]%s\n' "$1" "$2" $(($(date +%s) - t0)) "${3:+  ($3)}"
}

# compare <name> <golden> <screendump command...>
#
# The golden goes in as --expect too, so a --settle ends as soon as the
# screen is the golden (screendump.py's settle()): a case that passes takes
# as long as it needs, one that fails the whole --settle. Not when blessing,
# which wants the screen at the end of the settle.
compare() {
    name=$1
    golden=$2
    shift 2
    [ "$bless" = 1 ] || set -- "$@" --expect "$golden"

    # Capture the failure rather than letting `set -e` abort the whole run:
    # one case that will not boot should be one FAIL, not silence.
    if ! actual=$("$@" 2>/dev/null); then
        result FAIL "$name" "did not complete"
        return
    fi

    if [ "$bless" = 1 ]; then
        printf '%s\n' "$actual" > "$golden"
        result BLESS "$name"
    elif [ "$actual" = "$(cat "$golden" 2>/dev/null)" ]; then
        result PASS "$name"
    else
        result FAIL "$name"
        printf '%s\n' "$actual" | diff -u "$golden" - || true
    fi
}

# A dialogue daemon of the unit's own, in --mock mode so the replies are the
# same every run. One each, on a port of its own: a daemon remembers what
# each person has said, and deals the inns' hints from one seeded deck, so
# two cases sharing one would hear each other's lines.
daemon=
daemon_start() {
    python3 host/npcd.py --mock --port "$1" --quiet >/dev/null 2>&1 &
    daemon=$!
    sleep 1
}
daemon_stop() {
    kill "$daemon" 2>/dev/null || true
    wait "$daemon" 2>/dev/null || true   # reap quietly; no job-control noise
}

# --- the units ----------------------------------------------------------------

# A golden: tests/golden/<name>.txt, with the arguments in <name>.cmd.
u_golden() {
    name=$1
    golden=tests/golden/$name.txt
    if [ -f "tests/golden/$name.cmd" ]; then
        # shellcheck disable=SC2046
        set -- $(cat "tests/golden/$name.cmd")
    else
        set -- "dist/$name.prg"
    fi
    [ -f "$1" ] || { result SKIP "$name" "no $1 -- run make"; return; }
    compare "$name" "$golden" tools/screendump.py "$@"
}

# The networked build's own title screen (it calls the host, and says that
# nobody answered), and the game's screen tests played in it: everything else
# is the same. A golden of its own, tests/golden-online-<name>.txt, wins where
# the two builds differ.
u_online_golden() {
    name=$1
    [ -f "$ON" ] && [ -f "$ONDISK" ] || { result SKIP "online-$name" "no $ONDISK"; return; }
    golden=tests/golden-online-$name.txt
    [ -f "$golden" ] || golden=tests/golden/$name.txt
    # shellcheck disable=SC2046
    set -- $(sed "s|dist/q4k\.|dist/online/q4k.|g" "tests/golden/$name.cmd")
    compare "online-$name" "$golden" tools/screendump.py "$@"
}

u_online_title() {
    [ -f "$ON" ] || { result SKIP online-title "no $ON"; return; }
    compare online-title tests/golden-online-title.txt tools/screendump.py "$ON"
}

# Cheap and worth doing first: a hand-edited overworld can seal a town behind
# the mountains, and no screen test would ever notice.
u_checkmap() {
    if tools/checkmap.py >/dev/null 2>&1; then
        result PASS checkmap
    else
        result FAIL checkmap
        tools/checkmap.py || true
    fi
}

# The continents are generated, so no screen test would notice a square a
# mountain had cut off from the landing -- only searching every one does. The
# model of the generator is searched over every game seed and compass point,
# then the C64's own continents are held up against the model's.
u_checkisle() {
    [ -f dist/isletest.prg ] || { result SKIP checkisle "no dist/isletest.prg"; return; }
    if tools/checkisle.py --machine-seeds 16 >/dev/null 2>&1; then
        result PASS checkisle
    else
        result FAIL checkisle
        tools/checkisle.py --machine-seeds 16 || true
    fi
}

# The open world's roads on home (src/lib/ow_roads.s) are laid by the
# bootstrap, and only a few of their squares are ever on a golden's screen:
# the model (tools/roadref.py) is searched over every seed for a place left
# out or a road on a mountain or water, then the C64's home, roads and all,
# is held up against the model's for the quick start and seed 198 (where a
# way back once went into the place it came from).
u_ow_roads() {
    if tools/roadref.py --check 256 >/dev/null 2>&1 \
            && tools/roadref.py --machine 198 >/dev/null 2>&1; then
        result PASS openworld-roads
    else
        result FAIL openworld-roads
        tools/roadref.py --check 256 | tail -5 || true
        tools/roadref.py --machine 198 || true
    fi
}

# The dialogue gates are decided host-side, and what matters about them --
# that the giver never knows the word, that a guess does not pass -- never
# shows on a screen.
u_npcd() {
    if python3 host/test_npcd.py >/dev/null 2>&1; then
        result PASS npcd
    else
        result FAIL npcd
        python3 host/test_npcd.py || true
    fi
}

# The arrow is one sprite block, filled from fight_arrow[] for the way it
# flies (src/fight/fight.c's arrow_to()): the shore case's party shoots
# up-right, and the block holds that picture after. --peek prints the last
# byte first, so this is the block's 48 bytes backwards.
u_arrow() {
    [ -f dist/owarena.prg ] && [ -f dist/owarena.d64 ] \
        || { result SKIP owarena-arrow-ur "no dist/owarena.d64"; return; }
    got=$(tools/screendump.py dist/owarena.prg --disk dist/owarena.d64 \
        --mux build/fight.lbl --poke auto_seed=7 --stage 3 --timeout 180 \
        --keys '3hw\x5ee' --peek '$5580:48' 2>&1 >/dev/null \
        | sed -n 's/.*(\$\([0-9A-F]*\)).*/\1/p')
    want=000000000000000040000024000060000008000008000002000002008000008200002200000A0000AA00000000000000
    if [ "$got" = "$want" ]; then
        result PASS owarena-arrow-ur
    else
        result FAIL owarena-arrow-ur "the arrow's block: ${got:-?}"
    fi
}

# Saving and restoring writes to that disk. A fresh copy each run, so the
# case does not depend on what a previous run left lying about: walk two
# squares, save, walk two more. The load case then restores from it, so the
# two are one unit.
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
u_save() {
    [ -f dist/q4k.prg ] && [ -f "$DISK" ] || { result SKIP save "no $DISK"; return; }
    # A full template: BSD mktemp takes -t <prefix>, GNU rejects it.
    base=$(mktemp "${TMPDIR:-/tmp}/q4ksave.XXXXXX")
    saves=$base.d64
    cp "$DISK" "$saves"
    compare save tests/golden-save.txt \
        tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys 'Alaric\r1nww\x85yww' --settle 8
    # The same disk now holds a save: restore it from the title screen with
    # F3, and start back where it was made rather than in a new morning.
    compare load tests/golden-load.txt \
        tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys '\x86' --settle 8
    rm -f "$saves" "$base"
}

# Saving twice must replace the first save. Scratching the old file used
# to fail silently, so the second save said "Saved." and wrote nothing,
# and a restore came back at the first. No screen shows the difference,
# so compare the turn counter instead: two saves, two squares apart,
# then restore on a fresh machine and expect the later turn.
u_resave() {
    [ -f dist/q4k.prg ] && [ -f "$DISK" ] || { result SKIP resave "no $DISK"; return; }
    base=$(mktemp "${TMPDIR:-/tmp}/q4ksave.XXXXXX")
    saves=$base.d64
    cp "$DISK" "$saves"
    turn() { sed -n 's/.*game_turn+0 @ .* = \([0-9]*\) .*/\1/p'; }
    saved=$(tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys 'Alaric\r1nww\x85yww\x85y' --settle 12 --peek game_turn:2 \
        2>&1 >/dev/null | turn) || true
    restored=$(tools/screendump.py dist/q4k.prg --disk "$saves" \
        --keys '\x86' --settle 8 --peek game_turn:2 \
        2>&1 >/dev/null | turn) || true
    if [ -n "$saved" ] && [ "$saved" = "$restored" ]; then
        result PASS resave
    else
        result FAIL resave "saved at turn ${saved:-?}, restored turn ${restored:-?}"
    fi
    rm -f "$saves" "$base"
}

# The open world saves the same way, its game block into OW-SAVE (F1), and
# F3 goes back through the bootstrap to the saved game's world. Two steps
# east of Millford, save, three more, restore: back at the first square.
# On a copy of the disk, since the save writes it and openworld-load wants
# the save the disk comes with.
u_ow_save() {
    base=$(mktemp "${TMPDIR:-/tmp}/q4kow.XXXXXX")
    saves=$base.d81
    cp dist/openworld.d81 "$saves"
    wxof() { sed -n 's/.*wx+0 @ .* = \([0-9]*\) .*/\1/p'; }
    saved=$(tools/screendump.py dist/openworld.prg --disk "$saves" \
        --splash quick --lbl build/ow-cpu.lbl --keys 'dd\x85yddd' \
        --settle 20 --peek wx:2 2>&1 >/dev/null | wxof) || true
    restored=$(tools/screendump.py dist/openworld.prg --disk "$saves" \
        --splash quick --lbl build/ow-cpu.lbl --keys '\x86y' \
        --settle 40 ${saved:+--until-peek wx:2=$((saved - 3))} --peek wx:2 2>&1 >/dev/null | wxof) || true
    if [ -n "$saved" ] && [ "$restored" = $((saved - 3)) ]; then
        result PASS openworld-save
    else
        result FAIL openworld-save "left at ${saved:-?}, restored at ${restored:-?}"
    fi
    rm -f "$saves" "$base"
}

# P: Alaric, hurt to 5, drinks the quick start's one draught (12 back,
# its floor) and the second P finds none.
u_ow_quaff() {
    hp=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke party+2=5 --keys pp \
        --settle 4 --until-peek party+2=17 --peek party+2 2>&1 >/dev/null \
        | sed -n 's/.*party+2 @ .* = \([0-9]*\) .*/\1/p') || true
    if [ "$hp" = 17 ]; then
        result PASS openworld-quaff
    else
        result FAIL openworld-quaff "health ${hp:-?}, not 17"
    fi
}

# I, then cursor down twice: Alaric's colour (14, light blue, from his
# name) goes to 15 and round past black to 1, and any other key goes
# back to the engine, whose hero is drawn in it (sm_tunic). The j's are
# padding while owtown loads (it empties the buffer as it starts).
u_ow_recolour() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl \
        --keys 'ijjjjjjjjj\x11\x11j' --settle 15 \
        --peek '$CECA' --peek sm_tunic \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "1 1 " ]; then
        result PASS openworld-recolour
    else
        result FAIL openworld-recolour "colour in the block, the hero's: ${got:-?}; not 1 1"
    fi
}

# A fight: enc_always makes the second step's roll one, a goblin on the
# grass; Alaric stands firm (six SPACEs) until it comes to him, from
# above, then strikes up until it is over; any key leaves the screen
# that says what came of it, and the engine is back. The dice come from
# the game's turn, so it goes the same way every time: won, 4
# experience, 6 marks, 4 health left. (Where it comes from follows the
# dice and the field's scatter: a change to either can need the keys
# worked out again, with --shot.) The j's are padding (no key in the
# engine or the fight), while owtown and the module load.
u_ow_fight() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke enc_always=1 \
        --keys "ddjjjjjjjjjjjjjjjjjjjj      $(printf 'w%.0s' $(seq 20))" \
        --settle 30 --until-peek party+2=4 --until-peek party+88:2=4 \
        --until-peek party+90:2=15 \
        --peek party+2 --peek party+88:2 --peek party+90:2 \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "4 4 15 " ]; then
        result PASS openworld-fight
    else
        result FAIL openworld-fight "health, experience, gold: ${got:-?}; not 4 4 15"
    fi
}

# Into the Hollow Crown from the quick start, for the dungeon's units.
ow_enter=dddssssssssssssssssdsssssaasaaaaaaa

# A dungeon's monsters (src/ow/dungeon.c), which the quick start's
# OWG_CALM keeps out but enc_always=2 lets in: into the Hollow Crown and
# west along its first level, where a rat (monster 0, at 22,14) sees
# Alaric from the seventh step and comes up the corridor, a square a
# turn, until it is beside him after the eighth: a fight, which he waits
# out and wins by walking into it. Then it is dead in the game block
# (its kind OWG_FOE_DEAD, at 24,12: $D8), gone from the map the engine
# comes back to, and its blood is on that square ($92 in WORLD). The placing
# follows the game's turn and the level, so a change to the generator,
# the walk or the dice can want the keys worked out again, with --shot.
u_ow_foes() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke still=1 \
        --poke enc_always=2 \
        --keys "${ow_enter}aaaaaaaa$(printf 'j%.0s' $(seq 20))     eeeeeezzzz$(printf 'j%.0s' $(seq 20))" \
        --settle 30 --until-peek '$CECF=216' --until-peek sm_npc_x=0 \
        --until-peek party+88:2=2 --until-peek '$B318=146' \
        --peek '$CECF' --peek sm_npc_x --peek party+88:2 --peek '$B318' \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "216 0 2 146 " ]; then
        result PASS openworld-foes
    else
        result FAIL openworld-foes "rat in the block, on the map, experience, the stain: ${got:-?}; not 216 0 2 146"
    fi
}

# The keeper: down the Hollow Crown to the Gaoler, who stands on the
# bottom level while the party has no Emberstone (the quick start has
# every stone, so items=55 takes it away), and walk into him: the fight
# is asked for (OWB: OWB_FIGHT, the dungeon, OWF_KEEPER). The keys come
# from the levels' maps (dungeon_build() and the quick start's seed).
u_ow_keeper() {
    ow_down=aaaaaaaaaaaaaaaaaaaazssssssssssaaaaaaaaaaaaaaaaaaaaaaaazssssssss
    ow_bump=dddddddewwwweeweddddddddddddddddxxxdddddewwwwwww
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke still=1 --poke items=55 \
        --keys "${ow_enter}${ow_down}${ow_bump}$(printf 'j%.0s' $(seq 20))" \
        --stage 3 --peek '$02ED:3' \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p')
    if [ "$got" = 65545 ]; then     # $010009
        result PASS openworld-keeper
    else
        result FAIL openworld-keeper "hand-off ${got:-?}, not 65545: \$010009"
    fi
}

# The REU load cache (src/lib/ow_cache.s): into Millford and out with an
# REU that starts empty, so the world's overlay and its tile set come back
# from the REU (cache_hits), on the screen openworld-out has; then OW-CPU
# with an REU (--cpu-engine) that starts full (--reucache, the image make
# writes), which loads nothing from the disk at all (cache_misses 0).
u_ow_cache() {
    [ -f build/owcache.reu ] || { result SKIP openworld-cache "no build/owcache.reu"; return; }
    log=$(mktemp "${TMPDIR:-/tmp}/q4kcache.XXXXXX")
    peek() { sed -n "s/.*$1+0 @ .* = \([0-9]*\) .*/\1/p" "$log"; }
    # shellcheck disable=SC2046
    set -- $(sed 's/ow-cpu\.lbl/ow-reu.lbl/' tests/golden/openworld-out.cmd)
    cold=$(tools/screendump.py "$@" \
               --vice=-reu --vice=-reusize --vice=512 --peek cache_hits:2 \
               2>"$log" | diff -q tests/golden/openworld-out.txt - >/dev/null \
           && echo same)
    hits=$(peek cache_hits)
    # shellcheck disable=SC2046
    set -- $(cat tests/golden/openworld-out.cmd)
    full=$(tools/screendump.py "$@" --reucache --cpu-engine \
               --peek cache_hits:2 --peek cache_misses:2 \
               2>"$log" | diff -q tests/golden/openworld-out.txt - >/dev/null \
           && echo same)
    fhits=$(peek cache_hits)
    fmiss=$(peek cache_misses)
    rm -f "$log"
    if [ "$cold" = same ] && [ "${hits:-0}" -gt 0 ] && [ "$full" = same ] \
            && [ "${fhits:-0}" -gt 0 ] && [ "$fmiss" = 0 ]; then
        result PASS openworld-cache
    else
        result FAIL openworld-cache "empty: ${cold:-another screen}, ${hits:-?} hits; full: ${full:-another screen}, ${fhits:-?} hits, ${fmiss:-?} misses"
    fi
}

# The networked build (make d64-online), experimental: the dialogue cases
# need a daemon on the other end of the modem line. With the modem the
# machine runs at its real speed (no warp: screendump.py), and screendump.py
# serves the disk without emulating the drive, or every conversation would
# wait a minute for its engine. So these are long, and nearly all waiting:
# they are the ones running side by side helps most.
u_online_dialog() {
    daemon_start 25301
    compare online-dialog tests/golden-online-dialog.txt \
        tools/screendump.py "$ON" --disk "$ONDISK" --rs232 25301 \
        --keys 'Alaric\r1nwwwwwwwwwwwwaaaawaaawhat of the blade\rcould I have a dagger\r' --settle 40
    daemon_stop
}

# Brann -> Vane: sail to Wardlow (the south continent, for seed $5A)
# holding the Emberstone, with the run's secrets pinned (that seed makes
# the fishers' word "dead low").
# Asking for the key is refused; saying the word earns it, announced in
# the transcript. Saying goodbye then has Vane end the conversation
# (mock mode's BYE), which waits for a key. Stepping onto a town square
# never rolls for a monster, so this is not a walk in the open.
u_online_tidal() {
    daemon_start 25302
    compare online-tidal tests/golden-online-tidal.txt \
        tools/screendump.py "$ON" --disk "$ONDISK" --rs232 25302 \
        --poke quest_stage=3 --poke items=8 --poke lore_seed=$SEED \
        --poke no_encounters=1 \
        --keys 'Alaric\r1ns<'"$(keys millford port2)$(keys dock story --land 2)"'>wwwwwwwaaaawwwmay I have the key\rdead low\rgoodbye\r' \
        --settle 100
    daemon_stop
}

# The king, from a new game that walks to the castle: neither his
# greeting nor a "hello" hands over the Sigil; asking about the quest
# has him say so, and it arrives in the transcript.
u_online_sigil() {
    daemon_start 25303
    compare online-sigil tests/golden-online-sigil.txt \
        tools/screendump.py "$ON" --disk "$ONDISK" --rs232 25303 \
        --poke quest_stage=1 --poke lore_seed=$SEED --poke no_encounters=1 \
        --keys 'Alaric\r1ns<'"$(keys millford castle)"'>wwwwwwwwwwwhello\rwhat is my quest\r' \
        --settle 100
    daemon_stop
}

# The king after Maldrath is banished (stage 9): the first word pays the
# 1,000 gold and moves the quest to Q_REWARDED, the next only praises.
u_online_reward() {
    daemon_start 25304
    compare online-reward tests/golden-online-reward.txt \
        tools/screendump.py "$ON" --disk "$ONDISK" --rs232 25304 \
        --poke quest_stage=9 --poke lore_seed=$SEED --poke no_encounters=1 \
        --keys 'Alaric\r1ns<'"$(keys millford castle)"'>wwwwwwwwwwwhello\rthank you\r' \
        --settle 100
    daemon_stop
}

# The inn in Millford, wounded: an ale brings a hint from the daemon's
# deck (mock mode shuffles it the same way every run), and a bed puts
# the hero back to full health for 20 marks.
u_online_inn() {
    daemon_start 25305
    compare online-inn tests/golden-online-inn.txt \
        tools/screendump.py "$ON" --disk "$ONDISK" --rs232 25305 \
        --poke hp_start=5 --poke gold=60 --poke lore_seed=$SEED --keys 'Alaric\r1nwwddddd12' --settle 24
    daemon_stop
}

# A fight, there and back, in either build: the dead rise at the first L on
# the road out of Millford (grave_odds=1), the hero has one point of health
# (hp_start=1) and only guards (SPACE), so the first blow kills, and a key later they wake
# in Millford. That loads the fight's engine over the map's, and the map's
# back over it, with the map's own state (the scene, the log) intact.
# fight <name> <prg> <disk>
fight() {
    out=$(tools/screendump.py "$2" --disk "$3" \
        --poke quest_stage=1 --poke lore_seed=$SEED --poke grave_odds=1 \
        --poke hp_start=1 \
        --keys 'Alaric\r1ns<l                                                            ' \
        --settle 40 --until 'The party has fallen\.' \
        --until 'You wake in Millford' 2>/dev/null) || out=
    if printf '%s\n' "$out" | grep -q 'The party has fallen\.' \
            && printf '%s\n' "$out" | grep -q 'You wake in Millford' \
            && ! printf '%s\n' "$out" | grep -q 'Put the game disk back'; then
        result PASS "$1"
    else
        result FAIL "$1"
        printf '%s\n' "$out"
    fi
}

# Garrett at stage 0 has one greeting. Asking for the dagger hands it
# over in the transcript and takes the topic off the renumbered menu, so
# 3 is then Maldrath.
u_talk() {
    compare talk tests/golden-talk.txt \
        tools/screendump.py dist/q4k.prg --disk "$DISK" \
        --keys 'Alaric\r1nwwwwwwwwwwwwaaaawaaa13' --settle 30
}

# With no disk there is no map's engine to load after the title screen
# (engine.h), so the game asks for the disk back, and keeps asking.
u_nodisk() {
    out=$(tools/screendump.py dist/q4k.prg --keys 'Alaric\r1nw' --settle 8 \
        --until 'Put the game disk back' 2>/dev/null) || out=
    if printf '%s\n' "$out" | grep -q 'Put the game disk back'; then
        result PASS nodisk
    else
        result FAIL nodisk
        printf '%s\n' "$out"
    fi
}

# The king at stage 9: his greeting is the line that gives, so the
# reward comes before anything is asked, and the menu is stage 10's.
u_reward() {
    compare reward tests/golden-reward.txt \
        tools/screendump.py dist/q4k.prg --disk "$DISK" \
        --poke quest_stage=9 --poke lore_seed=$SEED --poke no_encounters=1 \
        --keys 'Alaric\r1ns<'"$(keys millford castle)"'>wwwwwwwwwww1' \
        --settle 90
}

# Two ales in Millford's inn. Which rumours is random, so this checks
# only that two were told and the keeper was never "busy" (no file).
u_inn() {
    out=$(tools/screendump.py dist/q4k.prg --disk "$DISK" --poke gold=60 \
        --keys 'Alaric\r1nwwddddd11' --settle 30 --until 'You have 50 marks' \
        --until 'You drink\.(.|\n)*You drink\.' 2>/dev/null) || out=
    drinks=$(printf '%s\n' "$out" | grep -c 'You drink\.') || true
    if [ "$drinks" = 2 ] && ! printf '%s\n' "$out" | grep -q 'keeper is busy' \
            && printf '%s\n' "$out" | grep -q 'You have 50 marks'; then
        result PASS inn
    else
        result FAIL inn
        printf '%s\n' "$out"
    fi
}

# run_unit <name>: one unit, whatever it needs missing is a SKIP.
run_unit() {
    case $1 in
        checkmap) u_checkmap ;;
        checkisle) u_checkisle ;;
        npcd) u_npcd ;;
        owarena-arrow-ur) u_arrow ;;
        save|resave|fight|talk|nodisk|reward|inn)
            [ -f dist/q4k.prg ] && [ -f "$DISK" ] || [ "$1" = nodisk ] \
                || { result SKIP "$1" "no $DISK"; return; }
            case $1 in
                save) u_save ;;
                resave) u_resave ;;
                fight) fight fight dist/q4k.prg "$DISK" ;;
                talk) u_talk ;;
                nodisk) u_nodisk ;;
                reward) u_reward ;;
                inn) u_inn ;;
            esac ;;
        openworld-save|openworld-quaff|openworld-recolour|openworld-fight|\
        openworld-foes|openworld-keeper|openworld-cache|openworld-roads)
            [ -f dist/openworld.d81 ] || { result SKIP "$1" "no dist/openworld.d81"; return; }
            case $1 in
                openworld-save) u_ow_save ;;
                openworld-quaff) u_ow_quaff ;;
                openworld-recolour) u_ow_recolour ;;
                openworld-fight) u_ow_fight ;;
                openworld-foes) u_ow_foes ;;
                openworld-keeper) u_ow_keeper ;;
                openworld-cache) u_ow_cache ;;
                openworld-roads) u_ow_roads ;;
            esac ;;
        online-dialog|online-tidal|online-sigil|online-reward|online-inn|online-fight)
            [ -f "$ON" ] && [ -f "$ONDISK" ] || { result SKIP "$1" "no $ONDISK"; return; }
            case $1 in
                online-dialog) u_online_dialog ;;
                online-tidal) u_online_tidal ;;
                online-sigil) u_online_sigil ;;
                online-reward) u_online_reward ;;
                online-inn) u_online_inn ;;
                online-fight) fight online-fight "$ON" "$ONDISK" ;;
            esac ;;
        online-title) u_online_title ;;
        online-*) u_online_golden "${1#online-}" ;;
        *)
            if [ -f "tests/golden/$1.txt" ]; then
                u_golden "$1"
            else
                result FAIL "$1" "no such test"
            fi ;;
    esac
}

# Every unit, the slowest first (from CI's timings), so that what starts
# last is short and the run does not wait on one long case at the end.
# The modem's cases lead: they run in real time and take minutes, but are
# nearly all waiting, so they cost the others little CPU.
all_units() {
    echo online-tidal online-sigil online-reward online-dialog online-inn
    echo reward openworld-save checkisle fight online-fight talk inn \
        openworld-foes openworld-fight resave save openworld-recolour \
        online-treasuremap online-fov
    for g in tests/golden/*.txt; do basename "$g" .txt; done
    echo openworld-quaff openworld-keeper openworld-cache openworld-roads \
        owarena-arrow-ur \
        nodisk npcd checkmap online-title
    for n in $ONLINE_GOLDENS; do
        case $n in treasuremap|fov) ;; *) echo "online-$n" ;; esac
    done
}

# Which third of the suite a unit is in, for CI's three machines.
group_of() {
    case $1 in
        online-*) echo online ;;
        openworld*|owarena*) echo openworld ;;
        *) echo game ;;
    esac
}

# --- the run ------------------------------------------------------------------

if [ -n "$one" ]; then
    # One unit, for xargs: its lines in one go when it ends, so that two
    # ending together do not interleave. Always 0: xargs stops on 255.
    unit_log=$TEST_OUT/$one
    run_unit "$one" > "$unit_log" 2>&1 \
        || result FAIL "$one" "the unit itself failed" >> "$unit_log"
    cat "$unit_log"
    exit 0
fi

if [ -n "$names" ]; then
    units=$names
else
    units=
    for u in $(all_units); do
        if [ -z "$group" ] || [ "$(group_of "$u")" = "$group" ]; then
            units="$units $u"
        fi
    done
fi
if [ "$list" = 1 ]; then
    printf '%s\n' $units
    exit 0
fi

TEST_OUT=$(mktemp -d "${TMPDIR:-/tmp}/q4ktest.XXXXXX")
export TEST_OUT
trap 'rm -rf "$TEST_OUT"' EXIT
flags=
[ "$bless" = 1 ] && flags=--bless
echo "test.sh: $(printf '%s\n' $units | wc -l | tr -d ' ') units, $jobs at a time"
# shellcheck disable=SC2086
printf '%s\n' $units | xargs -P "$jobs" -n 1 sh "$self" $flags --one || true

total=$(($(date +%s) - t0))
cat "$TEST_OUT"/* > "$TEST_OUT.all" 2>/dev/null || true
echo
echo "Slowest:"
awk '/^(PASS|FAIL|SKIP|BLESS) / { t = $3; gsub(/[^0-9]/, "", t); print t, $2 }' \
    "$TEST_OUT.all" | sort -rn | head -8 | awk '{ printf "  %4ds  %s\n", $1, $2 }'
failed=$(grep '^FAIL' "$TEST_OUT.all" | awk '{ print $2 }' | tr '\n' ' ' || true)
passed=$(grep -c '^PASS' "$TEST_OUT.all" || true)
rm -f "$TEST_OUT.all"
echo "test.sh: $passed passed, ${failed:+failed: }${failed:-none failed}, ${total}s"
[ -z "$failed" ]
