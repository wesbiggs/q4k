# Quest for the King

A grid-based RPG roguelike for the Commodore 64, in the spirit of Ultima III.
Written in C (cc65) with assembly where it pays, run and tested under VICE.

Letters go out from King Matheson to strangers all over the land: meet in
Millford, then come to the castle. The dragon Maldrath has been burning
Keldmarch for three winters and two armies have not come back. The blade that
can banish him was broken, and its three binding stones are sealed in three
deep places. Take arms from the smith, find the king, and start asking people
questions.

The plot is deliberately the oldest one there is. Everyone you meet has
something to say about it, picked from a menu of what you can ask. An
experimental build goes further: every NPC answers in their own voice, from
the world's canon and from how far you have got, through a model running on
the host at the other end of the C64's modem port.

## Getting it

Download the files from the
[latest release](https://github.com/wesbiggs/q4k/releases/latest):

| file | |
|---|---|
| `q4k-<version>.d64` | the game on a disk image, for a real C64 or any emulator -- start here |
| `q4k-<version>-online.zip` | experimental: the game with people who answer through a language model, and everything that needs |
| `q4k-<version>-online.d64` | experimental: that build's disk alone |

All of them run in [VICE](https://vice-emu.sourceforge.io), the Commodore
emulator (`brew install vice` on a Mac).

**The .d64** is the game on its disk: open it in VICE, or drop it on the
window; on a real machine with an SD card drive, `LOAD"Q4K",8` and `RUN`.
What everyone says is on the disk, and you pick what to ask from a menu.

**The online zip** (experimental; macOS or Linux, with Python 3). Unzip it
and start the game from inside the folder; this brings up the program that
lets people talk, connects the emulator's modem port to it, and gives you a
disk to save on (see [Talking to people](#talking-to-people)):

```bash
cd q4k-0.11.0-online
tools/play.sh
```

Either way, **keep the disk in the drive**. The game loads the map, the
fights, conversations and the shops from it as they are wanted, and saves to
it. If it cannot find the disk it asks for it back. It is meant for a fast
drive (an SD card drive, or VICE); a stock 1541 would take most of a minute
for every fight.

To build it yourself instead, see [DEVELOPERS.md](DEVELOPERS.md).

## Playing

The title screen sets the scene and makes your party: up to three who
answered the king's letter, each a name (RETURN alone picks a random one) and
a class -- `1` warrior, `2` archer, `3` wizard -- and then `Y` for another or
`N` to set out. `F3` loads a saved game instead, and `F5` lets you choose the
world's seed (1 to 255; the same seed makes the same lands and towns). The `?`
screen shows the seed in play. Then `WASD` or the cursor
keys move. A joystick in port 2 does too, and its
button is `F` in a fight (push it again, or press a key, to answer "Shoot
which way?") and elsewhere whichever of `>` and `<` makes sense where you
stand. On the map the party travels as one figure, its leader's. The panel
shows each member's health (and a wizard's magic), the party's level,
experience, gold, draughts and arrows, and any quest items; `I` shows what
each of them carries. Walk into someone to talk to them, open their shop, or
take a seat at their inn.

| key | |
|---|---|
| `>` / `<` | go in at a town or dungeon, or down a stair / out of a town, or up a stair |
| `L` | look where you stand, and take anything hidden there |
| `I` | the party and what each carries; a wizard's number to prepare spells; `T` for your treasure maps |
| `P` | the worst hurt of the party drinks a healing draught |
| `F1` / `F3` | save / restore (each asks `Y` first) |
| `SPACE` | guard, in a fight |
| `?` | keys, and what you are supposed to be doing next |
| `M` / `X` | music / sound effects on or off (costs no turn) |
| `Q` | quit (asks `Y` first) |

Treasure is hidden in the castle and in dungeon chests, and
a square need not look like anything: stand on it and press `L`. Innkeepers
sell a mug of ale for five marks, and with it -- when the dialogue host is
on the line -- a hint at where to look or what to do next; beds for twenty
put the whole party back to full health, and the wizards' magic with it.

Now and then a beaten monster drops a treasure map. It gives the land and how
many leagues east or west and north or south of a town the treasure is buried,
a league being one step. Walk there, press `L`, and dig. You can hold four.

### The party

Each of the party has their own health and arms; the gold, the draughts (one
each to start), the arrows, the experience and the level are shared. Garrett
the smith arms each by class: a dagger for a warrior, a hunting bow and a
quiver for an archer, a staff for a wizard.

- **Warriors** hit hardest and last longest, and only a warrior can wield
  anything better than a short sword.
- **Archers** are the only ones who can draw anything better than a
  shortbow, and their arrows hit harder than anyone else's, more as the party
  rises.
- **Wizards** have the least health and wear nothing heavier than robes
  (between travel clothes and leather), but their own wards turn some of
  every blow, more as the party rises, and they cast spells for magic (MP).
  Magic comes back slowly as you travel, a point a round in a fight, fully in
  an inn's beds, and whenever the party rises a level. Their spells grow
  stronger with the party's level too.

Whatever is bought, or dropped by a monster, goes to whoever it would be
better for and may use it; when that is more than one of you, you are asked
who takes it.

Every wizard knows **Spark**, a bolt of fire. The rest are learned from books
sold at the magic shop in Keldmarch's own town (the one that is not
Millford), each once the party has reached its level: **Heal** mends one of
the party, **Ward** has everyone take less for the rest of the fight,
**Sleep** keeps the foes still for two rounds, **Bolt** is lightning that
flies far and hits hard, and **Blaze** burns every foe at once. A wizard
prepares two spells to begin with and one more every third level, up to
five: on the `I` screen, press their number, then the spells' numbers to
change which.

A healing draught restores a share of your full health, so it is worth a
turn late in the game as well as early, and its price climbs to match: five
marks, plus five for each level you have.

Saved games go to the game's disk, in drive 8. The online zip's
`tools/play.sh` plays from a copy of it, `dist/online/saves.d64`, and keeps
your save when a new game disk replaces it.

### Fighting

A fight happens on a field of the ground you were crossing, a dungeon room,
or Maldrath's lair. The whole party stands on it, near the bottom; the
monster starts somewhere in the top third of it and comes for whichever of
you it can reach soonest. Each of you takes a turn in order -- the one whose
turn it is is the moving figure, marked `>` on the panel, and the rest stand
as their class's mark -- and then the monsters move. You walk into it to
strike.

| key | |
|---|---|
| move | step, or strike if it is in the way |
| `F` then a direction | shoot this one's bow, if they have one (one arrow a shot) |
| `C` | a wizard casts a prepared spell: its number, then a direction or whom |
| `SPACE` | stand your ground: take less from the next blow |
| `P` | give a draught to any of the party (their number) |
| walk off the side | the gap in the middle of the left or right edge is the only way to flee, and it always works for the whole party if one of you reaches it -- but not from a keeper or Maldrath |

One of the party who falls is out of the fight, and gets up again after it
with a little health, if it is won or fled; it is lost only when all of you
fall. A monster that comes alone -- a keeper, Maldrath, a lone wanderer --
is a match for a party: it has three quarters as much health again and hits
two points harder for each of you past the first, and it moves between your
turns, half a turn more for each of you (two turns a round against three).
Groups come a little larger instead.

Some things shoot back. Bandits and bone archers carry bows, the
Drowned King throws water and Maldrath breathes fire, each whenever you are
in a straight line and within reach with nothing solid between. Beside you,
they strike like everything else. Tougher things turn up as you rise in
level: rats and goblins from the start, drakes from level seven.

As you rise, the smaller things come in numbers: up to one of a kind for
each time your level covers the level it first appears at, and never more
than six. At level six that is as many as six rats or three giant wasps.
They go round one another to reach you, and an archer will not shoot
through its fellows. Keepers and Maldrath always come alone.

Each continent also has two monsters of its own, met
nowhere else, after the ground it is mostly made of -- gnolls and centaurs on
the plains, trolls and wisps in the forest, leeches and bog hags in the mire,
sand wyrms and efreets in the desert, rock trolls and storm giants in the
mountains -- one of each pair shooting arrows, fire, water or lightning, and
each tougher on every continent the story opens after the first. And anything
that goes armed (goblins, bandits, skeletons, ogres and their like) now and
then leaves a blade, mail or a bow of the grades that land's shops sell,
for whichever of you it would be better for.

## The world

Home, Keldmarch, is a 64x64 map, and nothing in the game is in the same place
twice: it is made from the game's secret seed when you step out of Millford, so
a new game has the castle, the Hollow Crown and Maldrath's lair somewhere new,
and in the same place every time you come back to it in the same game. The
edge of the map is sea. Towns are exactly one 21x19 screen, so they never
scroll: walk in and the whole place is in front of you. Dungeon levels are
48x32 and scroll with you, like the overworld, until you reach an edge. A
dungeon level you walk back into is the level you left.

Dungeons are dark. A room lights up whole when you stand in it, you can see
straight along a corridor until something stops the eye, and squares you
have already seen stay on the map drawn dim. The map of every level stays
with you until you go into a different dungeon, so going back in -- by the
stairs, or from Millford after dying down there -- is not starting over.
Every level has at least one chest, and some have three.

### Across the sea

Keldmarch has four ports, an anchor on the coast at each compass point:
north, east, south and west. Step onto one and a ship takes you out to sea,
and you watch it go -- the ship holds the middle of the screen while Keldmarch
slides away behind it, until you are alone on open sea, and then the far
continent slides in the same way until the ship reaches its port and you
come ashore. Step onto that port again and the ship takes you home.

The captains answer to the king and will not sail for a stranger at first: the
first country opens when you have brought the Emberstone out of the Hollow
Crown, and one more with each token you win after it. Wardlow, Ashcombe, the
Drowned Stair and Blackpit are one to a country, and which is which is
different in every game; nobody can tell you the bearing, only that it is
over the water.

None of those countries is on any map to begin with: they are made when you
sail, from the game's secret seed and the port you sailed from, and the same
country is the same land every time you come back to it. Each is mostly one
kind of ground -- the plains, a forest, a swamp, the desert, or mountains with
a little grass between -- with a little of the others, ranges of mountain and
lakes. Everywhere you can stand can be walked to from the port you arrived at:
mountains and water are never allowed to cut a piece of land off.

Every land has a town of its own as well: an inn, a shop, and a house or two,
laid out differently each time. The shop is a smithy, an armourer or a bowyer;
a third building may sell draughts. Keldmarch's own castle sells blades, mail
and bows, so you never have to go over the water to be armed -- but only the
plainest. There are six grades of blade and of mail and four of bow, and each
land's shops sell a few of them: Keldmarch the first two blades and suits of
mail and the shortbow, and each continent across the sea, in the order the
story takes you there, a grade or so better than the one before, up to the
sixth and the arbalest on the last.

## Talking to people

Walk into someone, or stand beside them and press **T**, and they greet
you; then a number asks about one of the things they know, which changes as
the story moves on, and RUN/STOP leaves. Some of them hand over what the
quest needs once you ask about the right thing.

### Online (experimental)

In the online build, every person you meet answers in their own voice, from
the world's canon and from how far you have got. The answers come from a language model running on
the machine the emulator is on, reached over the C64's modem port at 300
baud, so a reply arrives at reading pace -- which is what a 1983 modem did
anyway.

Stand beside someone in a town or the castle and press **T**, or walk into
them, then type what you want to say.

The model is whatever [Ollama](https://ollama.com) is serving, by default
`llama3.2:3b`. No account, no API key and nothing leaves your machine:

```bash
ollama serve &                 # if it is not already running
ollama pull llama3.2:3b

tools/play.sh                  # from inside the unzipped online folder
Q4K_MOCK=1 tools/play.sh       # canned replies, no model needed
Q4K_MODEL=llama3.2:1b tools/play.sh
```

The game calls the host when it starts, and the title screen says if it
could not talk: **No host on the modem** when nothing answered, or **The
host has no model** when the host answered but Ollama is not running or the
model is not pulled. Either way the game plays on without conversations:
walking into someone hands over whatever the quest needs from them, and
otherwise they have nothing for you, told in a line on the map. Nothing in
the quest depends on the modem, so the game can be finished without it.
With it, some things have to be earned: listen for the words people tell
you, because sometimes the right word is what opens a door.

People know who lives where, but not the lay of the land: the world is made
fresh for every game, so nobody will give you a bearing or a distance.
