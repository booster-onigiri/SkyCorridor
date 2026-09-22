# Build from source

[日本語](SETUP.ja.md) · [Project](../README.md)

## Toolchain

| Tool | Version / role |
|---|---|
| Windows | 64-bit development host; public game targets Win64 / DirectX 12 |
| Unreal Engine | **5.8.2**, installed separately under Epic's terms |
| Visual Studio | **2022**, with Game development with C++ / the Unreal C++ build tools |
| Windows SDK | **10.0.26100.0** |
| MSVC toolset | **14.44.35227**, selected by the release UAT build |
| Python | **3.13** for setup and project pipeline scripts; standard library for setup |
| Blender | **4.5**, only to rebuild Blender-authored source art |
| NumPy | **2.3.5** for procedural audio; install `Tools/Audio/requirements.txt`. Optional texture generators also use NumPy and Pillow; see [assets](ASSETS.md) |
| FFmpeg + ffprobe | Optional audio/trailer verification tools; separately installed on PATH |

The SDK and MSVC versions above were selected by the release UAT build and are the
recommended reproduction environment. The wrappers do not pin their selection;
Unreal chooses from the installed toolchains.

An installed engine folder named `UE_5.8` must contain **5.8.2**: the wrapper checks
`Engine/Build/Build.version`. The directory name alone is not sufficient.
CPU, memory, storage and GPU requirements for the minimum playable configuration
have not been established. Building Unreal assets needs substantially more space
than the packaged game.

## Restore the versioned assets

Check out **v0.1.1** and use the asset archives pinned in its manifest.
This menu-only update reuses the unchanged v0.1.0 development asset archives. The repository
contains original source and generator scripts. Large Content/SourceArt files are
listed in the root `release-assets.json` and delivered through GitHub Releases.

```powershell
git clone --branch v0.1.1 https://github.com/booster-onigiri/SkyCorridor.git
cd SkyCorridor
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
```

Setup downloads the listed archives into ignored `Downloads`, verifies archive
SHA-256 and individual asset hashes, and restores `Project/Content` and
`Project/SourceArt`. Existing matching files are reused. A differing existing file
causes an error so that your edited asset is preserved; back it up or move it to a
separate work folder before restoring the release version.

For a local archive set, supply the directory containing the exact filenames from
the manifest:

```powershell
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -AssetsDirectory "D:\SkyCorridor-assets"
```

Setup does not install Unreal, Visual Studio, Python or vendor SDKs. Use `-Python`
if the desired Python executable is not `python` on PATH. `-SkipAssets` is for an
already-populated project; it is not a substitute for the release asset set.
If a tag's Release archives are not yet published, use the prepared local archives
with `-AssetsDirectory` or wait for that release. Do not mix another version's assets.

## Build Editor or Shipping

```powershell
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Editor
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Shipping
```

Each invocation creates a new `Local/Editor-<timestamp>` or
`Local/Shipping-<timestamp>` output directory with `build.log`. An explicit
`-OutputRoot` must not already exist. `-ParallelActions` defaults to 4 and can be
lowered for a constrained development machine. The wrapper checks material/shader
failure messages as well as the process exit code.

After the Editor build, open `Project/EndlessWorld.uproject` using UE 5.8.2.
Shipping output is under the selected output directory's `Archive`, with
`PLAY.cmd`, player guides, notices and `Windows`. Launch `PLAY.cmd` there;
the launcher sets the save location to its sibling `PlayData` folder.

The YouTube browser is initialized only in the packaged game. Editor/PIE cannot
validate that feature. Test browser playback using your packaged build and a
network connection; playback and quality choices also depend on the provider.

## Graphics profiles

Setup defaults to `-Graphics baseline`. The project descriptor's
`EWGraphicsProfile` selects the baseline TSR path explicitly. An optional SDK
folder appearing on disk does not automatically change the selected profile.

Use [NVIDIA setup](NVIDIA.md) for supported official SDK archives and the optional
`nvidia` profile. Vendor SDK source is kept outside the public repository. Switch
back using:

```powershell
python Tools/configure-graphics.py baseline
```

Rebuild after changing the profile. Report the profile with every performance or
compatibility result; FPS gains from optional features are not minimum hardware guarantees.

## Editing and validation

See [asset workflows](ASSETS.md) for source-art regeneration, [architecture](ARCHITECTURE.md)
for code entry points, and [experimental networking](ONLINE.md) for your own EOS setup.
Complete the checks relevant to your change in the [release checklist](RELEASE-CHECKLIST.md).
Keep source, generated assets, build verification and physical-device acceptance
as separate evidence. Do not publish `PlayData`, browser caches or real credentials.
