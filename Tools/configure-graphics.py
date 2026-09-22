#!/usr/bin/env python3
"""Select the public TSR baseline or locally installed NVIDIA plugins.

Examples (from the repository root):
  python Tools/configure-graphics.py baseline
  python Tools/configure-graphics.py baseline --check
  python Tools/configure-graphics.py nvidia --engine-dir C:/Epic/UE_5.8/Engine

This changes only Project/EndlessWorld.uproject. It never downloads or copies
vendor code. NVIDIA requires compatible DLSS 4.5 / Streamline plugin APIs;
presence checks do not establish binary compatibility or device acceptance.
"""

import argparse
import json
from pathlib import Path
import sys


NVIDIA_PLUGINS = (
    "DLSS", "StreamlineCore", "StreamlineNGXCommon", "StreamlineDLSSG", "StreamlineReflex"
)


def configured_descriptor(descriptor, profile):
    """Return a copy with synchronized profile and explicit plugin enablement."""
    result = json.loads(json.dumps(descriptor))
    result["EWGraphicsProfile"] = profile
    plugins = result.setdefault("Plugins", [])
    for name in NVIDIA_PLUGINS:
        matching = [entry for entry in plugins if entry.get("Name") == name]
        if len(matching) > 1:
            raise ValueError(f"Duplicate plugin entry: {name}")
        if not matching:
            matching = [{"Name": name}]
            plugins.extend(matching)
        matching[0]["Enabled"] = profile == "nvidia"
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("profile", choices=("baseline", "nvidia"))
    parser.add_argument("--engine-dir", type=Path, help="Unreal Engine directory containing Plugins (optional if installed in Project/Plugins)")
    parser.add_argument("--check", action="store_true", help="Validate current profile without writing")
    args = parser.parse_args(argv)
    project_dir = Path(__file__).resolve().parent.parent / "Project"
    project_file = project_dir / "EndlessWorld.uproject"
    try:
        descriptor = json.loads(project_file.read_text(encoding="utf-8-sig"))
        desired = configured_descriptor(descriptor, args.profile)
        if args.profile == "nvidia":
            roots = [project_dir / "Plugins"]
            if args.engine_dir:
                roots.append(args.engine_dir.resolve() / "Plugins")
            available = {p.stem for root in roots if root.is_dir() for p in root.rglob("*.uplugin")}
            missing = sorted(set(NVIDIA_PLUGINS) - available)
            if missing:
                raise ValueError("Install licensed compatible plugins locally first (or pass --engine-dir). Missing: " + ", ".join(missing))
        if args.check:
            if descriptor != desired:
                raise ValueError(f"Profile is not configured as {args.profile}; run again without --check.")
        elif descriptor != desired:
            temporary = project_file.with_suffix(".uproject.tmp")
            temporary.write_text(json.dumps(desired, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            temporary.replace(project_file)
        print(json.dumps({"profile": args.profile, "EW_WITH_NVIDIA": int(args.profile == "nvidia"),
                          "checked_only": args.check, "project": str(project_file)}, ensure_ascii=False))
        return 0
    except (OSError, ValueError, KeyError) as error:
        print(f"Graphics setup failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
