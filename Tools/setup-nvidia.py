#!/usr/bin/env python3
"""Install five optional stock NVIDIA plugins from a user-supplied official ZIP.

No download, license acceptance, existing-file replacement, or graphics-profile
change is performed. Read the SDK license yourself before installation.
Use --verify-only to inspect the exact archive without creating any files.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import sys
import uuid
import zipfile


SDK_SHA256 = "caec541ce620ca60e455151e905e0e17cf01bd7337708ff5d3cae24fc9038bef"
SDK_URL = "https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/UE-DLSS-5.8/8.7.2/2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip"
SDK_NAME = "2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip"
PLUGINS = ("DLSS", "StreamlineCore", "StreamlineNGXCommon", "StreamlineDLSSG", "StreamlineReflex")
RESERVED_NAMES = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1, 10)), *(f"lpt{i}" for i in range(1, 10))}
MAX_UNCOMPRESSED_BYTES = 4 * 1024 ** 3


def sha256_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def safe_archive_path(info):
    """Reject traversal, Windows alternate streams, links and path aliases."""
    name = info.filename
    if "\\" in name or "\x00" in name or name.startswith("/"):
        raise ValueError(f"Unsafe ZIP entry: {name!r}")
    parts = name.rstrip("/").split("/")
    for part in parts:
        if not part or part in (".", "..") or ":" in part or part.endswith((" ", ".")):
            raise ValueError(f"Unsafe ZIP entry: {name!r}")
        if part.split(".", 1)[0].casefold() in RESERVED_NAMES:
            raise ValueError(f"Reserved Windows name in ZIP: {name!r}")
    mode = info.external_attr >> 16
    if stat.S_ISLNK(mode) or (stat.S_IFMT(mode) and not (stat.S_ISREG(mode) or stat.S_ISDIR(mode))):
        raise ValueError(f"Non-regular ZIP entry: {name!r}")
    if info.flag_bits & 1:
        raise ValueError(f"Encrypted ZIP entry: {name!r}")
    return PurePosixPath(*parts)


def selected_entries(archive):
    entries = []
    seen = set()
    total = 0
    for info in archive.infolist():
        parts = safe_archive_path(info).parts
        # No Samples, Platforms, unrelated plugins or top-level documentation.
        if len(parts) < 3 or parts[0] != "Plugins" or parts[1] not in PLUGINS:
            continue
        relative = PurePosixPath(*parts[1:])
        key = str(relative).casefold()
        if key in seen:
            raise ValueError(f"Duplicate or case-colliding ZIP entry: {info.filename}")
        seen.add(key)
        if not info.is_dir():
            entries.append((info, relative))
            total += info.file_size
    if len(entries) > 5000 or total > MAX_UNCOMPRESSED_BYTES:
        raise ValueError("Unexpected SDK payload size")
    names = {str(relative) for _, relative in entries}
    for plugin in PLUGINS:
        descriptor_name = f"{plugin}/{plugin}.uplugin"
        if descriptor_name not in names:
            raise ValueError(f"Missing plugin descriptor: {descriptor_name}")
        descriptor = json.loads(archive.read(f"Plugins/{descriptor_name}").decode("utf-8-sig"))
        if descriptor.get("EngineVersion") != "5.8.0":
            raise ValueError(f"Unexpected engine version: {plugin}")
        for dependency in descriptor.get("Plugins", []):
            if dependency.get("Enabled") and dependency.get("Name") not in PLUGINS:
                raise ValueError(f"Unexpected plugin dependency: {plugin}: {dependency.get('Name')}")
    return entries, total


def reject_link(path):
    if path.is_symlink() or path.is_junction():
        raise ValueError(f"Refusing linked installation directory: {path}")


def install(archive, entries, project_dir):
    if not (project_dir / "EndlessWorld.uproject").is_file():
        raise ValueError(f"No EndlessWorld.uproject in {project_dir}")
    plugins_dir = project_dir / "Plugins"
    local_dir = project_dir.parent / "Local"
    reject_link(plugins_dir)
    reject_link(local_dir)
    existing = [str(plugins_dir / name) for name in PLUGINS if (plugins_dir / name).exists()]
    if existing:
        raise ValueError("Existing plugins are preserved. Move/back up them yourself before installation: " + ", ".join(existing))
    plugins_dir.mkdir(parents=True, exist_ok=True)
    local_dir.mkdir(parents=True, exist_ok=True)
    staging = local_dir / f"nvidia-staging-{uuid.uuid4().hex}"
    staging.mkdir()
    file_hashes = {}
    moved = []
    try:
        for info, relative in entries:
            destination = staging.joinpath(*relative.parts)
            if not destination.resolve().is_relative_to(staging.resolve()):
                raise ValueError(f"Destination escaped staging: {relative}")
            destination.parent.mkdir(parents=True, exist_ok=True)
            digest = hashlib.sha256()
            count = 0
            with archive.open(info) as source, destination.open("xb") as output:
                while chunk := source.read(1024 * 1024):
                    output.write(chunk)
                    digest.update(chunk)
                    count += len(chunk)
            if count != info.file_size:
                raise ValueError(f"Incomplete extraction: {relative}")
            file_hashes[str(relative)] = digest.hexdigest()
        for name in PLUGINS:
            target = plugins_dir / name
            if target.exists() or target.is_symlink() or target.is_junction():
                raise ValueError(f"Target appeared during installation; preserved: {target}")
            (staging / name).rename(target)
            moved.append(name)
        # Independently read back installed bytes before reporting success.
        for relative, expected in file_hashes.items():
            if sha256_file(plugins_dir.joinpath(*PurePosixPath(relative).parts)) != expected:
                raise ValueError(f"Installed file verification failed: {relative}")
    except Exception:
        # Move only this invocation's directories back; never delete pre-existing
        # files. Preserve the staged SDK on failure for diagnosis/recovery.
        for name in reversed(moved):
            target = plugins_dir / name
            if target.exists() and not (staging / name).exists():
                target.rename(staging / name)
        raise
    else:
        staging.rmdir()
    return file_hashes


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-zip", type=Path, required=True, help="ZIP obtained by you from NVIDIA after reviewing its license")
    parser.add_argument("--project-dir", type=Path, default=Path(__file__).resolve().parent.parent / "Project")
    parser.add_argument("--verify-only", action="store_true", help="Verify the archive without extraction or profile changes")
    args = parser.parse_args(argv)
    try:
        sdk_zip = args.sdk_zip.resolve(strict=True)
        actual = sha256_file(sdk_zip)
        if actual != SDK_SHA256:
            raise ValueError(f"Unexpected SDK SHA-256: {actual}; expected {SDK_SHA256}")
        with zipfile.ZipFile(sdk_zip) as archive:
            entries, total = selected_entries(archive)
            hashes = {} if args.verify_only else install(archive, entries, args.project_dir.resolve())
        result = {
            "status": "archive_verified" if args.verify_only else "installed_and_readback_verified",
            "checked_at_utc": datetime.now(timezone.utc).isoformat(),
            "sdk_name": SDK_NAME, "source_url": SDK_URL, "sha256": actual,
            "plugins": list(PLUGINS), "files": len(entries), "uncompressed_bytes": total,
            "license_acceptance_performed": False, "profile_changed": False,
            "download_performed": False, "build_or_runtime_verified": False,
        }
        if hashes:
            result["installed_file_sha256"] = hashes
            record = args.project_dir.resolve().parent / "Local" / "nvidia-install.local.json"
            record.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            result.pop("installed_file_sha256")
            result["local_record"] = str(record)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"NVIDIA setup failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
