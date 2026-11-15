"""Cataloghi: nessuna battuta senza personaggio o con un file audio irraggiungibile."""
import json
import tempfile
import unittest
from pathlib import Path
from extract_lines import read_catalog, key, literals, interview_variants


class VoiceCatalogTests(unittest.TestCase):
    def read(self, entries):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "chapter.json"
            path.write_text(json.dumps(entries, ensure_ascii=False), encoding="utf-8")
            return list(read_catalog(path))

    def test_accepts_utf8_dialogue_without_precomputed_key(self):
        self.assertEqual(self.read([{"speaker":"Gaia", "text":"  La rete è pronta.\n", "kind":"day3"}]),
                         [("Gaia", "La rete è pronta.", "day3")])

    def test_rejects_stale_key_after_dialogue_edit(self):
        with self.assertRaises(ValueError):
            self.read([{"speaker":"Luca", "text":"Testo corretto", "key":key("Testo vecchio")}])

    def test_rejects_unknown_voice_and_empty_lines(self):
        for entry in [{"speaker":"../../altro", "text":"Testo"}, {"speaker":"Jonny", "text":" \n"}]:
            with self.subTest(entry=entry), self.assertRaises(ValueError):
                self.read([entry])

    def test_runtime_greeting_is_not_a_literal_voice_file(self):
        self.assertEqual(literals('FString::Printf(TEXT("Benvenuto in TNES, %s."), *PlayerName)'), [])
        self.assertEqual(literals('TEXT("Nel tuo primo giorno, cosa ti incuriosisce di più?")'),
                         ["Nel tuo primo giorno, cosa ti incuriosisce di più?"])

    def test_interview_count_has_four_exact_spoken_variants(self):
        source = 'FString::Printf(TEXT("Hai scelto %d attività sulle connessioni e %d sulla protezione. Ti proporrei %s."),ArrivalNetworkingAnswers,3-ArrivalNetworkingAnswers,ArrivalNetworkingAnswers>=2?TEXT("Networking con Luca"):TEXT("Cybersecurity con Jonny"))'
        rows = list(interview_variants(source))
        self.assertEqual(len(rows), 4)
        self.assertEqual(rows[0], "Hai scelto 0 attività sulle connessioni e 3 sulla protezione. Ti proporrei Cybersecurity con Jonny.")
        self.assertEqual(rows[2], "Hai scelto 2 attività sulle connessioni e 1 sulla protezione. Ti proporrei Networking con Luca.")
        self.assertFalse(any("%" in row for row in rows))


if __name__ == "__main__":
    unittest.main()
