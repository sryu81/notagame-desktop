"""Synthesise 48 kHz mono 16-bit SFX and a second music loop into assets/.

select.wav move.wav hit.wav miss.wav turn.wav music2.wav
Stdlib only, deterministic. Regenerate with: python3 tools/gen_sfx.py [outdir]
"""
import math
import os
import random
import struct
import sys

RATE = 48000


def envelope(i, n, attack=0.005, release=0.3):
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    rel_start = n * (1.0 - release)
    r = 1.0 if i < rel_start else max(0.0, (n - i) / (n - rel_start))
    return a * r


def tone(freq0, freq1, dur, amp=0.5, shape="sine", release=0.4):
    n = int(RATE * dur)
    out = []
    phase = 0.0
    for i in range(n):
        f = freq0 + (freq1 - freq0) * i / n
        phase += 2.0 * math.pi * f / RATE
        if shape == "sine":
            v = math.sin(phase)
        elif shape == "square":
            v = 1.0 if math.sin(phase) >= 0 else -1.0
            v *= 0.5
        else:
            v = 2.0 * abs((phase / math.pi) % 2.0 - 1.0) - 1.0
        out.append(v * amp * envelope(i, n, release=release))
    return out


def noise(dur, amp=0.4, seed=1):
    rng = random.Random(seed)
    n = int(RATE * dur)
    return [rng.uniform(-1.0, 1.0) * amp * envelope(i, n, release=0.8) for i in range(n)]


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for t in tracks:
        for i, v in enumerate(t):
            out[i] += v
    return out


def concat(*tracks):
    out = []
    for t in tracks:
        out.extend(t)
    return out


def write_wav(path, samples):
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = min(1.0, 0.9 / peak)
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples)
    header = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16)
    header += b"data" + struct.pack("<I", len(data))
    with open(path, "wb") as f:
        f.write(header + data)
    print(f"wrote {len(samples) / RATE:.2f}s -> {os.path.normpath(path)}")


def music_loop(bars=2, bpm=120):
    beat = 60.0 / bpm
    notes = [220.0, 261.63, 329.63, 392.0, 329.63, 261.63, 196.0, 246.94]
    melody = []
    for b in range(bars):
        for k, f in enumerate(notes):
            melody.extend(tone(f, f, beat / 2, amp=0.25, shape="tri", release=0.5))
    n = len(melody)
    bass = []
    while len(bass) < n:
        bass.extend(tone(110.0, 110.0, beat * 2, amp=0.2, shape="sine", release=0.2))
    return mix(melody, bass[:n])


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "assets")
    os.makedirs(out, exist_ok=True)
    sounds = {
        "select.wav": tone(880.0, 1320.0, 0.08, shape="square", release=0.5),
        "move.wav": tone(520.0, 360.0, 0.15, shape="tri"),
        "hit.wav": mix(noise(0.18, amp=0.5), tone(160.0, 60.0, 0.2, amp=0.6)),
        "miss.wav": tone(600.0, 280.0, 0.25, amp=0.4, shape="sine", release=0.6),
        "turn.wav": concat(tone(660.0, 660.0, 0.12, amp=0.4), tone(990.0, 990.0, 0.22, amp=0.4)),
        "music2.wav": music_loop(),
    }
    for name, samples in sounds.items():
        write_wav(os.path.join(out, name), samples)


if __name__ == "__main__":
    main()
