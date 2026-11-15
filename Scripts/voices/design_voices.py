"""Progetta le voci dei personaggi di CLARA con Qwen3-TTS 1.7B VoiceDesign (voci nuove, di nessuna persona reale).

    python design_voices.py <cartella_uscita> [candidati_per_voce] [Personaggio ...]

Per ogni personaggio scrive <uscita>/<Nome>/candidato-N.wav, riascolta ogni candidato con Whisper,
misura l'altezza della voce e copia il migliore in <uscita>/<Nome>/ref.wav con ref.json (testo e descrizione).
generate_voices.py usa ref.wav come riferimento per dire tutte le battute con la stessa voce.
Per cambiare voce a mano basta copiare un altro candidato su ref.wav.
"""
import difflib
import json
import os
import re
import shutil
import sys
import time

import numpy as np
import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel

# Le descrizioni sono in inglese: il modello segue meglio sesso, eta' e registro della voce. Il testo detto resta in italiano.
STUDIO = " Clear standard Italian diction, no strong regional accent. Close-microphone studio recording, dry, no reverb, no music."
VOCI = {
    "Luca": {
        "descrizione": "A man of about thirty-five with a deep, resonant bass-baritone voice, low pitch, relaxed chest voice. "
                       "He is a network technician mentoring a new colleague: friendly, calm and reassuring, unhurried and natural." + STUDIO,
        "testo": "Ciao! Luca, piacere. Il portatile laggiù è il tuo: prendilo, ti servirà. Oggi montiamo la rete da zero, un passo alla volta.",
        "f0": (85, 150),
    },
    "Alessia": {
        "descrizione": "A woman of about thirty with a bright, clear voice. She runs the front desk and onboarding at an IT company: "
                       "cordial, welcoming and professional, never syrupy, calm and assured pace." + STUDIO,
        "testo": "Eccoti qui. Puntualissimo! Benvenuto in CLARA: reti costruite e protette, questo facciamo. Oggi ti mettiamo subito al lavoro con la squadra.",
        "f0": (190, 300),
    },
    "Gaia": {
        "descrizione": "A mature woman of about forty-five with a low, steady alto voice. She is the technical head of the company: "
                       "dry, firm and authoritative, short sentences, measured pace, no theatrical emphasis." + STUDIO,
        "testo": "Fermi tutti. Il piano due è senza rete da dieci minuti. Quando pensi di aver finito, premi TEST. Controllo io.",
        "f0": (160, 225),
    },
    "Jonny": {
        "descrizione": "A young man of about twenty-five with a light, lively tenor voice. He is a security analyst: "
                       "quick, curious and slightly ironic, energetic but not shouting." + STUDIO,
        "testo": "Io sono Jonny. Quando hai un ping che risponde, chiamami: ti faccio vedere quante cose racconta quel piccolo messaggio.",
        "f0": (105, 175),
    },
}


def model_path(repo_id):
    """Usa la copia gia' scaricata nella cache di Hugging Face, senza chiamate di rete; altrimenti il nome del repository."""
    try:
        from huggingface_hub import snapshot_download
        return snapshot_download(repo_id, local_files_only=True)
    except Exception:
        return repo_id


def words(s):
    return re.sub(r"[^a-zà-ù0-9 ]", " ", s.lower()).split()


def main():
    out = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    names = sys.argv[3:] or list(VOCI)
    import librosa
    from faster_whisper import WhisperModel
    device = "cuda" if torch.cuda.is_available() else "mps" if torch.backends.mps.is_available() else "cpu"
    dtype = torch.bfloat16 if device == "cuda" else torch.float32
    model = Qwen3TTSModel.from_pretrained(model_path("Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign"), device_map=device, dtype=dtype)
    asr = WhisperModel("small", device="cpu", compute_type="int8")
    for name in names:
        voce = VOCI[name]
        folder = os.path.join(out, name)
        os.makedirs(folder, exist_ok=True)
        report = []
        for k in range(1, n + 1):
            torch.manual_seed(4100 + 17 * k + len(name) + int(os.environ.get("CLARA_VOICE_SEED", "0")))
            t0 = time.time()
            limit = int(12 * max(6.0, len(voce["testo"].split()) * 0.55 * 1.8))
            wavs, sr = model.generate_voice_design(text=voce["testo"], instruct=voce["descrizione"], language="Italian", max_new_tokens=limit)
            audio = np.asarray(wavs[0], dtype="float32")
            path = os.path.join(folder, f"candidato-{k}.wav")
            sf.write(path, audio, sr, subtype="PCM_16")
            a16 = librosa.resample(audio, orig_sr=sr, target_sr=16000).astype("float32")
            heard = " ".join(s.text for s in asr.transcribe(a16, language="it")[0])
            score = difflib.SequenceMatcher(None, words(voce["testo"]), words(heard)).ratio()
            # Altezza della voce: mediana sui tratti piu' sonori. Serve a distinguere un timbro maschile da uno femminile
            # quando nessuno puo' ascoltare i candidati; la scelta finale resta di chi li ascolta.
            f0 = librosa.yin(a16, fmin=65, fmax=350, sr=16000, frame_length=1024)
            rms = librosa.feature.rms(y=a16, frame_length=1024, hop_length=256)[0]
            m = min(len(f0), len(rms))
            loud = f0[:m][rms[:m] > np.percentile(rms[:m], 55)]
            pitch = float(np.median(loud)) if len(loud) else 0.0
            seconds = len(audio) / sr
            lo, hi = voce["f0"]
            off = 0.0 if lo <= pitch <= hi else min(abs(pitch - lo), abs(pitch - hi))
            fit = max(0.0, 1.0 - off / 40.0)
            rate = len(voce["testo"]) / max(seconds, 0.1)
            pace = 1.0 - min(abs(rate - 15.0) / 15.0, 1.0)
            total = score * 0.45 + fit * 0.40 + pace * 0.15
            report.append({"file": os.path.basename(path), "testo_sentito": heard.strip(), "corrispondenza": round(score, 3),
                           "altezza_hz": round(pitch, 1), "secondi": round(seconds, 2), "caratteri_al_secondo": round(rate, 1),
                           "punteggio": round(total, 3), "secondi_di_calcolo": round(time.time() - t0, 1)})
            print(f"{name} candidato {k}: {seconds:.1f} s audio in {time.time() - t0:.0f} s · corrispondenza {score:.2f} · {pitch:.0f} Hz · punteggio {total:.2f}", flush=True)
        best = max(report, key=lambda r: r["punteggio"])
        shutil.copyfile(os.path.join(folder, best["file"]), os.path.join(folder, "ref.wav"))
        json.dump({"descrizione": voce["descrizione"], "testo": voce["testo"], "scelto": best["file"], "candidati": report},
                  open(os.path.join(folder, "ref.json"), "w"), ensure_ascii=False, indent=2)
        print(f"{name}: scelto {best['file']}", flush=True)


if __name__ == "__main__":
    main()
