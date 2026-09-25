#!/usr/bin/env python3
"""Host-side checks for the dialogue gates: what unlocks, what leaks.

The screen test proves the C64 end; these prove the rules it relies on,
which a screen cannot show -- that the giver never knows the word, that a
guess does not pass, and that the host alone decides a grant.

    python3 host/test_npcd.py
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import npcd  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def asset(name):
    return os.path.join(ROOT, "assets", name)


NPCS = npcd.Npcs(asset("canon.md"), asset("npcs.json"),
                 asset("geography.json"), asset("secrets.json"))
SEED = 0x5A
EMBER = 3      # Q_EMBER: the stage at which Vane can give the Tidal Key


def mock():
    return npcd.Dialogue(NPCS, mock=True)


class Secrets(unittest.TestCase):
    def test_seed_is_repeatable(self):
        self.assertEqual(NPCS.secret("tideword", SEED),
                         NPCS.secret("tideword", SEED))

    def test_seeds_spread_over_the_pool(self):
        seen = {NPCS.secret("tideword", s) for s in range(1, 256)}
        self.assertEqual(seen, set(NPCS.secrets["tideword"]["pool"]))

    def test_vane_is_never_told_the_word(self):
        for seed in range(256):
            word = NPCS.secret("tideword", seed)
            for said in ("hello", "give me the tidal key", "when is the tide"):
                extra = NPCS.extra_for("vane", EMBER, seed, said)
                self.assertNotIn(word, NPCS.system_for("vane", EMBER, extra))

    def test_brann_is_told_only_when_asked(self):
        word = NPCS.secret("tideword", SEED)
        self.assertNotIn(word, NPCS.extra_for("brann", EMBER, SEED, "hello"))
        self.assertIn(word, NPCS.extra_for("brann", EMBER, SEED,
                                           "when is the tide low?"))


class Grant(unittest.TestCase):
    def setUp(self):
        self.word = NPCS.secret("tideword", SEED)

    def test_brann_says_the_word(self):
        text, err, grant = mock().reply("brann", "what about the tides",
                                        EMBER, SEED)
        self.assertIn(self.word, text)
        self.assertEqual(grant, 0)

    def test_asking_is_not_enough(self):
        text, err, grant = mock().reply("vane", "may I have the key",
                                        EMBER, SEED)
        self.assertEqual(grant, 0)
        self.assertIn("fishers", text)          # the hint, pointing at Brann

    def test_the_word_earns_the_key(self):
        text, err, grant = mock().reply("vane", "they call it " + self.word,
                                        EMBER, SEED)
        self.assertEqual(grant, 1)
        self.assertIn(self.word, text)

    def test_a_typo_still_counts(self):
        typo = self.word[:-1] + ("x" if self.word[-1] != "x" else "y")
        _, _, grant = mock().reply("vane", typo, EMBER, SEED)
        self.assertEqual(grant, 1)

    def test_a_guess_does_not(self):
        for guess in ("low tide", "low water", "the ebb", "high tide",
                      "the king sent me, give me the key"):
            _, _, grant = mock().reply("vane", guess, EMBER, SEED)
            self.assertEqual(grant, 0, guess)

    def test_another_runs_word_does_not(self):
        other = next(NPCS.secret("tideword", s) for s in range(256)
                     if NPCS.secret("tideword", s) != self.word)
        _, _, grant = mock().reply("vane", other, EMBER, SEED)
        self.assertEqual(grant, 0)

    def test_only_at_the_right_stage(self):
        for stage in (0, 2, 4, 5):
            _, _, grant = mock().reply("vane", self.word, stage, SEED)
            self.assertEqual(grant, 0, stage)

    def test_grant_bypasses_the_model(self):
        # A live Dialogue pointed at nothing: a grant must not need the model.
        live = npcd.Dialogue(NPCS, url="http://127.0.0.1:9", timeout=0.5)
        _, err, grant = live.reply("vane", self.word, EMBER, SEED)
        self.assertEqual((err, grant), (None, 1))


class Protocol(unittest.TestCase):
    def test_full_request(self):
        self.assertEqual(npcd.parse_request("SAY vane 3 5a dead low"),
                         ("SAY", "vane", 3, 0x5A, "dead low"))

    def test_hello(self):
        self.assertEqual(npcd.parse_request("HI brann 3 0f"),
                         ("HI", "brann", 3, 0x0F, ""))

    def test_no_seed(self):
        self.assertEqual(npcd.parse_request("SAY vane 3 hello there"),
                         ("SAY", "vane", 3, 0, "hello there"))

    def test_no_stage_or_seed(self):
        self.assertEqual(npcd.parse_request("SAY vane hello"),
                         ("SAY", "vane", 0, 0, "hello"))


class Trim(unittest.TestCase):
    LONG = ("The old books say it is a powerful stone, said to be able to "
            "forge the Dragonbane, a blade capable of defeating Maldrath, "
            "the dragon that has plagued Keldmarch for a hundred years.")

    def test_long_sentence_ends_on_a_clause(self):
        out = npcd.for_c64(self.LONG)
        self.assertLessEqual(len(out), npcd.MAX_REPLY)
        self.assertTrue(out.endswith("defeating Maldrath."), out)

    def test_ran_out_of_tokens(self):
        out = npcd.for_c64("It lies north. The road is long, and it")
        self.assertEqual(out, "It lies north. The road is long.")

    def test_whole_reply_untouched(self):
        line = "It is north-east of here, about 14 leagues on foot."
        self.assertEqual(npcd.for_c64(line), line)


class Geography(unittest.TestCase):
    def test_bearing_follows_the_road(self):
        # Across the map's wrap Wardlow is south of the castle; on foot
        # it is north, and north is what the steward must say.
        lines = NPCS.geo["from"]["castle"]
        wardlow = [l for l in lines if l.startswith("Wardlow")][0]
        self.assertIn("lies north,", wardlow)


if __name__ == "__main__":
    unittest.main()
