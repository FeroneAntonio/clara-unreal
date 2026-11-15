import json
from pathlib import Path
import tempfile
import unittest
import wave

from audit_voices import audit


class VoiceAuditTests(unittest.TestCase):
    def test_missing_invalid_and_verified_clips_are_distinguished(self):
        lines = [{"speaker": "Luca", "key": key, "kind": "dialogue", "text": "Prendi il cavo."}
                 for key in ("missing", "broken", "valid")]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "Luca").mkdir()
            (root / "Luca" / "broken.wav").write_bytes(b"unfinished")
            with wave.open(str(root / "Luca" / "valid.wav"), "wb") as sound:
                sound.setparams((1, 2, 24000, 0, "NONE", "not compressed"))
                sound.writeframes(b"\0\0" * 2400)
            record = {**lines[2], "heard": lines[2]["text"]}
            (root / "voices.jsonl").write_text(json.dumps(record) + "\n")
            report = audit(lines, root)
        self.assertEqual(report["present"], 2)
        self.assertEqual([row["key"] for row in report["missing"]], ["missing"])
        self.assertEqual([row["key"] for row in report["invalid"]], ["broken"])
        self.assertEqual([row["key"] for row in report["review"]], ["broken"])


if __name__ == "__main__":
    unittest.main()
