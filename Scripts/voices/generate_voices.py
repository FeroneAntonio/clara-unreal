"""Doppia le battute di CLARA con Qwen3-TTS 1.7B Base, in locale (Mac con MPS, Windows/Linux con CUDA, oppure CPU).

    python generate_voices.py <voice_lines.json> <cartella voci di riferimento> <Content/Audio/Voices> [opzioni]

    --only Luca,Alessia     solo alcuni personaggi
    --kinds dialogue,hint   solo alcuni tipi di battuta (dialogue, hint, day2)
    --limit N               al massimo N battute nuove (per una prova veloce)
    --tries N               tentativi per battuta (predefinito 3)
    --retry-low-quality     ricontrolla anche i file presenti usando le trascrizioni salvate
    --min-match N           corrispondenza minima (predefinito 0.9); gli scarti restano nei riferimenti/review
    --model-path CARTELLA   snapshot Qwen locale, anche trasferita da un altro computer

Per ogni personaggio usa <riferimenti>/<Nome>/ref.wav e ref.json (prodotti da design_voices.py): tutte le battute
escono con la stessa voce. Ogni battuta viene riascoltata con Whisper e rifatta se non corrisponde al testo; si tiene
la migliore solo se supera la soglia e gli indirizzi IP corrispondono. Gli scarti non vengono riprodotti dal gioco. I file gia' presenti vengono saltati: si puo' interrompere e riprendere. Uscita: WAV mono PCM 16 bit
in <Voices>/<Nome>/<chiave>.wav, dove la chiave e' quella calcolata dal gioco per quel testo.
"""
import json
import os
import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf

from voice_quality import spoken, quality, accepted, retry_seed_offset


def model_path(repo_id):
    try:
        from huggingface_hub import snapshot_download
        return snapshot_download(repo_id, local_files_only=True)
    except Exception as error:
        raise RuntimeError(f"Modello non disponibile nella cache locale: {repo_id}") from error


def tidy(audio, sr):
    """Toglie il silenzio ai bordi, porta il picco a -3 dB e aggiunge una dissolvenza brevissima."""
    audio = np.asarray(audio, dtype=np.float32)
    level = np.abs(audio)
    loud = np.where(level > max(0.012, level.max() * 0.03))[0]
    if len(loud):
        a, b = max(0, loud[0] - int(0.05 * sr)), min(len(audio), loud[-1] + int(0.18 * sr))
        audio = audio[a:b]
    peak = np.abs(audio).max()
    if peak > 0:
        audio = audio / peak * 0.707
    fade = min(len(audio) // 2, int(0.01 * sr))
    if fade:
        audio[:fade] *= np.linspace(0, 1, fade)
        audio[-fade:] *= np.linspace(1, 0, fade)
    return audio


def option(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    lines = json.load(open(sys.argv[1], encoding="utf-8"))
    refs, out = sys.argv[2], sys.argv[3]
    only = set(filter(None, (option("--only") or "").split(",")))
    kinds = set(filter(None, (option("--kinds") or "").split(",")))
    limit, tries = int(option("--limit", "100000")), int(option("--tries", "3"))
    minimum = float(option("--min-match", "0.9"))
    if tries < 1 or limit < 0 or not 0 <= minimum <= 1:
        raise ValueError("Tentativi, limite o soglia non validi")
    previous = {}
    manifest = Path(out) / "voices.jsonl"
    if manifest.exists():
        for row in manifest.read_text(encoding="utf-8").splitlines():
            record = json.loads(row)
            previous[(record["speaker"], record["key"])] = record
    retry = "--retry-low-quality" in sys.argv
    todo = [l for l in lines if (not only or l["speaker"] in only) and (not kinds or l.get("kind") in kinds)
            and (not os.path.exists(os.path.join(out, l["speaker"], l["key"] + ".wav"))
                 or (retry and not accepted(previous.get((l["speaker"], l["key"]), {"text": l["text"]}), minimum)))][:limit]
    print(f"{len(todo)} battute da doppiare su {len(lines)}", flush=True)
    if not todo:
        return
    import librosa
    import torch
    from faster_whisper import WhisperModel
    from qwen_tts import Qwen3TTSModel
    device = "cuda" if torch.cuda.is_available() else "mps" if torch.backends.mps.is_available() else "cpu"
    dtype = torch.bfloat16 if device == "cuda" else torch.float32
    local_model = option("--model-path") or model_path("Qwen/Qwen3-TTS-12Hz-1.7B-Base")
    model = Qwen3TTSModel.from_pretrained(local_model, device_map=device, dtype=dtype, attn_implementation="sdpa")
    asr = WhisperModel("small", device="cpu", compute_type="int8", local_files_only=True)
    prompts, started, audio_seconds = {}, time.time(), 0.0
    for n, line in enumerate(todo, 1):
        name = line["speaker"]
        if name not in prompts:
            meta = json.load(open(os.path.join(refs, name, "ref.json"), encoding="utf-8"))
            prompts[name] = model.create_voice_clone_prompt(ref_audio=os.path.join(refs, name, "ref.wav"), ref_text=meta["testo"])
        say = spoken(line["text"])
        # Tetto di token (12 al secondo): evita che il modello resti in ciclo senza chiudere la frase.
        cap = int(12 * max(4.0, len(say.split()) * 0.55 * 1.9))
        best = (-1.0, None, 0, "")
        seed_offset = retry_seed_offset(previous.get((name, line["key"])), tries, minimum)
        attempts = 0
        for k in range(tries):
            attempts += 1
            torch.manual_seed((int(line["key"][:8], 16) + 31 * (seed_offset + k)) % (2 ** 32))
            wavs, sr = model.generate_voice_clone(text=say, language="Italian", voice_clone_prompt=prompts[name], max_new_tokens=cap)
            audio = np.asarray(wavs[0], dtype=np.float32)
            a16 = librosa.resample(audio, orig_sr=sr, target_sr=16000).astype("float32")
            heard = " ".join(s.text for s in asr.transcribe(a16, language="it")[0])
            score, addresses_match = quality(line["text"], heard)
            if not addresses_match:
                score = min(score, minimum - 0.001)
            if score > best[0]:
                best = (score, audio, sr, heard.strip())
            if score >= minimum:
                break
        clean = tidy(best[1], best[2])
        folder = os.path.join(out, name)
        os.makedirs(folder, exist_ok=True)
        destination = Path(folder) / (line["key"] + ".wav")
        passed = best[0] >= minimum
        if not passed:
            review = Path(refs) / name / "review"
            review.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                destination.replace(review / (line["key"] + ".previous.wav"))
            destination = review / (line["key"] + ".wav")
        temporary = destination.with_suffix(".tmp.wav")
        sf.write(str(temporary), clean, best[2], subtype="PCM_16")
        os.replace(temporary, destination)
        audio_seconds += len(clean) / best[2]
        with open(os.path.join(out, "voices.jsonl"), "a", encoding="utf-8") as log:
            log.write(json.dumps({"speaker": name, "key": line["key"], "text": line["text"], "spoken": say, "heard": best[3],
                                  "match": round(best[0], 3), "accepted": passed, "seed_offset": seed_offset,
                                  "attempts": attempts, "seconds": round(len(clean) / best[2], 2)}, ensure_ascii=False) + "\n")
        elapsed = time.time() - started
        print(f"[{n}/{len(todo)}] {'OK' if passed else 'DA RIVEDERE'} {name} {line['key']} {len(clean) / best[2]:.1f}s corrispondenza {best[0]:.2f} · trascorsi {elapsed / 60:.1f} min "
              f"· stima residua {elapsed / n * (len(todo) - n) / 60:.0f} min", flush=True)
    print(f"fatto: {audio_seconds / 60:.1f} minuti di voce in {(time.time() - started) / 60:.1f} minuti", flush=True)


if __name__ == "__main__":
    main()
