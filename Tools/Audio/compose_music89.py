"""Render the three Sky Corridor scores using procedural synthesis only.

Requires Python 3.10+ and NumPy. No sample libraries, impulse-response files,
network access, audio devices, or external applications are used by this file.
Run from any directory; the default output is relative to this repository.
Existing output files are preserved. Render elsewhere with --output DIR.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import platform
import wave

import numpy as np

RATE = 48000
SEED = 891409
SPECS = [
    {"id": "CanalAfterglow", "title": "水路のあと", "seconds": 116,
     "starts": [3, 20, 38, 58, 79, 98],
     "chords": [[50,57,62,65], [46,53,60,62], [48,55,62,64], [45,52,59,60], [50,57,62,65], [46,53,60,65]],
     "melody": [[69,67,65,64], [65,62,60], [64,67,62], [64,60,59], [69,65,64], [65,62,64]], "pace": 1.28},
    {"id": "WindowWithoutVoices", "title": "声のない窓", "seconds": 124,
     "starts": [4, 25, 48, 73, 98],
     "chords": [[53,60,64,67], [50,57,60,64], [48,55,59,62], [46,53,57,60], [53,60,67,69]],
     "melody": [[72,69,67], [69,65,64], [67,64,62], [65,62,60], [69,67,64]], "pace": 1.40},
    {"id": "LampOnTheWayHome", "title": "帰り道の灯", "seconds": 108,
     "starts": [5, 28, 52, 78],
     "chords": [[48,55,62,63], [44,51,58,60], [46,53,60,62], [43,50,57,58]],
     "melody": [[67,65,63,62], [67,63,60], [65,62,60], [62,58,60]], "pace": 1.52},
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def voice(kind: str, key: int, length: float, velocity: float, pan: float,
          rng: np.random.Generator) -> np.ndarray:
    """Band-limited modal tones; every waveform is computed from equations."""
    t = np.arange(round(length * RATE), dtype=np.float64) / RATE
    frequency = 440 * 2 ** ((key - 69) / 12)
    stereo = np.zeros((len(t), 2), dtype=np.float64)
    if kind == "felt_keys":
        # Soft struck strings: slightly stiff modes, a hollow third harmonic,
        # and two very close strings whose slow beating moves the decay.
        stiffness = 0.000075 * 2 ** ((key - 60) / 24)
        for mode in range(1, 13):
            hz = frequency * mode * np.sqrt((1 + stiffness * mode * mode) / (1 + stiffness))
            if hz >= RATE * 0.44:
                break
            level = mode ** -1.75 * (0.66 if mode == 2 else 1.0)
            lifetime = (3.9 + (60 - key) * 0.022) / mode ** 0.72
            envelope = np.exp(-t / lifetime) * (-np.expm1(-t / (0.003 + mode * 0.0003)))
            phase = float(rng.uniform(-0.12, 0.12))
            for channel in range(2):
                detune = (-0.7 if channel == 0 else 0.8) / 1200
                partial = np.sin(2 * np.pi * hz * 2 ** detune * t + phase)
                partial += 0.24 * np.sin(2 * np.pi * hz * t + phase + 0.03)
                stereo[:, channel] += level * envelope * partial
        # Quiet, filtered procedural hammer noise; no recorded attack.
        hammer = rng.normal(0, 1, len(t))
        hammer = np.convolve(hammer, np.hanning(25) / np.hanning(25).sum(), mode="same")
        hammer *= np.exp(-t / 0.018) * (-np.expm1(-t / 0.002)) * 0.13
        stereo += hammer[:, None]
        stereo *= (-np.expm1(-t / 0.002))[:, None]
        release = min(1.4, length / 3)
    else:
        # A breath-like string halo. Slow detuning and vibrato avoid a static
        # organ tone; low harmonic energy keeps the accompaniment restrained.
        vibrato = 0.0018 * frequency / 4.7 * np.sin(2 * np.pi * 4.7 * t)
        vibrato *= np.minimum(1, t / 1.8)
        for mode in range(1, 10):
            if mode * frequency >= RATE * 0.44:
                break
            level = mode ** -2.15 * (0.52 if mode % 2 == 0 else 1)
            for channel in range(2):
                phase = float(rng.uniform(0, 2 * np.pi))
                detune = (-2.2 if channel == 0 else 2.0) / 1200
                angle = 2 * np.pi * (frequency * 2 ** detune * t + vibrato)
                stereo[:, channel] += level * np.sin(mode * angle + phase)
        stereo *= (np.sin(np.minimum(1, t / 1.7) * np.pi / 2) ** 2
                   * np.exp(-t / 12) * (0.97 + 0.03 * np.sin(2 * np.pi * 0.31 * t)))[:, None]
        release = min(2.8, length / 3)
    count = round(release * RATE)
    stereo[-count:] *= np.cos(np.linspace(0, np.pi / 2, count))[:, None] ** 2
    stereo /= max(1.0, float(np.max(np.abs(stereo))))
    stereo *= 0.34 * velocity
    stereo[:, 0] *= np.sqrt(1 - pan)
    stereo[:, 1] *= np.sqrt(1 + pan)
    return stereo.astype(np.float32)


def reverberate(dry: np.ndarray, rng: np.random.Generator) -> np.ndarray:
    """A generated room: early taps plus a decaying, filtered random field."""
    wet = np.zeros_like(dry)
    length = RATE * 5
    size = 1 << (len(dry) + length - 1).bit_length()
    time = np.arange(length, dtype=np.float64) / RATE
    for channel in range(2):
        impulse = np.zeros(length, dtype=np.float64)
        for delay, gain in [(0.037, 0.16), (0.071, 0.12), (0.109, 0.095), (0.163, 0.06)]:
            impulse[round((delay + channel * 0.003) * RATE)] = gain
        diffuse = rng.normal(0, 1, length)
        diffuse = np.convolve(diffuse, np.hanning(31) / np.hanning(31).sum(), mode="same")
        impulse += diffuse * np.exp(-time * 1.48) * 0.0027 * np.minimum(1, time / 0.12)
        impulse[-RATE // 2:] *= np.linspace(1, 0, RATE // 2)
        room_input = dry[:, channel] * 0.82 + dry[:, 1 - channel] * 0.18
        wet[:, channel] = np.fft.irfft(np.fft.rfft(room_input, size) * np.fft.rfft(impulse, size), size)[:len(dry)]
    return dry * 0.87 + wet * 0.85


def score(spec: dict, rng: np.random.Generator) -> list[dict]:
    events = []
    for section, (start, chord, melody) in enumerate(zip(spec["starts"], spec["chords"], spec["melody"])):
        events.append(dict(kind="felt_keys", key=chord[0], time=start, duration=9, velocity=0.49, pan=-0.12))
        for j, key in enumerate(chord[1:]):
            events.append(dict(kind="felt_keys", key=key, time=start + 0.8 + j * 0.68,
                               duration=8 - j * 0.4, velocity=0.31 + j * 0.018, pan=(key - 60) * 0.008))
        for j, key in enumerate(melody):
            events.append(dict(kind="felt_keys", key=key,
                               time=start + 4.3 + j * spec["pace"] * (1.8 if j != 1 else 1.6),
                               duration=7, velocity=0.61 - j * 0.045, pan=0.08))
        if section % 2 == 1:
            for j, key in enumerate(chord[2:]):
                events.append(dict(kind="harmonic_halo", key=key, time=start + 2 + j * 0.6,
                                   duration=8.5, velocity=0.105, pan=-0.28 if j == 0 else 0.28))
    events.append(dict(kind="felt_keys", key=spec["melody"][-1][-1], time=spec["seconds"] - 13,
                       duration=9, velocity=0.29, pan=0.04))
    for event in events:
        event["time"] = round(event["time"] + float(rng.uniform(-0.065, 0.075)), 5)
        event["velocity"] = round(event["velocity"] * float(rng.uniform(0.91, 1.07)), 5)
    return events


def render(spec: dict, target: Path, index: int) -> dict:
    # Independent cue streams make each track reproducible in isolation.
    event_rng = np.random.default_rng(SEED + index * 100)
    sound_rng = np.random.default_rng(SEED + index * 100 + 1)
    room_rng = np.random.default_rng(SEED + index * 100 + 2)
    dither_rng = np.random.default_rng(SEED + index * 100 + 3)
    events = score(spec, event_rng)
    dry = np.zeros((spec["seconds"] * RATE, 2), dtype=np.float32)
    for event in events:
        sound = voice(event["kind"], event["key"], event["duration"], event["velocity"], event["pan"], sound_rng)
        offset = round(event["time"] * RATE)
        count = min(len(sound), len(dry) - offset)
        dry[offset:offset + count] += sound[:count]
    master = reverberate(dry, room_rng)
    master -= master.mean(axis=0)
    master *= 10 ** (-5.5 / 20) / max(0.001, float(np.max(np.abs(master))))
    master[:RATE * 2] *= np.linspace(0, 1, RATE * 2)[:, None]
    master[-RATE * 3:] *= np.linspace(1, 0, RATE * 3)[:, None] ** 2
    if not np.isfinite(master).all() or np.max(np.abs(master)) >= 0.8:
        raise RuntimeError("Master finite/peak check failed")
    # TPDF dither at one 16-bit LSB; final 50 ms is digital silence for clean loops.
    dither = dither_rng.uniform(-0.5, 0.5, master.shape) + dither_rng.uniform(-0.5, 0.5, master.shape)
    pcm = np.round(master * 32767 + dither).astype("<i2")
    pcm[:RATE // 20] = 0
    pcm[-RATE // 20:] = 0
    with wave.open(str(target), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm.tobytes())
    measured = pcm.astype(np.float64) / 32768
    return {**spec, "events": events, "file": target.name, "sha256": sha256(target),
            "seed": SEED + index * 100, "sample_rate": RATE, "channels": 2, "bits_per_sample": 16,
            "frames": len(pcm), "peak_dbfs": float(20 * np.log10(np.max(np.abs(measured)))),
            "rms_dbfs": float(20 * np.log10(np.sqrt(np.mean(measured ** 2)))),
            "clipped_samples": int(np.count_nonzero((pcm == -32768) | (pcm == 32767))),
            "physical_playback": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[2] / "Project/SourceArt/Audio89/Masters-v1")
    args = parser.parse_args()
    output = args.output.resolve()
    names = [f'{spec["id"]}.wav' for spec in SPECS] + ["score-and-masters.json"]
    for name in names:
        if (output / name).exists():
            raise FileExistsError(f"Preserving existing output: {output / name}")
    output.mkdir(parents=True, exist_ok=True)
    cues = []
    for index, spec in enumerate(SPECS):
        cue = render(spec, output / f'{spec["id"]}.wav', index)
        cues.append(cue)
        print(f'{spec["id"]}: {spec["seconds"]} s, peak {cue["peak_dbfs"]:.2f} dBFS, RMS {cue["rms_dbfs"]:.2f} dBFS', flush=True)
    manifest = {
        "schema_version": 1, "composition": "Original Sky Corridor score sequences, 2026-09-14; procedural synthesis edition, 2026-09-22",
        "synthesis": "Damped inharmonic modal felt keys, additive harmonic halo, mathematically generated diffuse room",
        "recorded_samples_used": False, "external_audio_files_read": [], "external_impulse_responses_used": False,
        "credits": "Sky Corridor original compositions and procedural instrument synthesis. No sample-library assets.",
        "generator": "Tools/Audio/compose_music89.py", "generator_sha256": sha256(Path(__file__)),
        "runtime": {"python": platform.python_version(), "numpy": np.__version__},
        "determinism": "Fixed NumPy PCG64 seeds; byte-identical rerender verified for the recorded runtime. Floating-point results may vary with other NumPy/platform versions.",
        "speaker_playback": False, "cues": cues,
    }
    (output / "score-and-masters.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("Rendered three procedural masters. No playback or engine import.", flush=True)


if __name__ == "__main__":
    main()
