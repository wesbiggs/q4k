# Changelog

Each release's section here becomes its notes on GitHub; the release
workflow reads the one headed with the tag's version.

## Unreleased

- The offline build loads its engines from the disk: the map, the fights,
  conversations and the inns, and the shops each come in when they are wanted,
  which frees about 14K for what comes next. Each scene brings its own music
  (the same tune everywhere for now), which starts again at every fight and
  conversation. Keep the disk in the drive: without it the game cannot leave
  the title screen, and it asks for the disk back if a load fails. Meant for
  a fast drive (TeensyROM's vdrive); a stock 1541 would take most of a minute
  for every fight. See ENGINES.md.
- The offline build is now the game: `q4k-<version>.d64`, and what `make`
  builds. The networked one, where people answer through a language model, is
  experimental: `q4k-<version>-online.d64`, and `q4k-<version>-online.zip`
  with the host (`make online`, `make play`). There is no `.prg` on its own,
  since the game needs its disk.
- The online build loads its engines from the disk too, and so has everything
  the offline one has: each continent's own monsters, the arms monsters drop,
  lightning, and the joystick in port 2. The two differ only in who talks.
  `tools/play.sh` plays from a copy of its disk, `dist/online/saves.d64`, and
  carries your save over when the game is rebuilt.

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
