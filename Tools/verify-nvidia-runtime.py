#!/usr/bin/env python3
"""Verify stock NVIDIA Shipping files without launching the game.

The install receipt is always Local/nvidia-install.local.json beside this tool's
repository. The output must be new and outside the archive being inspected.
This supplements audit-release.py; it does not inspect cooked package contents,
prove device compatibility, accept a license, or measure graphics/performance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import zipfile


ROOT = Path(__file__).resolve().parent.parent
INSTALL_RECORD = ROOT / "Local/nvidia-install.local.json"
SDK_SHA256 = "caec541ce620ca60e455151e905e0e17cf01bd7337708ff5d3cae24fc9038bef"
SDK_NAME = "2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip"
PLUGINS = frozenset(("DLSS", "StreamlineCore", "StreamlineNGXCommon", "StreamlineDLSSG", "StreamlineReflex"))
PLUGIN_KEYS = frozenset(name.casefold() for name in PLUGINS)
RUNTIME_PREFIX = "Windows/EndlessWorld/Plugins/"
PRODUCTION_DLLS = tuple(
    f"{plugin}/Binaries/ThirdParty/Win64/{name}"
    for plugin, names in (
        ("DLSS", ("nvngx_dlss.dll", "nvngx_dlssd.dll")),
        ("StreamlineCore", ("sl.interposer.dll", "sl.common.dll", "sl.reflex.dll",
                            "sl.pcl.dll", "nvngx_dlssg.dll", "sl.dlss_g.dll",
                            "nvngx_deepdvc.dll", "sl.deepdvc.dll")),
    ) for name in names
)
LICENSES = {
    "Licenses/NVIDIA-RTX-SDK-LICENSE.txt": "dc2778a3283427285984cdb5b3f7f03eae7d8a06057f672f2999c5ee7fd4f67d",
    "Licenses/NVIDIA-RTX-SDK-EULA.pdf": "640951de3a7af9e5d30628ca7e528a5b815f6902fc879cba96eb478b072578b8",
}
FORBIDDEN_DIRS = frozenset(("development", "debug", "source", "intermediate", "samples", "documentation"))
SOURCE_SUFFIXES = frozenset((".h", ".hpp", ".c", ".cc", ".cpp", ".cs", ".lib", ".obj", ".pdb", ".exp", ".precompiled"))


def sha256_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def linked(path):
    return path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction())


def reject_linked_ancestors(path):
    for candidate in (path, *path.parents):
        if linked(candidate):
            raise ValueError(f"Refusing a linked path: {candidate}")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def install_expectations(record_path):
    reject_linked_ancestors(record_path)
    data = record_path.read_bytes()
    record = json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique_object)
    if not isinstance(record, dict) or record.get("status") != "installed_and_readback_verified":
        raise ValueError("NVIDIA install receipt is not a verified installation")
    if record.get("sha256") != SDK_SHA256 or record.get("sdk_name") != SDK_NAME:
        raise ValueError("NVIDIA install receipt does not describe the reviewed stock 8.7.2 SDK")
    plugins = record.get("plugins")
    if not isinstance(plugins, list) or not all(isinstance(p, str) for p in plugins) or len(plugins) != 5 or set(plugins) != PLUGINS:
        raise ValueError("NVIDIA install receipt must contain exactly the five reviewed plugins")
    hashes = record.get("installed_file_sha256")
    if not isinstance(hashes, dict):
        raise ValueError("NVIDIA install receipt has no file hashes")
    expected = {}
    for relative in PRODUCTION_DLLS:
        value = hashes.get(relative)
        if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
            raise ValueError(f"Missing or invalid production DLL hash in install receipt: {relative}")
        expected[RUNTIME_PREFIX + relative] = value
    return expected, hashlib.sha256(data).hexdigest()


def plugin_context(parts):
    return any(part.casefold() in PLUGIN_KEYS for part in parts)


def zip_contains_sdk(path):
    # Inspect only names, never extract or run nested SDK content. This also
    # detects the original SDK ZIP when renamed before being accidentally copied.
    with zipfile.ZipFile(path) as archive:
        for entry in archive.infolist():
            parts = PurePosixPath(entry.filename.replace("\\", "/")).parts
            if plugin_context(parts):
                return True
    return False


def audit(archive_root, record_path=INSTALL_RECORD):
    archive_root = Path(archive_root).absolute()
    record_path = Path(record_path).absolute()
    report = {
        "format": "ew.nvidia-runtime-files", "schema_version": 1,
        "checked_at_utc": datetime.now(timezone.utc).isoformat(),
        "archive_root": str(archive_root), "install_record": str(record_path),
        "sdk_name": SDK_NAME, "sdk_sha256": SDK_SHA256,
        "success": False, "runtime_tested": False, "performance_tested": False,
        "scope": "Loose Shipping NVIDIA payload and two license hashes only; run audit-release.py and real-device tests separately.",
        "files_scanned": 0, "production_dlls_checked": 0, "licenses_checked": 0,
        "checked_files": [], "problems": [],
    }
    problems = report["problems"]

    def problem(relative, reason):
        problems.append({"file": str(relative), "reason": reason})

    try:
        reject_linked_ancestors(archive_root)
        if not archive_root.is_dir():
            raise ValueError("Archive root is not a directory")
        expected, record_digest = install_expectations(record_path)
        report["install_record_sha256"] = record_digest
    except (OSError, ValueError) as error:
        problem("inputs", str(error))
        return report

    expected.update(LICENSES)
    allowed_dlls = {key.casefold() for key in expected if key.lower().endswith(".dll")}
    production_names = {PurePosixPath(key).name.casefold() for key in allowed_dlls}
    inventory = {}
    seen = set()

    def walk_error(error):
        problem(getattr(error, "filename", "archive"), f"Cannot enumerate archive: {error}")

    for parent, dirs, files in os.walk(archive_root, topdown=True, followlinks=False, onerror=walk_error):
        parent = Path(parent)
        for name in sorted(dirs + files):
            path = parent / name
            relative = path.relative_to(archive_root).as_posix()
            key = relative.casefold()
            if key in seen:
                problem(relative, "Case-colliding archive paths")
            seen.add(key)
            if linked(path):
                problem(relative, "Linked files and directories are not inspectable release content")
                if name in dirs:
                    dirs.remove(name)
                continue
            parts = path.relative_to(archive_root).parts
            nvidia = plugin_context(parts)
            lower_parts = {part.casefold() for part in parts}
            if nvidia and FORBIDDEN_DIRS.intersection(lower_parts):
                problem(relative, "NVIDIA development, source or SDK-only component")
            if name in dirs:
                continue
            report["files_scanned"] += 1
            inventory[key] = path
            suffix = path.suffix.casefold()
            lower_name = name.casefold()
            if lower_name in production_names and key not in allowed_dlls:
                problem(relative, "NVIDIA production DLL copied outside its approved runtime path")
            if nvidia:
                if not key.startswith(RUNTIME_PREFIX.casefold()):
                    problem(relative, "NVIDIA plugin payload outside the Shipping plugin directory")
                if suffix in SOURCE_SUFFIXES or lower_name.startswith("sl.imgui.") or (suffix == ".dll" and "editor" in lower_name):
                    problem(relative, "NVIDIA source, linker input, symbols, editor module or debug overlay")
                if suffix == ".dll" and key not in allowed_dlls:
                    problem(relative, "NVIDIA DLL is not one of the ten approved production paths")
            if suffix == ".zip":
                try:
                    if lower_name == SDK_NAME.casefold() or zip_contains_sdk(path):
                        problem(relative, "Raw NVIDIA SDK/plugin ZIP must not ship")
                except (OSError, ValueError, zipfile.BadZipFile, NotImplementedError) as error:
                    problem(relative, f"Cannot inspect ZIP for NVIDIA SDK content: {error}")

    for relative, expected_hash in expected.items():
        path = inventory.get(relative.casefold())
        row = {"file": relative, "expected_sha256": expected_hash, "sha256": None, "matches": False}
        report["checked_files"].append(row)
        if path is None:
            problem(relative, "Required production DLL or license is missing")
            continue
        try:
            # Read-only digest; never load a DLL to determine its identity.
            row["sha256"] = sha256_file(path)
            row["bytes"] = path.stat().st_size
            row["matches"] = row["sha256"] == expected_hash
            report["production_dlls_checked" if relative.endswith(".dll") else "licenses_checked"] += 1
            if not row["matches"]:
                problem(relative, "SHA-256 differs from the reviewed install receipt or pinned license")
        except OSError as error:
            problem(relative, f"Cannot hash required file: {error}")
    report["success"] = not problems
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", required=True, type=Path, help="UAT Archive directory containing Windows and Licenses")
    parser.add_argument("--output", required=True, type=Path, help="New JSON report outside the archive; existing files are preserved")
    args = parser.parse_args(argv)
    try:
        output = args.output.absolute()
        reject_linked_ancestors(output)
        if output.exists():
            raise ValueError(f"Preserving existing output: {output}")
        if output.resolve().is_relative_to(args.archive_root.resolve()):
            raise ValueError("Output must be outside the read-only archive")
        report = audit(args.archive_root)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("x", encoding="utf-8", newline="\n") as stream:
            json.dump(report, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
        print(json.dumps({"success": report["success"], "output": str(output),
                          "problems": len(report["problems"]), "runtime_tested": False}))
        return 0 if report["success"] else 1
    except (OSError, ValueError) as error:
        print(f"NVIDIA runtime file verification failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
