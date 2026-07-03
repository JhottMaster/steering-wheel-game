from __future__ import annotations

import math
import struct
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOUNDS = ROOT / "assets" / "sounds"
SAMPLE_RATE = 44100


def clamp_sample(value: float) -> int:
    value = max(-1.0, min(1.0, value))
    return int(value * 32767)


def write_wav(path: Path, samples: list[float]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(b"".join(struct.pack("<h", clamp_sample(sample)) for sample in samples))


def envelope(index: int, total: int, attack: float = 0.02, release: float = 0.08) -> float:
    t = index / SAMPLE_RATE
    duration = total / SAMPLE_RATE
    attack_gain = min(1.0, t / attack) if attack > 0 else 1.0
    release_gain = min(1.0, max(0.0, (duration - t) / release)) if release > 0 else 1.0
    return min(attack_gain, release_gain)


def make_coin():
    duration = 0.42
    total = int(SAMPLE_RATE * duration)
    samples = []
    notes = [880.0, 1174.66, 1567.98]
    for i in range(total):
        t = i / SAMPLE_RATE
        note_index = min(len(notes) - 1, int(t / 0.12))
        tone = math.sin(2.0 * math.pi * notes[note_index] * t)
        sparkle = 0.45 * math.sin(2.0 * math.pi * (notes[note_index] * 2.0) * t)
        samples.append((tone + sparkle) * 0.28 * envelope(i, total, 0.004, 0.16))
    write_wav(SOUNDS / "coin_chime.wav", samples)


def make_engine():
    duration = 1.25
    total = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(total):
        t = i / SAMPLE_RATE
        wobble = 1.0 + 0.035 * math.sin(2.0 * math.pi * 5.0 * t)
        base = 72.0 * wobble
        tone = 0.55 * math.sin(2.0 * math.pi * base * t)
        tone += 0.25 * math.sin(2.0 * math.pi * base * 2.0 * t)
        tone += 0.12 * math.sin(2.0 * math.pi * base * 3.0 * t)
        putter = 0.12 * math.sin(2.0 * math.pi * 18.0 * t)
        samples.append((tone + putter) * 0.28 * envelope(i, total, 0.03, 0.03))
    write_wav(SOUNDS / "toy_engine_loop.wav", samples)


def make_background():
    duration = 8.0
    total = int(SAMPLE_RATE * duration)
    samples = []
    melody = [261.63, 329.63, 392.00, 523.25, 392.00, 329.63, 293.66, 349.23]
    bass = [130.81, 146.83, 164.81, 196.00]
    for i in range(total):
        t = i / SAMPLE_RATE
        beat = int(t * 2.0)
        melody_freq = melody[beat % len(melody)]
        bass_freq = bass[(beat // 2) % len(bass)]
        local = (t * 2.0) % 1.0
        pluck_env = math.exp(-local * 4.0)
        melody_tone = math.sin(2.0 * math.pi * melody_freq * t) * pluck_env
        chime = math.sin(2.0 * math.pi * melody_freq * 2.0 * t) * pluck_env * 0.35
        bass_tone = math.sin(2.0 * math.pi * bass_freq * t) * 0.35
        samples.append((melody_tone + chime + bass_tone) * 0.16 * envelope(i, total, 0.08, 0.08))
    write_wav(SOUNDS / "carpet_cruise_loop.wav", samples)


def main():
    make_coin()
    make_engine()
    make_background()
    print(f"Generated sounds in {SOUNDS}")


if __name__ == "__main__":
    main()
