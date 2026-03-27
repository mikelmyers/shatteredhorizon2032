"""Tests for pipeline.style.audio_checker — audio validation."""

import unittest

from pipeline.style.audio_checker import AudioChecker


class TestAudioChecker(unittest.TestCase):
    def setUp(self):
        self.config = {
            "music_bpm_range": [60, 140],
            "approved_keys": ["Cm", "Dm", "Em", "Am", "Gm", "Fm", "Bbm", "Ebm"],
            "approved_moods": ["tense", "ambient", "cinematic", "dramatic", "sparse"],
            "max_duration_seconds": 300,
        }
        self.checker = AudioChecker(self.config)

    # ------------------------------------------------------------------
    # Extension
    # ------------------------------------------------------------------

    def test_wav_passes(self):
        score, issues = self.checker.check("track.wav")
        ext_fails = [i for i in issues if "extension" in i.lower()]
        self.assertEqual(len(ext_fails), 0)

    def test_ogg_passes(self):
        score, issues = self.checker.check("track.ogg")
        ext_fails = [i for i in issues if "extension" in i.lower()]
        self.assertEqual(len(ext_fails), 0)

    def test_mp3_passes(self):
        score, issues = self.checker.check("track.mp3")
        ext_fails = [i for i in issues if "extension" in i.lower()]
        self.assertEqual(len(ext_fails), 0)

    def test_bad_extension_fails(self):
        score, issues = self.checker.check("track.aac")
        ext_fails = [i for i in issues if "extension" in i.lower()]
        self.assertGreater(len(ext_fails), 0)

    # ------------------------------------------------------------------
    # BPM
    # ------------------------------------------------------------------

    def test_bpm_in_range(self):
        score, issues = self.checker.check("track.wav", metadata={"bpm": 90})
        bpm_issues = [i for i in issues if "bpm" in i.lower()]
        self.assertEqual(len(bpm_issues), 0)

    def test_bpm_at_boundaries(self):
        score, issues = self.checker.check("track.wav", metadata={"bpm": 60})
        bpm_issues = [i for i in issues if "bpm" in i.lower()]
        self.assertEqual(len(bpm_issues), 0)

        score, issues = self.checker.check("track.wav", metadata={"bpm": 140})
        bpm_issues = [i for i in issues if "bpm" in i.lower()]
        self.assertEqual(len(bpm_issues), 0)

    def test_bpm_too_high(self):
        score, issues = self.checker.check("track.wav", metadata={"bpm": 200})
        bpm_issues = [i for i in issues if "bpm" in i.lower()]
        self.assertGreater(len(bpm_issues), 0)

    def test_bpm_too_low(self):
        score, issues = self.checker.check("track.wav", metadata={"bpm": 30})
        bpm_issues = [i for i in issues if "bpm" in i.lower()]
        self.assertGreater(len(bpm_issues), 0)

    # ------------------------------------------------------------------
    # Key
    # ------------------------------------------------------------------

    def test_approved_key(self):
        score, issues = self.checker.check("track.wav", metadata={"key": "Cm"})
        key_issues = [i for i in issues if "key" in i.lower()]
        self.assertEqual(len(key_issues), 0)

    def test_unapproved_key(self):
        score, issues = self.checker.check("track.wav", metadata={"key": "C"})
        key_issues = [i for i in issues if "key" in i.lower()]
        self.assertGreater(len(key_issues), 0)

    # ------------------------------------------------------------------
    # Mood
    # ------------------------------------------------------------------

    def test_approved_mood(self):
        score, issues = self.checker.check("track.wav", metadata={"mood": "tense"})
        mood_issues = [i for i in issues if "mood" in i.lower()]
        self.assertEqual(len(mood_issues), 0)

    def test_unapproved_mood(self):
        score, issues = self.checker.check("track.wav", metadata={"mood": "happy"})
        mood_issues = [i for i in issues if "mood" in i.lower()]
        self.assertGreater(len(mood_issues), 0)

    def test_mood_case_insensitive(self):
        score, issues = self.checker.check("track.wav", metadata={"mood": "TENSE"})
        mood_issues = [i for i in issues if "mood" in i.lower()]
        self.assertEqual(len(mood_issues), 0)

    # ------------------------------------------------------------------
    # Duration
    # ------------------------------------------------------------------

    def test_duration_within_limit(self):
        score, issues = self.checker.check("track.wav", metadata={"duration_seconds": 120})
        dur_issues = [i for i in issues if "duration" in i.lower()]
        self.assertEqual(len(dur_issues), 0)

    def test_duration_exceeds_limit(self):
        score, issues = self.checker.check("track.wav", metadata={"duration_seconds": 400})
        dur_issues = [i for i in issues if "duration" in i.lower()]
        self.assertGreater(len(dur_issues), 0)

    # ------------------------------------------------------------------
    # Score computation
    # ------------------------------------------------------------------

    def test_perfect_score(self):
        score, issues = self.checker.check(
            "track.wav",
            metadata={"bpm": 90, "key": "Dm", "mood": "ambient", "duration_seconds": 180}
        )
        self.assertEqual(score, 1.0)
        self.assertEqual(len([i for i in issues if "FAIL" in i]), 0)

    def test_all_fail_score(self):
        score, issues = self.checker.check(
            "track.aac",
            metadata={"bpm": 999, "key": "Z#", "mood": "happy", "duration_seconds": 9999}
        )
        self.assertLess(score, 0.5)

    def test_no_metadata_only_extension_checked(self):
        score, issues = self.checker.check("track.wav")
        # Only extension check — should pass
        self.assertEqual(score, 1.0)


if __name__ == "__main__":
    unittest.main()
