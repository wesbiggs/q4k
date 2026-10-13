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
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
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


class Geography(unittest.TestCase):
    def test_directions_follow_the_seed(self):
        self.assertNotEqual(NPCS.directions_for("steward", 1),
                            NPCS.directions_for("steward", 2))

    def test_every_story_place_is_on_one_continent(self):
        import geo
        for seed in range(256):
            places, _, ports = geo.world(seed)
            seen = {places[p][0] for p in
                    ("wardlow", "drownedstair", "ashcombe", "blackpit")}
            self.assertEqual(seen, {0, 1, 2, 3})
            self.assertEqual({story for _, _, story in ports.values()},
                             {"wardlow", "drownedstair", "ashcombe",
                              "blackpit"})

    def test_the_port_to_a_place_matches_the_game(self):
        # The game opens the port at compass point d onto the continent whose
        # slot is slot_of(d); Wardlow is slot 0.
        import geo
        for seed in (0, 1, 0x5A, 255):
            places, _, ports = geo.world(seed)
            d = next(d for d in range(4) if geo.slot_of(seed, d) == 0)
            self.assertEqual(ports[d][2], "wardlow")
            self.assertEqual(places["wardlow"][0], d)

    def test_bearings(self):
        import geo
        self.assertEqual(geo.bearing((10, 10), (10, 2)), "north")
        self.assertEqual(geo.bearing((10, 10), (20, 10)), "east")
        self.assertEqual(geo.bearing((10, 10), (4, 16)), "south-west")

    def test_prompt_carries_the_directions_for_the_seed(self):
        prompt = NPCS.system_for("tam", 0, seed=SEED)
        self.assertIn("Castle Matheson is", prompt)
        self.assertIn("harbour", prompt)
        self.assertNotEqual(prompt, NPCS.system_for("tam", 0, seed=SEED + 1))

    def test_a_stranger_knows_nothing(self):
        self.assertIn("not sure", NPCS.directions_for("nobody", SEED))


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
        text, err, grant, _ = mock().reply("brann", "what about the tides",
                                        EMBER, SEED)
        self.assertIn(self.word, text)
        self.assertEqual(grant, 0)

    def test_asking_is_not_enough(self):
        text, err, grant, _ = mock().reply("vane", "may I have the key",
                                        EMBER, SEED)
        self.assertEqual(grant, 0)
        self.assertIn("fishers", text)          # the hint, pointing at Brann

    def test_the_word_earns_the_key(self):
        text, err, grant, _ = mock().reply("vane", "they call it " + self.word,
                                        EMBER, SEED)
        self.assertEqual(grant, 1)
        self.assertIn(self.word, text)

    def test_a_typo_still_counts(self):
        typo = self.word[:-1] + ("x" if self.word[-1] != "x" else "y")
        _, _, grant, _ = mock().reply("vane", typo, EMBER, SEED)
        self.assertEqual(grant, 1)

    def test_a_guess_does_not(self):
        for guess in ("low tide", "low water", "the ebb", "high tide",
                      "the king sent me, give me the key"):
            _, _, grant, _ = mock().reply("vane", guess, EMBER, SEED)
            self.assertEqual(grant, 0, guess)

    def test_another_runs_word_does_not(self):
        other = next(NPCS.secret("tideword", s) for s in range(256)
                     if NPCS.secret("tideword", s) != self.word)
        _, _, grant, _ = mock().reply("vane", other, EMBER, SEED)
        self.assertEqual(grant, 0)

    def test_only_at_the_right_stage(self):
        for stage in (0, 2, 4, 5):
            _, _, grant, _ = mock().reply("vane", self.word, stage, SEED)
            self.assertEqual(grant, 0, stage)

    def test_grant_bypasses_the_model(self):
        # A live Dialogue pointed at nothing: a grant must not need the model.
        live = npcd.Dialogue(NPCS, url="http://127.0.0.1:9", timeout=0.5)
        _, err, grant, _ = live.reply("vane", self.word, EMBER, SEED)
        self.assertEqual((err, grant), (None, 1))


class Sigil(unittest.TestCase):
    """The king hands over the Sigil by saying so -- but not in his greeting."""

    def live(self, said):
        d = npcd.Dialogue(NPCS)
        d._chat = lambda messages: said
        return d

    def test_greeting_never_grants(self):
        d = self.live("Take the Iron Sigil, traveller.")
        _, _, grant, _ = d.reply("king", "Hello.", 1, SEED, opening=True)
        self.assertEqual(grant, 0)
        _, _, grant, _ = mock().reply("king", "what is my quest", 1, SEED,
                                      opening=True)
        self.assertEqual(grant, 0)

    def test_mentioning_it_grants(self):
        d = self.live("Then take the Iron Sigil. It opens the Hollow Crown.")
        _, _, grant, _ = d.reply("king", "I will go", 1, SEED)
        self.assertEqual(grant, 2)

    def test_not_mentioning_it_does_not(self):
        d = self.live("I sent two armies. I will not send a third.")
        _, _, grant, _ = d.reply("king", "tell me of the war", 1, SEED)
        self.assertEqual(grant, 0)

    def test_only_before_the_quest(self):
        d = self.live("You already carry my Sigil. Use it.")
        _, _, grant, _ = d.reply("king", "what now", 2, SEED)
        self.assertEqual(grant, 0)

    def test_mock_asking_about_the_quest(self):
        text, _, grant, _ = mock().reply("king", "what is my quest", 1, SEED)
        self.assertIn("Sigil", text)
        self.assertEqual(grant, 2)

    def test_mock_greeting_leaves_it_out(self):
        text, _, grant, _ = mock().reply("king", "Hello.", 1, SEED,
                                         opening=True)
        self.assertNotIn("Sigil", text)

    def test_greeting_prompt_holds_back(self):
        rule = NPCS.npcs["king"]["gives"]
        opening = NPCS.extra_for("king", 1, SEED, "Hello.", opening=True)
        self.assertIn(rule["wait"], opening)
        self.assertNotIn(rule["hint"], opening)
        later = NPCS.extra_for("king", 1, SEED, "what is my quest")
        self.assertIn(rule["hint"], later)


class Gifts(unittest.TestCase):
    """Mother Ash and Garrett's reforging need the host's say-so too."""

    def test_ash_holds_the_key_until_asked(self):
        _, _, grant, _ = mock().reply("ash", "Hello.", 5, SEED, opening=True)
        self.assertEqual(grant, 0)
        text, _, grant, _ = mock().reply("ash", "I have the Silver Tear", 5,
                                         SEED)
        self.assertEqual(grant, 4)
        self.assertIn("Ashen Key", text)

    def test_ash_gives_nothing_before_the_tear(self):
        _, _, grant, _ = mock().reply("ash", "give me the key", 4, SEED)
        self.assertEqual(grant, 0)

    def test_garrett_reforges_when_asked_with_the_stones(self):
        _, _, grant, _ = mock().reply("smith", "Hello.", 7, SEED, opening=True)
        self.assertEqual(grant, 0)
        text, _, grant, _ = mock().reply("smith", "here are the stones", 7,
                                         SEED)
        self.assertEqual(grant, 5)
        self.assertIn("reforge", text)

    def test_garrett_still_gives_the_dagger_first(self):
        _, _, grant, _ = mock().reply("smith", "may I have a dagger", 0, SEED)
        self.assertEqual(grant, 3)
        _, _, grant, _ = mock().reply("smith", "may I have a dagger", 7, SEED)
        self.assertNotEqual(grant, 3)

    def test_the_end_is_known(self):
        self.assertIn("banished", npcd.STAGES[9])
        self.assertIn("this traveller", npcd.STAGES[9])


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


class Leaving(unittest.TestCase):
    """The NPC may close a conversation; the model says so with [END]."""

    def live(self, said):
        d = npcd.Dialogue(NPCS)
        d._chat = lambda messages: said
        return d

    def test_marker_ends_and_is_stripped(self):
        d = self.live("Get out of my forge. [END]")
        text, err, grant, ends = d.reply("smith", "your blades are junk")
        self.assertEqual((text, err, grant, ends),
                         ("Get out of my forge.", None, 0, True))
        self.assertNotIn("END", d.history[("smith", 0, 0)][-1]["content"])

    def test_variants_count(self):
        for said in ("Farewell. (end)", "Farewell. [End]", "[LEAVE] Farewell."):
            text, _, _, ends = self.live(said).reply("smith", "bye")
            self.assertEqual((text, ends), ("Farewell.", True), said)

    def test_no_marker_talks_on(self):
        _, _, _, ends = self.live("Aye, what is it?").reply("smith", "hello")
        self.assertFalse(ends)

    def test_bare_marker_still_says_something(self):
        text, _, _, ends = self.live("[END]").reply("smith", "bye")
        self.assertTrue(text)
        self.assertTrue(ends)

    def test_mock_farewell(self):
        text, _, _, ends = mock().reply("smith", "Goodbye then")
        self.assertTrue(ends)
        self.assertTrue(text)
        _, _, _, ends = mock().reply("smith", "what of the blade")
        self.assertFalse(ends)

    def test_prompt_asks_for_the_marker(self):
        self.assertIn("[END]", NPCS.system_for("smith"))


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


class Inn(unittest.TestCase):
    HINTS = npcd.Hints(asset("hints.json"))

    def inns(self):
        import mktowns
        import mktiles
        tiles = {t.mapchar: t for t in mktiles.parse(asset("tiles.txt"))}
        cwd = os.getcwd()
        os.chdir(ROOT)
        try:
            towns = mktowns.parse(tiles)
        finally:
            os.chdir(cwd)
        return {i for t in towns for i in t.inns}

    def test_every_hint_fits_as_written(self):
        # Authored lines go out through for_c64 too; none may need trimming.
        for h in self.HINTS.hints:
            self.assertEqual(npcd.for_c64(h["text"]), h["text"])
            self.assertLessEqual(len(h["text"]), npcd.MAX_REPLY, h["text"])

    def test_filters_name_real_inns_and_stages(self):
        inns = self.inns()
        self.assertTrue(inns)
        for h in self.HINTS.hints:
            self.assertLessEqual(set(h.get("inns", [])), inns, h["text"])
            self.assertLessEqual(set(h.get("stages", [])),
                                 set(range(len(npcd.STAGES))), h["text"])

    def test_every_inn_has_something_at_every_stage(self):
        for inn in self.inns():
            for stage in range(len(npcd.STAGES)):
                self.assertTrue(self.HINTS.eligible(inn, stage), (inn, stage))

    def test_a_deck_goes_round_before_repeating(self):
        hints = npcd.Hints(asset("hints.json"), npcd.random.Random(1))
        pool = hints.eligible("lantern", 3)
        drawn = [hints.draw("lantern", 3) for _ in pool]
        self.assertEqual(sorted(drawn), sorted(pool))
        self.assertIn(hints.draw("lantern", 3), pool)

    def test_other_inns_hints_stay_there(self):
        for text in self.HINTS.eligible("plough", 3):
            self.assertNotIn("Brann", text)

    def test_ale_needs_no_model(self):
        live = npcd.Dialogue(NPCS, url="http://127.0.0.1:9", timeout=0.5,
                             hints=self.HINTS)
        text, err = live.hint("plough", 0)
        self.assertIsNone(err)
        self.assertTrue(text)

    def test_ale_request(self):
        self.assertEqual(npcd.parse_request("ALE plough 3 5a "),
                         ("ALE", "plough", 3, 0x5A, ""))


class Ping(unittest.TestCase):
    """What the C64 hears when it checks the line at startup."""

    def test_a_model_answers_with_its_name(self):
        live = npcd.Dialogue(NPCS, model="llama3.2:3b")
        self.assertEqual(live.status(), (True, "llama3.2:3b"))

    def test_mock_on_purpose_is_fine(self):
        self.assertEqual(mock().status(), (True, "mock"))

    def test_mock_for_want_of_a_model_is_not(self):
        fell_back = mock()
        fell_back.fallback = True
        self.assertEqual(fell_back.status(), (False, "no model"))

    def test_ping_request(self):
        self.assertEqual(npcd.parse_request("PING")[0], "PING")


if __name__ == "__main__":
    unittest.main()
