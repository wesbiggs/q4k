# Quest for the King

A grid-based RPG roguelike for the Commodore 64, in the spirit of Ultima III.
Written in C (cc65) with assembly where it pays, run and tested under VICE.

A letter comes from King Matheson. The dragon Maldrath has been burning
Keldmarch for three winters and two armies have not come back. The blade that
can banish him was broken, and its three binding stones are sealed in three
deep places. Take a dagger from the smith, find the king, and start asking people questions.

The plot is deliberately the oldest one there is. What is not old is who you
talk to: every NPC answers in their own voice, from the world's canon and
from how far you have got, through a model running on the host at the other
end of the C64's modem port.

## Getting it

Download the files from the
[latest release](https://github.com/wesbiggs/q4k/releases/latest). There are
three, and you only need one:

| file | |
|---|---|
| `q4k-<version>.zip` | the game and everything people need to talk -- start here |
| `q4k-<version>.prg` | the game alone |
| `q4k-<version>.d64` | the game on a disk image, for a real C64 or any emulator |

All of them run in [VICE](https://vice-emu.sourceforge.io), the Commodore
emulator (`brew install vice` on a Mac).

**The zip** (macOS or Linux, with Python 3). Unzip it and start the game
from inside the folder; this brings up the program that lets people talk,
connects the emulator's modem port to it, and gives you a disk to save on:

```bash
cd q4k-0.9.0
tools/play.sh
```

**The .prg** runs on its own: open it in VICE, or drop it on the window.
With nothing on the other end of the modem there are no conversations (see
[Talking to people](#talking-to-people)), but the game can still be
finished. To save you need a disk in drive 8: attach any blank `.d64` from
VICE's File menu.

**The .d64** is the same game on a disk. In VICE, or on a real machine with
an SD card drive: `LOAD"Q4K",8` and `RUN`.

To build it yourself instead, see [DEVELOPERS.md](DEVELOPERS.md).

## Playing

The title screen sets the scene and asks your name; RETURN alone picks a
random one, `F3` loads a saved game instead, and `F5` lets you choose the
world's seed (1 to 255; the same seed makes the same lands and towns). The `?`
screen shows the seed in play. Then `WASD` or the cursor
keys move. The panel shows what you are carrying and wearing, alongside your
stats and any quest items. Walk into someone to talk to them, open their
shop, or take a seat at their inn.

| key | |
|---|---|
| `>` / `<` | go in at a town or dungeon, or down a stair / out of a town, or up a stair |
| `L` | look where you stand, and take anything hidden there |
| `I` | read your treasure maps |
| `P` | drink a healing draught |
| `F1` / `F3` | save / restore (each asks `Y` first) |
| `SPACE` | guard, in a fight |
| `?` | keys, and what you are supposed to be doing next |
| `M` / `X` | music / sound effects on or off (costs no turn) |
| `Q` | quit (asks `Y` first) |

Treasure is hidden in the castle and in dungeon chests, and
a square need not look like anything: stand on it and press `L`. Innkeepers
sell a mug of ale for five marks, and with it -- when the dialogue host is
on the line -- a hint at where to look or what to do next; a bed for twenty
puts you back to full health.

Now and then a beaten monster drops a treasure map. It gives the land and how
many leagues east or west and north or south of a town the treasure is buried,
a league being one step. Walk there, press `L`, and dig. You can hold four.

A healing draught restores a share of your full health, so it is worth a
turn late in the game as well as early, and its price climbs to match: five
marks, plus five for each level you have.

Saved games go to a disk in drive 8. `tools/play.sh` makes one,
`dist/saves.d64`, the first time and attaches it every time after.

### Fighting

A fight happens on a field of the ground you were crossing, a dungeon room,
or Maldrath's lair. The monster starts somewhere in the top third of it and
comes for you; you walk into it to strike.

| key | |
|---|---|
| move | step, or strike if it is in the way |
| `F` then a direction | shoot your bow, if you have one (one arrow a shot) |
| `SPACE` | stand your ground: take less from the next blow |
| `P` | drink a healing draught |
| walk off the side | the gap in the middle of the left or right edge is the only way to flee, and it always works if you reach it -- but not from a keeper or Maldrath |

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
and bows, so you never have to go over the water to be armed.

## Talking to people

Every person you meet answers in their own voice, from the world's canon and
from how far you have got. The answers come from a language model running on
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

tools/play.sh                  # from inside the unzipped folder
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
