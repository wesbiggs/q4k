# Quest for the King

A grid-based RPG roguelike for the Commodore 64, in the spirit of Ultima III.
Written in C (cc65) with assembly where it pays, run and tested under VICE.

A letter comes from King Matheson. The dragon Maldrath has been burning
Keldmarch for three winters and two armies have not come back. The blade that
can banish him was broken, and its three binding stones are sealed in three
deep places. Buy a sword, walk south, and start asking people questions.

The plot is deliberately the oldest one there is. What is not old is who you
talk to: every NPC answers in their own voice, from the world's canon and
from how far you have got, through a model running on the host at the other
end of the C64's modem port.

## Running it

The game is built from source and run in the VICE emulator; the toolchain,
and everything else about working on the game, is in
[DEVELOPERS.md](DEVELOPERS.md). Once that is installed:

```bash
make                  # build
make play             # play, with NPC dialogue if a model is running
Q4K_MOCK=1 make play  # play with canned NPC replies, no model needed
make d64 PROG=q4k     # or build a disk image to load anywhere
```

## Playing

The title screen sets the scene and asks your name; RETURN alone picks a
random one, and `F3` loads a saved game instead. Then `WASD` or the cursor
keys move. The panel shows what you are carrying and wearing, alongside your
stats and any quest items. Walk into someone to talk to them, open their
shop, or take a seat at their inn.

| key | |
|---|---|
| `T` | talk to whoever is beside you |
| `L` | look where you stand, and take anything hidden there |
| `P` | drink a healing draught |
| `F1` / `F3` | save / restore |
| `SPACE` | wait a turn |
| `?` | keys, and what you are supposed to be doing next |
| `Q` | quit |

Treasure is hidden in towns, out on the overworld and in dungeon chests, and
a square need not look like anything: stand on it and press `L`. Innkeepers
sell a mug of ale for five marks, and with it -- when the dialogue host is
on the line -- a hint at where to look or what to do next; a bed for twenty
puts you back to full health.

A healing draught restores a share of your full health, so it is worth a
turn late in the game as well as early, and its price climbs to match: five
marks, plus five for each level you have.

Saved games go to a disk in unit 8; `make play` creates `dist/saves.d64` the
first time and attaches it.

### Fighting

A fight happens on a field of the ground you were crossing, a dungeon room,
or Maldrath's lair. The monster starts somewhere in the top third of it and
comes for you; you walk into it to strike.

| key | |
|---|---|
| move | step, or strike if it is in the way |
| `F` then a direction | shoot your bow, if you have one |
| `SPACE` | stand your ground: take less from the next blow |
| `P` | drink a healing draught |
| `R` | run -- three times in four, and never from a keeper or Maldrath |

Some things shoot back. Bandits and skeleton archers carry bows, the
Drowned King throws water and Maldrath breathes fire, each whenever you are
in a straight line and within reach with nothing solid between. Beside you,
they strike like everything else. Tougher things turn up as you rise in
level: rats and goblins from the start, drakes from level seven.

## The world

The overworld is a wrapping 64x64 grid. Towns are exactly one 21x19 screen,
so they never scroll: walk in and the whole place is in front of you.
Dungeon levels are 48x32 and scroll with you, like the overworld, until you
reach an edge. A dungeon level you walk back into is the level you left.

Dungeons are dark. A room lights up whole when you stand in it, you can see
straight along a corridor until something stops the eye, and squares you
have already seen stay on the map drawn dim.

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

make play                      # live dialogue
Q4K_MOCK=1 make play           # canned replies, no model needed
Q4K_MODEL=llama3.2:1b make play
```

If Ollama is unreachable or the model is not pulled, people answer with
canned lines rather than not at all -- and nothing in the quest depends on
the modem, so the game can be finished without it. With it, some things
have to be earned: listen for the words people tell you, because sometimes
the right word is what opens a door.

People know the real lay of the land. Ask the way and they give the true
bearing and distance on foot, and tell you when the road bends a long way
round.
