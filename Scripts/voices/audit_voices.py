"""Verifica copertura, formato WAV e corrispondenza delle trascrizioni senza caricare modelli.

    python audit_voices.py voice_lines.json Content/Audio/Voices

La corrispondenza ASR individua file da ascoltare: non certifica timbro, recitazione o naturalezza.
"""
import argparse
import json
from pathlib import Path
import wave

from voice_quality import accepted, quality


def audit(lines, root):
    manifest = root / "voices.jsonl"
    records = {}
    if manifest.exists():
        for row in manifest.read_text(encoding="utf-8").splitlines():
            record = json.loads(row)
            records[(record["speaker"], record["key"])] = record
    missing, invalid, review = [], [], []
    present = 0
    for line in lines:
        identity = (line["speaker"], line["key"])
        path = root / identity[0] / (identity[1] + ".wav")
        label = {"speaker": identity[0], "key": identity[1], "kind": line["kind"], "text": line["text"]}
        if not path.exists():
            missing.append(label)
            continue
        present += 1
        try:
            with wave.open(str(path), "rb") as sound:
                valid = (sound.getsampwidth() == 2 and sound.getnchannels() == 1
                         and 8000 <= sound.getframerate() <= 192000 and sound.getnframes() > 0
                         and len(sound.readframes(sound.getnframes())) == sound.getnframes() * 2)
            if not valid:
                invalid.append(label)
        except (wave.Error, EOFError):
            invalid.append(label)
        record = records.get(identity, {"text": line["text"]})
        if record.get("text") != line["text"] or not accepted(record):
            score, addresses = quality(line["text"], record.get("heard", ""))
            review.append({**label, "match": round(score, 3), "addresses_match": addresses,
                           "heard": record.get("heard", "")})
    return {"expected": len(lines), "present": present, "missing": missing, "invalid": invalid, "review": review}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lines", type=Path)
    parser.add_argument("voices", type=Path)
    args = parser.parse_args()
    report = audit(json.loads(args.lines.read_text(encoding="utf-8")), args.voices)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(bool(report["missing"] or report["invalid"] or report["review"]))


if __name__ == "__main__":
    main()
