"""Suoni originali di CLARA, sintetizzati da zero (nessun campione di terzi, nessun download).

    python make_sfx.py <cartella Content/Audio>

Scrive Ambience/*.wav (anelli senza giunta) e Sfx/*.wav, PCM 16 bit. Il seme e' fisso: rieseguendo si ottengono
gli stessi file. Il gioco li carica a runtime da disco (ClaraAudio.cpp), senza importarli come asset Unreal.
"""
import os
import sys

import numpy as np
import soundfile as sf

SR = 32000
rng = np.random.default_rng(20261114)


def t_axis(seconds):
    return np.arange(int(seconds * SR)) / SR


def shaped(x, gain_fn):
    """Filtro a fase zero definito dalla sola curva di guadagno (funzione della frequenza)."""
    n = len(x)
    f = np.fft.rfftfreq(n, 1 / SR)
    return np.fft.irfft(np.fft.rfft(x) * gain_fn(f), n)


def band(lo, hi, order=3):
    def g(f):
        f = np.maximum(f, 1e-6)
        return 1 / np.sqrt(1 + (lo / f) ** (2 * order)) / np.sqrt(1 + (f / hi) ** (2 * order))
    return g


def loop_noise(seconds, gain_fn):
    """Rumore periodico per costruzione: l'ultimo campione si salda al primo senza scatti."""
    n = int(seconds * SR)
    f = np.fft.rfftfreq(n, 1 / SR)
    spec = gain_fn(f) * np.exp(1j * rng.uniform(0, 2 * np.pi, len(f)))
    spec[0] = 0
    x = np.fft.irfft(spec, n)
    return x / np.max(np.abs(x))


def decay(seconds, tau, attack=0.0015):
    t = t_axis(seconds)
    return np.minimum(t / attack, 1.0) * np.exp(-t / tau)


def tone(freq, seconds, tau, harmonics=((1, 1.0),)):
    t = t_axis(seconds)
    y = sum(a * np.sin(2 * np.pi * freq * h * t) for h, a in harmonics)
    return y * decay(seconds, tau)


def burst(seconds, tau, lo, hi):
    return shaped(rng.standard_normal(int(seconds * SR)), band(lo, hi)) * decay(seconds, tau, 0.0006)


def place(canvas, sound, at, gain=1.0):
    i = int(at * SR)
    j = min(len(canvas), i + len(sound))
    if j > i:
        canvas[i:j] += sound[: j - i] * gain


def finish(x, peak):
    x = np.asarray(x, dtype=np.float64)
    fade = int(0.004 * SR)
    if x.ndim == 1 and len(x) > 2 * fade:
        x[:fade] *= np.linspace(0, 1, fade)
        x[-fade:] *= np.linspace(1, 0, fade)
    m = np.max(np.abs(x))
    return (x / m * peak) if m > 0 else x


def write(root, name, x, peak=0.5, loop=False):
    path = os.path.join(root, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    y = x / np.max(np.abs(x)) * peak if loop else finish(x, peak)
    sf.write(path, y.astype(np.float32), SR, subtype="PCM_16")
    print(f"{name}: {len(y) / SR:.2f} s", flush=True)


def stereo(make, width=0.45):
    """Due esecuzioni indipendenti mescolate: ambiente largo ma stabile al centro."""
    a, b = make(), make()
    mid = (a + b) / 2
    return np.stack([mid + width * (a - mid), mid + width * (b - mid)], axis=1)


# ---------------------------------------------------------------- ambienti
def amb_office(seconds=24.0):
    def one():
        air = loop_noise(seconds, lambda f: 1 / (1 + (f / 160.0) ** 2) ** 0.95)
        hiss = loop_noise(seconds, band(500, 2600, 2)) * 0.07
        t = t_axis(seconds)
        swell = 1 + 0.07 * np.sin(2 * np.pi * 3 * t / seconds) + 0.04 * np.sin(2 * np.pi * 7 * t / seconds + 1.3)
        hum = 0.012 * np.sin(2 * np.pi * 100 * t) + 0.005 * np.sin(2 * np.pi * 200 * t)
        return (air + hiss) * swell + hum
    return stereo(one)


def amb_server(seconds=16.0):
    def one():
        t = t_axis(seconds)
        wind = loop_noise(seconds, lambda f: band(140, 5200, 2)(f) / (1 + (f / 1100.0) ** 1.1))
        fans = np.zeros_like(t)
        for base, level in ((146.0, 0.20), (163.5, 0.15), (218.0, 0.08)):
            for h, a in ((1, 1.0), (2, 0.5), (3, 0.22), (4, 0.1)):
                fans += level * a * np.sin(2 * np.pi * base * h * t + rng.uniform(0, 6.28))
        whine = 0.015 * np.sin(2 * np.pi * 3170 * t) + 0.01 * np.sin(2 * np.pi * 4420 * t)
        wobble = 1 + 0.05 * np.sin(2 * np.pi * 5 * t / seconds)
        return wind * wobble + fans * 0.55 + whine
    return stereo(one, 0.35)


def key_click(strength=1.0):
    y = np.zeros(int(0.11 * SR))
    place(y, burst(0.03, 0.006, 1800, 6500), 0, 0.9 * strength)
    place(y, tone(rng.uniform(170, 260), 0.05, 0.016), 0.001, 0.8 * strength)
    place(y, burst(0.02, 0.004, 2500, 7500), rng.uniform(0.055, 0.085), 0.35 * strength)
    return y


def amb_keys(seconds=30.0):
    y = np.zeros(int(seconds * SR))
    at = 0.6
    while at < seconds - 3.5:
        for _ in range(int(rng.integers(4, 19))):
            if at > seconds - 0.6:
                break
            place(y, key_click(rng.uniform(0.45, 1.0)), at)
            at += rng.uniform(0.09, 0.24)
        if rng.random() < 0.35:
            at += rng.uniform(0.3, 0.8)
            place(y, burst(0.02, 0.004, 2200, 6000), at, 0.8)
            place(y, burst(0.02, 0.004, 2600, 7000), at + 0.07, 0.6)
        at += rng.uniform(1.2, 3.4)
    room = rng.standard_normal(int(0.16 * SR)) * np.exp(-t_axis(0.16) / 0.035)
    # Convoluzione circolare via FFT: rapida, e la coda del riverbero rientra dall'inizio dell'anello.
    wet = np.fft.irfft(np.fft.rfft(y) * np.fft.rfft(room, len(y)), len(y))
    out = shaped(y + 0.22 * wet / np.max(np.abs(wet)), band(120, 4200, 2))
    return out


# ---------------------------------------------------------------- effetti
def sfx_click():
    t = t_axis(0.05)
    chirp = np.sin(2 * np.pi * (2100 * t - 9000 * t * t)) * decay(0.05, 0.009)
    return chirp + 0.25 * burst(0.05, 0.004, 2500, 8000)


def sfx_two(a, b):
    y = np.zeros(int(0.22 * SR))
    place(y, tone(a, 0.12, 0.035, ((1, 1.0), (2, 0.18))), 0)
    place(y, tone(b, 0.14, 0.045, ((1, 1.0), (2, 0.18))), 0.07)
    return y


def sfx_pickup():
    n = int(0.2 * SR)
    t = t_axis(0.2)
    sw = shaped(rng.standard_normal(n), band(600, 3600, 2)) * np.hanning(n) ** 1.5
    return sw + 0.5 * tone(120, 0.2, 0.03) * (t < 0.12)


def sfx_drop():
    y = np.zeros(int(0.3 * SR))
    place(y, tone(96, 0.28, 0.06, ((1, 1.0), (2, 0.3))), 0)
    place(y, burst(0.12, 0.022, 180, 1400), 0, 0.7)
    place(y, burst(0.05, 0.008, 900, 3800), 0.012, 0.25)
    return y


def sfx_plug():
    y = np.zeros(int(0.26 * SR))
    place(y, burst(0.03, 0.003, 2200, 8500), 0, 0.55)
    place(y, burst(0.04, 0.004, 1800, 9000), 0.034, 1.0)
    place(y, tone(3350, 0.08, 0.011), 0.034, 0.35)
    place(y, tone(150, 0.14, 0.03), 0.036, 0.75)
    return y


def sfx_unplug():
    y = np.zeros(int(0.3 * SR))
    place(y, burst(0.03, 0.004, 2000, 7000), 0, 0.6)
    n = int(0.09 * SR)
    place(y, shaped(rng.standard_normal(n), band(900, 4200, 2)) * np.hanning(n), 0.03, 0.35)
    place(y, tone(210, 0.1, 0.02), 0.12, 0.5)
    place(y, burst(0.03, 0.005, 1500, 6000), 0.12, 0.45)
    return y


def sfx_error():
    y = np.zeros(int(0.42 * SR))
    soft = ((1, 1.0), (3, 0.16), (5, 0.05))
    place(y, tone(196.0, 0.2, 0.07, soft), 0)
    place(y, tone(164.8, 0.26, 0.09, soft), 0.14)
    return shaped(y, band(60, 2200, 2))


def bell(freq, seconds, tau):
    return tone(freq, seconds, tau, ((1, 1.0), (2, 0.28), (2.76, 0.09), (4.1, 0.04)))


def sfx_success():
    y = np.zeros(int(1.5 * SR))
    for k, f in enumerate((659.25, 880.0, 1108.73)):
        place(y, bell(f, 1.3, 0.42), 0.075 * k, 1.0 - 0.12 * k)
    return y


def sfx_hint():
    y = np.zeros(int(0.9 * SR))
    place(y, bell(1046.5, 0.85, 0.3), 0)
    place(y, bell(1568.0, 0.7, 0.22), 0.05, 0.35)
    return y


def sfx_step():
    y = np.zeros(int(0.2 * SR))
    place(y, tone(rng.uniform(78, 118), 0.16, rng.uniform(0.028, 0.04), ((1, 1.0), (2, 0.35))), 0)
    place(y, burst(0.09, rng.uniform(0.016, 0.026), 260, rng.uniform(1300, 2100)), 0.004, rng.uniform(0.5, 0.8))
    return y


def sfx_page():
    n = int(0.26 * SR)
    x = rng.standard_normal(n)
    out = np.zeros(n)
    for k in range(8):
        seg = slice(k * n // 8, (k + 1) * n // 8)
        centre = 1400 + 480 * k
        out[seg] = shaped(x, band(centre * 0.6, centre * 1.7, 2))[seg]
    flutter = 0.55 + 0.45 * np.abs(shaped(rng.standard_normal(n), band(18, 60, 2)))
    env = np.hanning(n) ** 0.7
    return out * env * flutter / np.max(flutter)


def sfx_zip():
    y = np.zeros(int(0.32 * SR))
    at, gap = 0.0, 0.012
    for _ in range(60):
        if at >= 0.27:
            break
        place(y, burst(0.012, 0.0022, 2400, 8000), at, 0.5 + at * 1.6)
        gap = max(gap * 0.955, 0.0035)
        at += gap
    return y


def sfx_laptop_open():
    y = np.zeros(int(0.95 * SR))
    place(y, burst(0.06, 0.012, 500, 2600), 0, 0.4)
    place(y, bell(523.25, 0.7, 0.26), 0.1, 0.7)
    place(y, bell(783.99, 0.75, 0.3), 0.21, 0.6)
    return y


def sfx_laptop_close():
    y = np.zeros(int(0.28 * SR))
    place(y, burst(0.1, 0.018, 200, 1500), 0, 0.9)
    place(y, tone(138, 0.2, 0.04), 0.004, 0.8)
    place(y, burst(0.03, 0.004, 1800, 6000), 0.02, 0.3)
    return y


def sfx_enter():
    y = key_click(1.0)
    place(y, tone(132, 0.08, 0.028), 0.002, 0.9)
    return y


def sfx_alert():
    y = np.zeros(int(2.2 * SR))
    for k in range(4):
        f = 880.0 if k % 2 == 0 else 659.25
        n = int(0.42 * SR)
        t = np.arange(n) / SR
        note = (np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * 2 * f * t)) * np.hanning(n) ** 0.35
        place(y, note, 0.5 * k)
    return y


def main():
    root = sys.argv[1]
    write(root, "Ambience/office.wav", amb_office(), 0.30, loop=True)
    write(root, "Ambience/server.wav", amb_server(), 0.42, loop=True)
    write(root, "Ambience/keys.wav", amb_keys(), 0.34, loop=True)
    write(root, "Sfx/click.wav", sfx_click(), 0.42)
    write(root, "Sfx/open.wav", sfx_two(587.33, 880.0), 0.36)
    write(root, "Sfx/close.wav", sfx_two(880.0, 587.33), 0.32)
    write(root, "Sfx/pickup.wav", sfx_pickup(), 0.5)
    write(root, "Sfx/drop.wav", sfx_drop(), 0.6)
    write(root, "Sfx/plug.wav", sfx_plug(), 0.72)
    write(root, "Sfx/unplug.wav", sfx_unplug(), 0.6)
    write(root, "Sfx/error.wav", sfx_error(), 0.5)
    write(root, "Sfx/success.wav", sfx_success(), 0.5)
    write(root, "Sfx/hint.wav", sfx_hint(), 0.4)
    write(root, "Sfx/page.wav", sfx_page(), 0.45)
    write(root, "Sfx/zip.wav", sfx_zip(), 0.42)
    write(root, "Sfx/laptop_open.wav", sfx_laptop_open(), 0.42)
    write(root, "Sfx/laptop_close.wav", sfx_laptop_close(), 0.55)
    write(root, "Sfx/enter.wav", sfx_enter(), 0.55)
    write(root, "Sfx/alert.wav", sfx_alert(), 0.5)
    for k in range(1, 4):
        write(root, f"Sfx/key{k}.wav", key_click(1.0), 0.45)
    for k in range(1, 5):
        write(root, f"Sfx/step{k}.wav", sfx_step(), 0.5)


if __name__ == "__main__":
    main()
