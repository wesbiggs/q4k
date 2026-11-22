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
health and a wizard's magic. `1`-`3` opens one's page, with their figure, to
send them home (if they are not the last), rename them (RETURN alone picks
a name), change their class, the colours of their clothes and skin, or
their pronouns (he/him, she/her, they/them or it); `A` adds another while
there are fewer than three. RETURN begins the
adventure: the new game is saved, so `F3` at the title starts it again,
and you start at Millford's gate.

| key | |
|---|---|
| cursor keys, `WASD`, a joystick in port 2 | walk |
| `Q` `E` `Z` `X`, or the stick's diagonals | walk diagonally |
| `L` | look where you stand, and take anything hidden there |
| `I` | the party, one at a time, and what each carries; cursor left and right for the next, up and down for their clothes' colour, `K` their skin's, `P` their pronouns, `S` to prepare a wizard's spells |
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

Every wizard knows **Spark**, a bolt of fire. The rest are learned from books
sold at the magic shop in Keldmarch's own town (the one that is not
Millford), each once the party has reached its level: **Heal** mends one of
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
| the up-arrow key, or the stick's button | shoot this one's bow, if they have one (one arrow a shot); a wizard's casts |
| `*` | a wizard casts a prepared spell |
| `SPACE` | stand your ground: take less from the next blow |
| `P` | give a draught to any of the party |
| walk off the side | the gap in the middle of the left or right edge: each of you who reaches it is out, and the fight is fled when the last one is -- but not from a keeper or Maldrath |

Sand and swamp are slow going, and a shot does not fly over the party: whoever
of you is in its way takes it. One of the party who falls is out of the
fight, and gets up again after it with a little health, if it is won or fled;
it is lost only when all of you fall, and you wake in Millford with half the
purse. A monster that comes alone -- a keeper, Maldrath, a lone wanderer --
is a match for a party: it has more health and hits harder for each of you
past the first, and moves between your turns.

## The world

Nothing is in the same place twice: the lands are made from the game's seed,
so a new game has the castle, the Hollow Crown and Maldrath's lair somewhere
new, and in the same place every time you come back to them in the same game.
Keldmarch is in the middle of a sea that wraps round both ways, with a
continent across the water to each side of it. Roads join its towns, the
castle and its four ports.

Step onto a town, the castle or a dungeon to go in. In a town a building you
are not in shows only its roof, which comes off as you step in at the door. A
dungeon's stairs go down and up; up from the first level is out.

Ships lie at Keldmarch's ports and the continents' docks. Step onto one to
board it, sail the open water, and step off onto any land, where it waits for
you. The ships at Keldmarch's ports answer to the king: the first sails when
you have brought the Emberstone out of the Hollow Crown, and one more with
each token you win after it. Wardlow, Ashcombe, the Drowned Stair and
Blackpit are one to a continent, and which is which is different in every
game. Each continent is mostly one kind of ground -- the plains, a forest, a
swamp, the desert, or mountains -- and has a town of its own as well.

## Not yet

Beaten monsters drop no gear or treasure maps yet, and a flight pays nothing
for the foes killed before it.
