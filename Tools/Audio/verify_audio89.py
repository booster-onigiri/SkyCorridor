"""Independently verify WAVs, measure BS.1770 loudness, and rerender all cues.

Requires NumPy and ffmpeg in PATH. No playback or engine use.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import wave

import numpy as np

from compose_music89 import RATE, SPECS, render, sha256


def loudness(ffmpeg: str, path: Path) -> dict:
    result = subprocess.run([ffmpeg, "-hide_banner", "-nostdin", "-v", "info", "-xerror",
                             "-err_detect", "explode", "-i", str(path), "-vn", "-af",
                             "loudnorm=I=-18:TP=-2:LRA=11:print_format=json", "-f", "null", "-"],
                            capture_output=True, text=True, check=True)
    matches = re.findall(r'\{\s*"input_i"[\s\S]*?\}', result.stderr)
    if not matches:
        raise RuntimeError("Missing FFmpeg loudness measurement")
    data = json.loads(matches[-1])
    return {"integrated_lufs": float(data["input_i"]), "true_peak_dbtp": float(data["input_tp"]),
            "loudness_range_lu": float(data["input_lra"]), "relative_threshold_lufs": float(data["input_thresh"])}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    repo = Path(__file__).resolve().parents[2]
    parser.add_argument("--masters", type=Path, default=repo / "Project/SourceArt/Audio89/Masters-v1")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.report.exists():
        raise FileExistsError(f"Preserving existing report: {args.report}")
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg must be on PATH")
    base = args.masters.resolve(strict=True)
    manifest = json.loads((base / "score-and-masters.json").read_text(encoding="utf-8"))
    rows = []
    # A fixed parent plus containment check bounds the temporary-directory cleanup.
    with tempfile.TemporaryDirectory(prefix="procedural-rerender-", dir=base.parent) as temporary:
        temporary_path = Path(temporary).resolve(strict=True)
        if not temporary_path.is_relative_to(base.parent):
            raise RuntimeError("Temporary directory outside the audio source tree")
        for index, spec in enumerate(SPECS):
            source = base / f'{spec["id"]}.wav'
            expected = next(c for c in manifest["cues"] if c["id"] == spec["id"])
            with wave.open(str(source), "rb") as wav:
                parameters = dict(sample_rate=wav.getframerate(), channels=wav.getnchannels(),
                                  bits_per_sample=wav.getsampwidth() * 8, frames=wav.getnframes(),
                                  compression=wav.getcomptype())
                pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, 2)
            normalized = pcm.astype(np.float64) / 32768
            row = {"file": source.name, "sha256": sha256(source), **parameters,
                   "duration_seconds": len(pcm) / RATE,
                   "sample_peak_dbfs": float(20 * np.log10(np.max(np.abs(normalized)))),
                   "rms_dbfs": float(20 * np.log10(np.sqrt(np.mean(normalized ** 2)))),
                   "dc_offset": normalized.mean(axis=0).tolist(),
                   "stereo_correlation": float(np.corrcoef(normalized.T)[0, 1]),
                   "clipped_samples": int(np.count_nonzero((pcm == -32768) | (pcm == 32767))),
                   "first_and_last_50ms_silent": bool(not pcm[:RATE // 20].any() and not pcm[-RATE // 20:].any()),
                   "loudness": loudness(ffmpeg, source)}
            regenerated = render(spec, temporary_path / source.name, index)
            row["deterministic_rerender_sha256"] = regenerated["sha256"]
            row["deterministic_rerender_equal"] = regenerated["sha256"] == row["sha256"]
            row["passed"] = all([
                parameters == dict(sample_rate=RATE, channels=2, bits_per_sample=16,
                                   frames=spec["seconds"] * RATE, compression="NONE"),
                row["sha256"] == expected["sha256"], row["deterministic_rerender_equal"],
                row["clipped_samples"] == 0, row["first_and_last_50ms_silent"],
                row["loudness"]["true_peak_dbtp"] <= -5, -35 < row["loudness"]["integrated_lufs"] < -15,
                max(abs(x) for x in row["dc_offset"]) < 0.0001,
            ])
            rows.append(row)
            print(f'{source.name}: {"PASS" if row["passed"] else "FAIL"}, {row["loudness"]}', flush=True)
    report = {"success": all(row["passed"] for row in rows), "recorded_samples_used": False,
              "source_manifest_sha256": sha256(base / "score-and-masters.json"),
              "generator_sha256": sha256(Path(__file__).with_name("compose_music89.py")),
              "verification_script_sha256": sha256(Path(__file__)),
              "measurement_tool": subprocess.run([ffmpeg, "-version"], capture_output=True, text=True, check=True).stdout.splitlines()[0],
              "full_audio_decode": "PASS", "physical_playback": False,
              "subjective_listening_acceptance": "NOT_TESTED", "tracks": rows}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if not report["success"]:
        raise SystemExit("Audio verification failed; inspect report")


if __name__ == "__main__":
    main()
