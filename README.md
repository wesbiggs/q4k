# Quest for the King

A grid-based RPG roguelike for the Commodore 64, in the spirit of Ultima III:
an open world that scrolls smoothly, a pixel or two a frame, over multicolour
tiles. Written in C (cc65) with assembly where it pays, run and tested under
VICE.

Letters go out from King Matheson to strangers all over the land: meet in
Millford, then come to the castle. The dragon Maldrath has been burning
Keldmarch for three winters and two armies have not come back. The blade that
can banish him was broken, and its three binding stones are sealed in three
deep places. Take arms from the smith, find the king, and start asking people
questions.

The plot is deliberately the oldest one there is. Everyone you meet has
something to say about it, picked from a menu of what you can ask.

> **A work in progress.** This is not a 1.0 release: the whole story can be
> played, but parts are unfinished (see [Not yet](#not-yet)), saves may not
> carry over between versions, and things will change. Issues and pull
> requests are welcome -- see [Contributing](#contributing).

## Getting it

Build it (see [DEVELOPERS.md](DEVELOPERS.md)): `make` writes
`dist/openworld.d81`, a 1581 disk image, and `make run` starts it in
[VICE](https://vice-emu.sourceforge.io), the Commodore emulator (`brew
install vice` on a Mac). On a real machine, mount the image on a drive that
takes a D81 (a TeensyROM, an SD card drive) and `LOAD"OPENWORLD",8` and
`RUN`.

**Keep the disk in the drive**: the game loads each part of the world, the
fights, the shops and the conversations from it as they are wanted, and saves
to it. It is made for a fast drive. An REU, if there is one, makes it faster
still: everything loaded is kept there after the first time, and the screen
scrolls through it.

The releases up to 0.11 are the earlier game, with a screen that moved a
square at a time; they are still on the
[releases page](https://github.com/wesbiggs/q4k/releases).

## Playing

The title picture waits for `F1`, a new game, or `F3`, the saved one. A new
game tells the story (any key goes on) and then shows who answered the
king's letter: a warrior, an archer and a wizard, named for you, with their
health and a wizard's magic. `1`-`3` opens one's page, with their figure,
where `C` changes their class, `N` their name (RETURN alone picks one), `P`
their pronouns (he/him, she/her, they/them or it), `S` their skin's colour
and `T` their tunic's (SHIFT steps the colours back), and DEL sends them
home (if they are not the last); cursor left and right go to the others,
`1`-`3` puts the one shown in that place, and RETURN goes back to the
list. `A` adds another while there are fewer than three. The skin, the
name and the class are settled once the adventure begins. RETURN begins the
adventure: the new game is saved, so `F3` at the title starts it again,
and you start at Millford's gate.

| key | |
|---|---|
| cursor keys, `WASD`, a joystick in port 2 | walk |
| `Q` `E` `Z` `X`, or the stick's diagonals | walk diagonally |
| `+` / `-`, or the stick's button | go into the town or dungeon you stand on; down / up a dungeon's stairs; anywhere else `+` searches where you stand, and takes anything hidden there (a chest, gold, a treasure map's hoard) |
| `L` | look about the map: the direction keys move the view (over what you have seen; the rest stays cloud or dark) while the party stays put and no time passes; `SPACE` brings the view back to the party, `RUN/STOP` (or `L`) ends the look there |
| `I` | the party, one at a time, and what each carries; cursor left and right for the next, `1`-`3` to put the one shown in that place (the first leads), `T` their tunic's colour (SHIFT+`T` back), `P` their pronouns, `S` to prepare a wizard's spells, `M` the treasure maps you carry; RUN/STOP goes back to the game |
| `P` | the worst hurt of the party drinks a healing draught |
| `SPACE` | hide the two lines of text under the map, for a bigger map |
| `F1` / `F3` | save / restore (each asks `Y` first) |
| `?` | keys, and what you are supposed to be doing next |
| `M` / `SHIFT+M` | the music / the sound effects off and on |
| `1` / `2` | walk one or two pixels a frame |
| `RUN/STOP` | quit to the title (asks `Y` first) |

What is said is said in the two lines under the map, where the party's health
(and a wizard's magic), draughts and arrows are otherwise. Walk into someone
to talk to them, open their shop, or take a seat at their inn. Stand on a
chest to open it. Innkeepers sell a mug of ale, and with it a rumour; beds put
the whole party back to full health, and the wizards' magic with it.

### The party

Each of the party has their own health and arms; the gold, the draughts, the
arrows, the experience and the level are shared. Garrett the smith arms each
by class: a dagger for a warrior, a hunting bow and a quiver for an archer, a
staff for a wizard. Millford's gate stays shut until he has.

- **Warriors** hit hardest and last longest, and only a warrior can wield
  anything better than a short sword.
- **Archers** are the only ones who can draw anything better than a
  shortbow, and their arrows hit harder than anyone else's, more as the party
  rises.
- **Wizards** have the least health and wear nothing heavier than robes,
  but their own wards turn some of every blow, more as the party rises, and
  they cast spells for magic. Magic comes back slowly as you travel, a point
  a round in a fight, fully in an inn's beds, and whenever the party rises a
  level.

Whatever is bought goes to whoever it would be better for and may use it;
when that is more than one of you, you are asked who takes it.

Every wizard knows **Spark**, a bolt of fire. The rest are learned from books,
each once the party has reached its level. There are two magic shops:
the one in Keldmarch's own town (the one that is not Millford) sells Heal,
Ward and Sleep, and Ashcombe's, across the sea, Ward, Sleep, Bolt and
Blaze: **Heal** mends one of
the party, **Ward** has everyone take less for the rest of the fight,
**Sleep** keeps the foes still for two rounds, **Bolt** is lightning that
flies far and hits hard, and **Blaze** burns every foe at once. A wizard
prepares two spells to begin with and one more every third level, up to five.

### Fighting

Things wander out at you as you cross open ground (less often on the roads);
in a dungeon they are on the map, and come for you once they see you. A fight
is on a field of the ground you were on. Each of the party takes a turn in
order, and then the monsters move. You walk into a foe to strike.

| key | |
|---|---|
| move, and `Q` `E` `Z` `X` | step, or strike if it is in the way |
| the up-arrow key, or the stick's button | aim this one's bow, if they have one (one arrow a shot); a wizard's casts. A yellow cursor marks the nearest foe in reach with a clear line; the direction keys (or the stick) go through the others, `SPACE` or the button again shoots, and any other key lowers the bow without spending the turn |
| `*` | a wizard casts a prepared spell (Spark and Bolt are aimed the same way) |
| `SPACE` | stand your ground: take less from the next blow |
| `P` | the one whose turn it is drinks a healing draught |
| walk off the side | the gap in the middle of the left or right edge: each of you who reaches it is out, and the fight is fled when the last one is -- but not from a keeper or Maldrath |

Sand and swamp are slow going. A shot flies straight at the foe it is aimed
at, at any angle, but nothing can be aimed at past a boulder, a tree, a wall,
another foe or one of the party. One of the party who falls is out of the
fight, and gets up again after it with a little health, if it is won or fled;
it is lost only when all of you fall, and you wake in Millford with half the
purse. A monster that comes alone -- a keeper, Maldrath, a lone wanderer --
is a match for a party: it has more health and hits harder for each of you
past the first, and moves between your turns.

A foe beaten may leave something behind: what goes armed now and then drops
a blade, mail or a bow, which goes to whoever in the party it is better for;
and out on the land, now and then a treasure map, which says where on that
land something is buried. Stand on the spot and press `+` to dig it up.

## The world

Nothing is in the same place twice: the lands are made from the game's seed,
so a new game has the castle, the Hollow Crown and Maldrath's lair somewhere
new, and in the same place every time you come back to them in the same game.
Keldmarch is in the middle of a sea that wraps round both ways, with a
continent across the water to each side of it. Roads join its towns, the
castle and its four ports.

Step onto a town, the castle or a dungeon and its name is shown; press `+`
(or the stick's button) to go in. In a town a building you are not in shows
only its roof, which comes off as you step in at the door; walk out at any
edge. In a dungeon `+` goes down the stairs and `-` up them (the button
whichever they are); up from the first level is out, and you are back on the
dungeon's square. Underground
you see the room you stand in, the squares round you and straight along the
corridors; the rest is dark until you have been near it, and stays on the map
once you have, for every level, until you leave the dungeon. A monster you
cannot see is not shown.

You see what is in your line of sight: mountains and forest hide what
lies beyond them (in a forest, only the trees about you), and what you have
not yet seen is dark. What you have seen stays on the map -- until you load
a saved game, which starts the map unseen again.

Out on the open sea between the lands lie small islands, a few in every
stretch of it, and on one of each group a town or a sea cave: sail there
and step ashore.

Every land is ringed by shallows, lighter water a few squares wide, where
nothing troubles a ship; out on the dark deep sea things come up out of the
water at you, and are fought on the ship's deck -- they cannot leave the
water (most spit at you from it), and you cannot leave the deck; only the
killer eagles come down onto it.

The king's ship lies at the port nearest the castle. Bring him the
Emberstone out of the Hollow Crown and he lends it to you: step onto it to
board it, sail the open water, and step off onto any land, where it waits for
you. The quest (`?`) says which way across the sea the next land lies. Wardlow, Ashcombe, the Drowned Stair and
Blackpit are one to a continent, and which is which is different in every
game. Each continent is mostly one kind of ground -- the plains, a forest, a
swamp, the desert, or mountains -- and has a town of its own as well.

## Not yet

A flight pays nothing for the foes killed before it.

## Contributing

Bug reports, ideas and pull requests are all welcome: open an issue on
[GitHub](https://github.com/wesbiggs/q4k/issues). For a bug, say what you
did, what you expected and what happened, with the emulator or hardware
(VICE's version, PAL or NTSC, an REU or not) and, if you can, a save
(`ow-save` on the disk image).

For code, [DEVELOPERS.md](DEVELOPERS.md) covers building and testing, and
[CLAUDE.md](CLAUDE.md) the working notes -- memory is tight and the machine
has traps, so read the parts it points to before changing something. The
tests run on every push. By sending a pull request you agree that your
contribution is licensed as below.

## Licence

Copyright (C) 2026 Wes Biggs.

- **The code** -- everything in `src/`, `include/`, `cfg/` and `tools/`, the
  `Makefile`, the tests and the workflows -- is free software under the
  [GNU General Public License, version 3](LICENSE) or (at your option) any
  later version.
- **The game's content** -- everything in `assets/` (the tiles, maps,
  monsters, fields, portraits, the score, what people say, the story) but
  the two directories below -- is under [Creative Commons
  Attribution-ShareAlike 4.0](assets/LICENSE) (CC BY-SA 4.0).
- `assets/splash-kit/` (the splash screen, as vic2orious made it) and
  `assets/vic2/` (its picture libraries) are under the Apache License 2.0,
  in their own `LICENSE` files.

Some checked-in sources are generated from the assets by the tools (the
ones that say so at the top), and the built game combines both. CC BY-SA
4.0 material may be adapted into a GPLv3 work, so the disk images and
programs `make` builds can be shared under the GPL, version 3 or later.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
more details.
