# Changelog

Each release's section here became its notes on GitHub. The release
workflow went with the classic build; the sections up to 0.11.0 are the
classic game's.

## Unreleased

- Licensed: the code under the GPL, version 3 or later (`LICENSE`), the
  game's content in `assets/` under CC BY-SA 4.0 (`assets/LICENSE`), with
  an SPDX line in every hand-written source. The README says it is a work
  in progress and how to contribute.
- The classic game, which moved a square at a time (`q4k.prg` on its
  `.d64`), is retired, with its experimental networked build and the
  dialogue host: the open world (`dist/openworld.d81`) is the game now. The
  0.11.0 tag builds the classic one.
- The open world scrolls smoothly over multicolour tiles, a pixel or two a
  frame (`1` or `2` chooses), and the party walks eight ways: the cursor
  keys, `WASD` or a joystick in port 2, and `Q` `E` `Z` `X` or the stick's
  diagonals go aslant. Keldmarch lies in the middle of a sea that wraps
  round both ways, with a continent across the water to each side of it, and
  roads join its towns, the castle and its four ports; things wander out
  less often on a road. Ships lie at Keldmarch's ports and the continents'
  docks: step onto one to board it, and it waits wherever you step off.
  Those at Keldmarch's ports sail only as the quest moves on.
- Places are entered and left by walking, not with `>` and `<`: step onto a
  town, the castle or a dungeon to go in, onto a way out of a town or the
  castle to leave, and onto a dungeon's stairs to go down or up (up from the
  first level is out). The text window says which place you are entering. In
  a town a building you are not in shows only its roof, which comes off as
  you step in at the door. A chest opens when you stand on it, and looks
  open once it is looted.
- Underground the party sees the room it stands in, the squares round it and
  straight along the corridors; the rest is dark until it has been near, and
  what has been seen stays on the map, for every level, until you leave the
  dungeon (a save does not keep it). A dungeon's monsters are on its map,
  where they were left, and come for the party once they see it: one that
  reaches you, or that you walk into, is a fight, and one you cannot see is
  not shown. Walk into a keeper to fight it.
- The two lines under the map show each member's health (green over half,
  yellow down to a quarter, red under it) and a wizard's magic, with the
  party's draughts, arrows and gold. Anything said takes them until the next
  step; `SPACE` hides them for a bigger map.
- Keys on the map: `L` looks where you stand, `P` is a draught for whoever
  is worst hurt, `I` is the party screen, `M` and `SHIFT+M` turn the music
  and the effects off and on, `?` lists the keys and what to do next, and
  RUN/STOP quits to the title (asking `Y` first). `F1` and `F3` save and
  restore, as before.
- The game saves to its own disk (`ow-save`), and loads the parts of the
  world, the fights, the shops and the conversations from it as they are
  wanted, so keep the disk in the drive. It is made for a fast drive that
  takes a D81 (a TeensyROM, an SD card drive); with an REU, everything
  loaded is kept there after the first time, and the screen scrolls through
  it. A save from 0.11.0 or before will not load.
- The title picture takes `F1` for a new game and `F3` for the saved one,
  and a new game's world comes from how long the picture waited: `F5` no
  longer chooses it. Tam, the princess and the king are shown in a portrait
  beside what they say.
- A party: the king's letter went to strangers, and up to three of them meet
  in Millford. A new game tells the story and then offers a warrior, an
  archer and a wizard, named for you. `1`-`3` opens one's page, where `C`
  changes their class, `N` their name, `P` their pronouns (he/him, she/her,
  they/them or it), `S` their skin's colour and `T` their tunic's, and DEL
  sends them home; `A` adds another while there are fewer than three, and
  RETURN begins, saving the new game. Garrett arms each by class -- a
  dagger, a hunting bow (and arrows), a staff -- and Millford's gate stays
  shut until he has. Gold, draughts (one each to start), arrows, experience
  and the level are the party's; health, arms and a wizard's magic are each
  member's own. A fourth place is kept for a companion who joins later.
- Only a warrior wields anything better than a short sword, only an archer
  draws anything better than a shortbow, and a wizard wears nothing heavier
  than robes, which now go between travel clothes and leather. Whatever is
  bought or dropped goes to whoever it would be better for, and you are
  asked who when that is more than one of you; what they had goes on to
  whoever it would be better for in turn.
- In a fight everyone stands on the field, fanned out, and takes a turn,
  walking and striking eight ways; monsters go for whoever they can reach
  soonest. `P` is a draught for whoever's turn it is. A member who falls
  gets up after a fight that is won or fled; the fight is lost only when
  everyone falls. Fleeing takes the whole party: each walks out of a side's
  gap on their own turn, and the fight is fled when the last one is out.
  Sand and swamp are slow going. A monster that comes alone is tougher for
  each of the party past the first.
- The up-arrow key (or the stick's button) aims a bow, instead of `F` and a
  direction: a yellow cursor marks the nearest foe in reach with a clear
  line, the direction keys go through the others, `SPACE` looses, and any
  other key lowers the bow without spending the turn. A shot flies straight
  at its foe at any angle, but nothing can be aimed at past a boulder, a
  tree, a wall, another foe or one of the party.
- Wizards cast (`*`) the spells they have prepared, for magic that comes
  back slowly on the road, fully at an inn and with a level: Spark, Heal,
  Ward, Sleep, Bolt and Blaze, with Spark and Bolt aimed as a bow is. Every
  wizard knows Spark; the others are books, each from a level, at two magic
  shops: Keldmarch's own town (not Millford) sells Heal, Ward and Sleep, and
  Ashcombe's, across the sea, Ward, Sleep, Bolt and Blaze. A wizard prepares
  two spells at first and one more every third level, up to five (`I`, then
  `S`).
- `I` shows the party one at a time and what each carries: cursor left and
  right go through them, `1`-`3` puts the one shown in that place (the first
  leads), `T` changes their tunic's colour, `P` their pronouns, `S` prepares
  a wizard's spells and `M` lists the treasure maps.
- As in the classic game, a fight won now and then leaves something: arms
  from a foe that goes armed, for whoever in the party they would be better
  for, and out on the land a treasure map, which says where on that land
  something is buried. `M` on the party screen lists the maps by the nearest
  place; stand on the spot and press `L` to dig it up.
- The fights are balanced for a party, measured by having the arena play
  itself hundreds of times (`tools/balance.py`). A keeper, Maldrath or any
  monster that comes alone has more health and hits harder for each of the
  party past the first, and moves between their turns. Archers' arrows and
  wizards' spells grow stronger with the party's level, wizards turn some of
  every blow with their own wards and regain a point of magic each round of
  a fight, and Ward is a little stronger.
- `make` builds the disk and `make run` plays it in VICE (`make run-reu`
  with a 512K REU); `make playtown TOWN=id` starts a saved game inside one
  town, for trying it out. CI and `tools/cloud-setup.sh` take cc65 and VICE
  from wesbiggs/c64-cloud-toolchain.

## 0.11.0 — 2026-11-06

- The offline build takes a joystick in port 2: it moves on the map and in a
  fight, and the button is `F` in a fight and `>` or `<` on the map, whichever
  fits where the hero stands. A held direction repeats.
- Leaving a continent's town now redraws the continent at once, instead of
  showing the town until the next move.

## 0.10.0 — 2026-11-01

- An offline build, `q4k-0.10.0-offline.d64`: no modem and no host. What
  everyone says is on the disk, picked from a menu of topics, and the inns'
  rumours with it. Keep the disk in the drive. It also has each continent's
  own monsters, arms that monsters drop, and lightning.
- Towns, dungeons and stairs are entered with `>` and left with `<`, standing
  on them, rather than by walking in; ports and the dragon's lair still work
  by walking. Saving and restoring ask for `Y` first.
- Moving the story on shows the next step in the message window, as on the
  `?` screen, which also shows the world's seed. `F5` on the title screen
  chooses the seed for a new game.
- Dungeon fights are half as likely per step. Shops say one thing on a sale
  and towns no longer announce themselves or the road out.
- The music stops while the world is built (the first time out of Millford)
  and comes back after, if it was on.
- Fights come about half as often, and there is no running (`R` is gone):
  fight, or reach a gap in the side of the field.
- Deeper dungeon levels draw tougher monsters, in bigger groups, and their
  chests hold more.
- `L` where there is nothing to find can raise the dead.
- Mother Ash and Garrett hand things over when asked, not the moment you
  walk up; once Maldrath is banished the people know it.
- Treasure maps measure from the nearest place, not always Millford.
- The bow shoots arrows, one a shot: a first bow comes with twelve, the bowyer
  sells ten for 12 marks (fourth item on his list), and some fights leave a
  few. A quiver holds forty. The panel shows how many; saves are `Q4K8`.
- Fight fields have an exit in the middle of each side, with about ten boulders in
  different places every time; walking out of one is a sure way to run,
  if the thing chasing you lets you get there.
- More rumours, and the place's name no longer flashes under a fight.

## 0.9.0 — 2026-09-26

The first numbered release.

- Keldmarch: a wrapping overworld, towns and a castle to walk into, and three
  dungeons whose levels are generated, dark, and lit as you explore.
- People who answer in their own voices, through a language model on the
  machine running the emulator, reached over the C64's modem port. They know
  the real roads, and some things have to be earned by what you say. The
  game checks the line at startup and says on the title screen if there is
  no host or no model; then people simply hand over what the quest needs,
  and the game can still be finished.
- Shops for blades, armour, bows and draughts; inns with ale, rumours and a
  bed; treasure hidden in towns, in the open and in dungeon chests (`L`).
- Fights on a field of the ground you were crossing. Monsters start anywhere
  in the top third, and some shoot back: bandits and skeleton archers with
  bows, the Drowned King with water, Maldrath with fire.
- Eleven kinds of wandering monster, each turning up from its own level, from
  rats to drakes, plus three dungeon keepers and Maldrath himself.
- Saved games on a disk in unit 8 (`F1` saves, `F3` restores, and `F3` on the
  title screen loads).
- The version is shown on the title screen.
