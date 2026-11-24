#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
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
# slowest and of any failures comes last. --group runs one half (goldens or
# cases: group_of() below), which is how CI splits the suite over two
# machines; NAMEs run just those units; --list prints them.
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

# Cheap and worth doing first: a hand-edited town can wall a person or a
# chest in, and no screen test would ever notice.
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

# The arrow is one sprite block, filled from fight_arrow[] for the nearest
# of the eight ways to the foe it is aimed at (src/fight/fight.c's
# arrow_to(), loose()): the dragon case's Alaric aims at Maldrath, up and
# to the left of him (the only foe, so SPACE looses at once), and the block
# holds that picture after. --peek prints the last byte first, so this is
# the block's 48 bytes backwards.
u_arrow() {
    [ -f dist/owarena.prg ] && [ -f dist/owarena.d64 ] \
        || { result SKIP owarena-arrow-ul "no dist/owarena.d64"; return; }
    got=$(tools/screendump.py dist/owarena.prg --disk dist/owarena.d64 \
        --mux build/fight.lbl --poke auto_seed=7 --stage 3 --timeout 180 \
        --keys '\x85\x85\x85\x85\x85\x85\x85\x85\x85owww\x5e\x20' \
        --peek '$5580:48' 2>&1 >/dev/null \
        | sed -n 's/.*(\$\([0-9A-F]*\)).*/\1/p')
    want=0000000000000001000018000009000020000020000080000080000000020000820000880000A00000AA000000000000
    if [ "$got" = "$want" ]; then
        result PASS owarena-arrow-ul
    else
        result FAIL owarena-arrow-ul "the arrow's block: ${got:-?}"
    fi
}

# The open world saves its game block into OW-SAVE (F1), and
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

# The princess's portrait (src/ow/talkpic.c): from the castle's gate to
# the tower, a step into her, padding while owtalk loads (it empties the
# buffer as it starts), and 2 asks about her father. The screen is a
# bitmap, which a text dump cannot read, so this reads the machine: the
# question's row is white and the answer's light green, beside the PAL
# portrait (cells 17 on: $4000 + 40 * row + 17), and the VIC is in bitmap
# mode. Then RUN/STOP goes back to the engine, where she stood.
PRINCESS='ddddddddddddssssdsssdsdddddddddddddddddddsssssddddddsd+wwwwwwwwssdddddwwdwwwwawwdddw'
u_ow_portrait() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke still=1 \
        --keys "${PRINCESS}jjjjjjjjjjj2" --settle 30 \
        --until-peek '$4061=16' --until-peek '$4089=208' \
        --peek '$4061' --peek '$4089' --peek '$D011' \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" != "16 208 59 " ]; then
        result FAIL openworld-portrait "question, answer, \$D011: ${got:-?}; not 16 208 59"
        return
    fi
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke still=1 \
        --keys "${PRINCESS}jjjjjjjjjjj2jjjjjjjjjj\\x03jjjjjjjjjj" --stage 2 --settle 30 \
        --until-peek '$D011=27' --peek hx --peek hy \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "53 38 " ]; then
        result PASS openworld-portrait
    else
        result FAIL openworld-portrait "back in the engine at ${got:-?}; not 53 38"
    fi
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

# I, then T twice: Alaric's clothes (14, light blue, from his name) go to
# 15 and round past black to 1, his skin staying light red (10, from his
# name too); P steps his pronouns from he/him on to she/her (the top two
# bits of his `known`, party+19, actor.h); and RUN/STOP goes back to the
# engine, whose hero is drawn in them (sm_tunic, sm_skin). The colour byte
# is skin << 4 | clothes. The j's are padding while owtown loads (it
# empties the buffer as it starts).
u_ow_recolour() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl \
        --keys 'ijjjjjjjjjttp\x03' --settle 15 \
        --peek party+139 --peek sm_tunic --peek sm_skin --peek party+19 \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "161 1 10 128 " ]; then
        result PASS openworld-recolour
    else
        result FAIL openworld-recolour "colours in the block, the hero's, Alaric's pronouns: ${got:-?}; not 161 1 10 128"
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

# Spoils: openworld-fight's goblin, with loot_always (OWF_LOOT) so that the
# win leaves all it can: its arms, a short sword, which Alaric takes up
# over his dagger, and a treasure map of Keldmarch (the game block's
# tmap[0], $554E: home, 14,21), on owtown's spoils screen (owtown loaded
# again: src/lib/ow_tmap.c). A key and the engine is back; I, then M,
# lists the map from the place nearest it. The j's are padding while
# owtown and the engine load.
u_ow_spoils() {
    pad=$(printf 'j%.0s' $(seq 60))
    out=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke enc_always=1 \
        --poke loot_always=128 \
        --keys "ddjjjjjjjjjjjjjjjjjjjj      $(printf 'w%.0s' $(seq 20))j${pad}i${pad}m" \
        --settle 40 --until '5 leagues west, 7 north of Millford' \
        --peek party+193:2 2>&1)
    map=$(echo "$out" | sed -n 's/.*party+193 @ .* = \([0-9]*\) .*/\1/p')
    if [ "$map" = 21838 ] && echo "$out" | grep -q 'Keldmarch' \
        && echo "$out" | grep -q '5 leagues west, 7 north of Millford'; then
        result PASS openworld-spoils
    else
        result FAIL openworld-spoils "map ${map:-?}, not 21838, or the list is not 5 west, 7 north of Millford"
    fi
}

# L on a treasure map's square: one poked onto the quick start's (home,
# 19,28: $5713) is dug up, 16 a quest stage past the second (Q_HEART: 144)
# and 15 from where it lay (19 ^ 28), and two draughts, and the map is
# gone: 9 + 159 gold, 3 draughts.
u_ow_dig() {
    got=$(tools/screendump.py dist/openworld.prg --disk dist/openworld.d81 --fastdisk \
        --splash quick --lbl build/ow-cpu.lbl --poke party+193=19 \
        --poke party+194=87 --poke grave_odds=0 --keys "jjl" \
        --settle 20 --until 'You dig up 159 gold and 2 draughts!' \
        --until-peek party+193:2=0 \
        --peek party+193:2 --peek party+90:2 --peek party+94 \
        2>&1 >/dev/null | sed -n 's/.*@ .* = \([0-9]*\) .*/\1/p' | tr '\n' ' ')
    if [ "$got" = "0 168 3 " ]; then
        result PASS openworld-dig
    else
        result FAIL openworld-dig "map, gold, draughts: ${got:-?}; not 0 168 3"
    fi
}

# Into the Hollow Crown from the quick start, for the dungeon's units.
ow_enter=dddssssssssssssssssdsssssaasaaaaaa+a

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
        --settle 30 --until-peek party+144=216 --until-peek sm_npc_x=0 \
        --until-peek party+88:2=2 --until-peek '$B318=146' \
        --peek party+144 --peek sm_npc_x --peek party+88:2 --peek '$B318' \
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
    ow_down=aaaaaaaaaaaaaaaaaaaazssssssssss+aaaaaaaaaaaaaaaaaaaaaaaazssssssss+
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

# run_unit <name>: one unit, whatever it needs missing is a SKIP.
run_unit() {
    case $1 in
        checkmap) u_checkmap ;;
        checkisle) u_checkisle ;;
        owarena-arrow-ul) u_arrow ;;
        openworld-save|openworld-quaff|openworld-recolour|openworld-fight|\
        openworld-spoils|openworld-dig|openworld-foes|openworld-keeper|openworld-cache|openworld-roads|\
        openworld-portrait)
            [ -f dist/openworld.d81 ] || { result SKIP "$1" "no dist/openworld.d81"; return; }
            case $1 in
                openworld-save) u_ow_save ;;
                openworld-quaff) u_ow_quaff ;;
                openworld-recolour) u_ow_recolour ;;
                openworld-fight) u_ow_fight ;;
                openworld-spoils) u_ow_spoils ;;
                openworld-dig) u_ow_dig ;;
                openworld-foes) u_ow_foes ;;
                openworld-keeper) u_ow_keeper ;;
                openworld-cache) u_ow_cache ;;
                openworld-roads) u_ow_roads ;;
                openworld-portrait) u_ow_portrait ;;
            esac ;;
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
all_units() {
    echo openworld-save checkisle openworld-foes openworld-fight \
        openworld-spoils openworld-recolour
    for g in tests/golden/*.txt; do basename "$g" .txt; done
    echo openworld-portrait openworld-quaff openworld-dig openworld-keeper openworld-cache \
        openworld-roads owarena-arrow-ul checkmap
}

# Which half of the suite a unit is in, for CI's two machines: the open
# world's goldens, and everything else (its cases of their own, the arena,
# the generators' checks).
group_of() {
    if [ -f "tests/golden/$1.txt" ] && [ "${1#openworld}" != "$1" ]; then
        echo goldens
    else
        echo cases
    fi
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
