"""Synthesise the background playlist: assets/music_01.wav .. music_03.wav.

48 kHz mono 16-bit (the engine loader's format). Each track is a calm loop
of soft pad chords, a round bass and a sparse pentatonic melody with a light
echo, faded in and out so tracks chain cleanly in the playlist. Seeded and
deterministic. Needs numpy.
Regenerate with: python3 tools/gen_music.py [outdir]
"""
import os
import sys
import wave

import numpy as np

RATE = 48000

TRACKS = [
    # name, root midi, bpm, chords (semitones above root), scale (pentatonic degrees), seed
    ("music_01.wav", 60, 76, [[0, 4, 7, 11], [-3, 0, 4, 7], [-7, -3, 0, 4], [-5, -1, 2, 9]],
     [0, 2, 4, 7, 9], 11),
    ("music_02.wav", 62, 84, [[0, 3, 7, 10], [5, 9, 12, 14], [-4, 0, 3, 7], [-2, 2, 5, 9]],
     [0, 3, 5, 7, 10], 23),
    ("music_03.wav", 57, 66, [[0, 3, 7, 10], [-4, 0, 3, 7], [3, 7, 10, 14], [-5, -2, 2, 7]],
     [0, 3, 5, 7, 10], 37),
]
BARS_PER_CHORD = 2
CYCLES = 2


def hz(midi):
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def adsr(n, attack, release):
    env = np.ones(n)
    a = min(n, int(attack * RATE))
    r = min(n - a, int(release * RATE))
    if a > 0:
        env[:a] = np.linspace(0.0, 1.0, a)
    if r > 0:
        env[n - r:] = np.linspace(1.0, 0.0, r)
    return env


def pad(midis, dur):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    out = np.zeros(n)
    for m in midis:
        f = hz(m)
        for detune in (-0.12, 0.12):
            fd = f * 2.0 ** (detune / 12.0)
            out += np.sin(2 * np.pi * fd * t) + 0.25 * np.sin(4 * np.pi * fd * t)
    return out * adsr(n, 1.2, 1.4) / (len(midis) * 2.5)


def pluck(midi, dur):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    f = hz(midi)
    tone = np.sin(2 * np.pi * f * t) + 0.35 * np.sin(4 * np.pi * f * t) + 0.1 * np.sin(6 * np.pi * f * t)
    return tone * np.exp(-t * 3.2) * adsr(n, 0.01, 0.1)


def bass(midi, dur):
    n = int(dur * RATE)
    t = np.arange(n) / RATE
    return np.sin(2 * np.pi * hz(midi) * t) * np.exp(-t * 1.6) * adsr(n, 0.02, 0.2)


def place(buf, clip, start):
    s = int(start * RATE)
    e = min(len(buf), s + len(clip))
    if s < len(buf):
        buf[s:e] += clip[:e - s]


def compose(root, bpm, chords, scale, seed):
    rng = np.random.default_rng(seed)
    beat = 60.0 / bpm
    chord_len = 4 * BARS_PER_CHORD * beat
    total = chord_len * len(chords) * CYCLES
    out = np.zeros(int((total + 3.0) * RATE))
    prev = 2
    for cycle in range(CYCLES):
        for ci, chord in enumerate(chords):
            t0 = (cycle * len(chords) + ci) * chord_len
            place(out, 0.55 * pad([root - 12 + c for c in chord], chord_len + 1.0), t0)
            for b in range(BARS_PER_CHORD * 4):
                if b % 2 == 0:
                    place(out, 0.45 * bass(root - 24 + chord[0], beat * 1.8), t0 + b * beat)
            steps = BARS_PER_CHORD * 8
            for s in range(steps):
                density = 0.18 if cycle == 0 else 0.32
                if s % 2 == 1 or rng.random() > density:
                    continue
                prev = int(np.clip(prev + rng.integers(-2, 3), 0, len(scale) * 2 - 1))
                octave, degree = divmod(prev, len(scale))
                note = root + 12 * octave + scale[degree]
                place(out, 0.28 * pluck(note, beat * 2.5), t0 + s * beat * 0.5)
    delay = int(beat * 0.75 * RATE)
    echo = np.zeros_like(out)
    echo[delay:] = out[:-delay] * 0.28
    echo[2 * delay:] += out[:-2 * delay] * 0.1
    out += echo
    out *= adsr(len(out), 2.5, 3.5)
    peak = np.max(np.abs(out))
    return out * (0.6 / peak if peak > 0 else 1.0)


def write_wav(path, samples):
    pcm = (np.clip(samples, -1.0, 1.0) * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "assets")
    for name, root, bpm, chords, scale, seed in TRACKS:
        samples = compose(root, bpm, chords, scale, seed)
        path = os.path.join(out_dir, name)
        write_wav(path, samples)
        print(f"wrote {os.path.normpath(path)}: {len(samples) / RATE:.1f}s")


if __name__ == "__main__":
    main()
