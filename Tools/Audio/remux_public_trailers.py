"""Replace trailer sound with the original procedural CanalAfterglow score.

Explicit source paths are read-only. Video streams are copied without encoding.
Requires ffmpeg and ffprobe in PATH. All checks write files or null output;
no playback, device access, upload, or engine launch occurs.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

from verify_audio89 import loudness


def run(command: list[str]) -> str:
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"{Path(command[0]).name} failed ({result.returncode}): {result.stderr[-5000:]}")
    return result.stdout + result.stderr


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def probe(ffprobe: str, path: Path) -> dict:
    return json.loads(run([ffprobe, "-v", "error", "-show_streams", "-show_format", "-show_data_hash", "sha256", "-of", "json", str(path)]))


def stream_hash(ffmpeg: str, path: Path) -> str:
    output = run([ffmpeg, "-v", "error", "-nostdin", "-i", str(path), "-map", "0:v:0", "-c", "copy",
                  "-f", "streamhash", "-hash", "sha256", "-"])
    match = re.search(r"SHA256=([a-f0-9]{64})", output)
    if not match:
        raise RuntimeError("Missing video packet stream hash")
    return match.group(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    repo = Path(__file__).resolve().parents[2]
    parser.add_argument("--video", type=Path, action="append", required=True)
    parser.add_argument("--music", type=Path, default=repo / "Project/SourceArt/Audio89/Masters-v1/CanalAfterglow.wav")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    ffmpeg, ffprobe = shutil.which("ffmpeg"), shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("ffmpeg and ffprobe must be on PATH")
    if args.report.exists():
        raise FileExistsError(f"Preserving existing report: {args.report}")
    manifest = json.loads(args.music.with_name("score-and-masters.json").read_text(encoding="utf-8"))
    cue = next(row for row in manifest["cues"] if row["file"] == args.music.name)
    music_hash = file_hash(args.music)
    if manifest.get("recorded_samples_used") is not False or cue["sha256"] != music_hash:
        raise RuntimeError("Require the verified procedural master")
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for source in args.video:
        source = source.resolve(strict=True)
        source_hash = file_hash(source)
        metadata = probe(ffprobe, source)
        source_video = next(row for row in metadata["streams"] if row["codec_type"] == "video")
        duration = float(source_video["duration"])
        is_hdr = source_video.get("color_transfer") == "smpte2084"
        label = "HDR-2160p60" if is_hdr else "SDR-1080p60"
        target = args.output / f"SkyCorridor-v0.1.0-{label}.mp4"
        if target.exists() or target.resolve() == source:
            raise FileExistsError(f"Preserving existing media: {target}")
        if duration > cue["seconds"] - 2:
            raise RuntimeError("Trailer exceeds the selected original score segment")
        base_filter = (f"atrim=start=2:duration={duration:.9f},asetpts=PTS-STARTPTS,"
                       f"afade=t=in:st=0:d=1.4,afade=t=out:st={duration - 3.5:.9f}:d=3.5")
        analysis = run([ffmpeg, "-hide_banner", "-nostdin", "-i", str(args.music), "-af",
                        base_filter + ",loudnorm=I=-18:TP=-2:LRA=11:print_format=json", "-f", "null", "-"])
        measured = json.loads(re.findall(r'\{\s*"input_i"[\s\S]*?\}', analysis)[-1])
        normalize = (f"loudnorm=I=-18:TP=-2:LRA=11:measured_I={measured['input_i']}:"
                     f"measured_TP={measured['input_tp']}:measured_LRA={measured['input_lra']}:"
                     f"measured_thresh={measured['input_thresh']}:offset={measured['target_offset']}:linear=true")
        print(f"Remuxing {label}; original video stream copied, audio replaced.", flush=True)
        run([ffmpeg, "-hide_banner", "-nostdin", "-v", "warning", "-n", "-i", str(source), "-i", str(args.music),
             "-map", "0:v:0", "-map", "1:a:0", "-map_metadata", "-1", "-map_chapters", "-1",
             "-c:v", "copy", "-af", base_filter + "," + normalize + ",aresample=48000",
             "-c:a", "aac", "-b:a", "320k", "-ar", "48000", "-ac", "2", "-t", str(duration),
             "-metadata", "title=Sky Corridor", "-metadata", "comment=Original procedural CanalAfterglow score; no recorded samples",
             "-movflags", "+faststart", str(target)])
        result_metadata = probe(ffprobe, target)
        final_video = next(row for row in result_metadata["streams"] if row["codec_type"] == "video")
        final_audio = [row for row in result_metadata["streams"] if row["codec_type"] == "audio"]
        keys = ["codec_name", "profile", "width", "height", "pix_fmt", "r_frame_rate", "avg_frame_rate",
                "nb_frames", "duration", "color_range", "color_space", "color_transfer", "color_primaries", "extradata_hash"]
        source_parameters = {key: source_video.get(key) for key in keys}
        final_parameters = {key: final_video.get(key) for key in keys}
        before_stream = stream_hash(ffmpeg, source)
        after_stream = stream_hash(ffmpeg, target)
        print(f"Verifying complete {label} decode and loudness.", flush=True)
        decoded = run([ffmpeg, "-hide_banner", "-nostdin", "-v", "error", "-xerror", "-err_detect", "explode",
                       "-i", str(target), "-map", "0:v:0", "-map", "0:a:0", "-progress", "pipe:1", "-f", "null", "-"])
        frames = [int(value) for value in re.findall(r"^frame=(\d+)$", decoded, re.MULTILINE)]
        loud = loudness(ffmpeg, target)
        row = {"file": target.name, "sha256": file_hash(target), "bytes": target.stat().st_size,
               "source_file": source.name, "source_sha256_before": source_hash, "source_sha256_after": file_hash(source),
               "duration_seconds": duration, "video_was_reencoded": False,
               "video_parameters": final_parameters, "video_parameters_unchanged": source_parameters == final_parameters,
               "source_video_packet_sha256": before_stream, "output_video_packet_sha256": after_stream,
               "video_packet_payload_identical": before_stream == after_stream,
               "full_video_and_audio_decode": "PASS", "decoded_video_frames": frames[-1] if frames else None,
               "output_audio_streams": len(final_audio), "audio_codec": final_audio[0]["codec_name"],
               "audio_sample_rate": int(final_audio[0]["sample_rate"]), "audio_channels": final_audio[0]["channels"],
               "loudness": loud}
        row["passed"] = all([row["source_sha256_before"] == row["source_sha256_after"], row["video_parameters_unchanged"],
                             row["video_packet_payload_identical"], row["decoded_video_frames"] == int(source_video["nb_frames"]),
                             len(final_audio) == 1, row["audio_codec"] == "aac", row["audio_sample_rate"] == 48000,
                             row["audio_channels"] == 2, -20 <= loud["integrated_lufs"] <= -16, loud["true_peak_dbtp"] <= -1.5])
        rows.append(row)
        print(f'{target.name}: {"PASS" if row["passed"] else "FAIL"}, {loud}', flush=True)
    report = {"success": all(row["passed"] for row in rows), "music_file": args.music.name, "music_sha256": music_hash,
              "recorded_samples_used": False, "original_audio_streams_retained": False,
              "music_segment_start_seconds": 2, "fade_in_seconds": 1.4, "fade_out_seconds": 3.5,
              "target_lufs": -18, "true_peak_target_dbtp": -2,
              "generator_sha256": file_hash(Path(__file__)), "speaker_playback": False,
              "visual_assets_release_clearance": "Outside audio verification scope",
              "outputs": rows}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if not report["success"]:
        raise SystemExit("Trailer verification failed; inspect report")


if __name__ == "__main__":
    main()
