# Sky Corridor / 空の回廊

[日本語](README.ja.md) · **v0.1.4** · Free Windows exploration prototype · English / Japanese in-game interface

An empty city floats above the clouds. Its water still flows and its trains still
run. Walk its canals, ride to the upper districts, and use a small handheld device
to find faint traces of the lives once lived here.

See the [verification results and remaining limits](Docs/VERIFICATION.md).

## Play

Get the Windows package from [the v0.1.4 release](https://github.com/booster-onigiri/SkyCorridor/releases/tag/v0.1.4).
Extract the entire archive into a writable folder and launch **PLAY.cmd** beside
the **Windows** folder. The Unreal Editor is not required. Progress is stored in
**PlayData** beside the launcher; back up that folder before updating.

Start with **WASD** to walk, **mouse** to look, **E** to interact, and **Q** to open
the handheld device. At Clock Plaza, choose **Q → Observe → Begin observing** to begin
looking for a memory. See the complete [English player guide](Docs/PLAY-EN.md) or
[日本語の遊び方](Docs/PLAY-JA.md).

The public release focuses on solo exploration on 64-bit Windows with DirectX 12.
Minimum GPU/CPU/RAM requirements have not been established. The interface supports English and Japanese. Use the language button on the title screen or in Graphics & Controls. Your choice is saved in PlayData/language.txt. On first launch, Japanese Windows defaults to Japanese; other system languages default to English. Online city browsing, world eggs/add-ons and the phone Friends app
are labelled **In development** / **開発中** and disabled by default, including the T shortcut.
Shared cinema is marked **Unavailable in public build** / **公開版では利用不可** and cannot be enabled by experimental flags.
Solo exploration, fishing and photography remain available. **In-game YouTube playback has been discontinued in the public release.**
The plaza, cinema and sky screens remain part of the scenery; they do not play online videos.
Older promotional footage may show the historical YouTube prototype, which is not available in v0.1.4.

v0.1.4 clears the skyline by removing the elevators' full-height guide masts and
the airship port's four decorative support columns. The lifts now float between
their existing stops; cabins, doors, landing floors and safety guards are retained.
See [the change details and verification scope](Docs/LIFT-RODS-REMOVAL.md).

## Build and explore the source

Use **Unreal Engine 5.8.2**, Visual Studio 2022 with C++ game-development tools,
Windows SDK **10.0.26100.0**, and Python **3.13** for the setup pipeline. Blender
**4.5** is needed only when rebuilding Blender-authored assets. The setup pipeline
uses Python's standard library; procedural music regeneration additionally uses
the pinned NumPy dependency in `Tools/Audio/requirements.txt`.

```powershell
git clone --branch v0.1.4 https://github.com/booster-onigiri/SkyCorridor.git
cd SkyCorridor
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Editor
```

Large `Project/Content` and `Project/SourceArt` files are distributed as matching
Release archives. Setup downloads and verifies them using `release-assets.json`;
the GitHub source ZIP alone is not a complete project. Already-downloaded archives
can be supplied with `-AssetsDirectory`. Use the assets pinned by this source tag's manifest. v0.1.4 reuses the unchanged v0.1.0 development assets. See [setup/build instructions](Docs/SETUP.md) and the [asset workflow](Docs/ASSETS.md).

The default graphics profile is **baseline**, using Unreal's **TSR**. NVIDIA SDK
plugins are optional and are not included in the public source checkout. Their
explicit setup and `EWGraphicsProfile` selection are covered in [NVIDIA setup](Docs/NVIDIA.md).
Experimental networking has its own [configuration guide](Docs/ONLINE.md).

## License and gameplay videos

Project-owned source code is available under the **PolyForm Noncommercial License
1.0.0** in [LICENSE](LICENSE). Project-owned original art, music and other assets
use **CC BY-NC 4.0** in [ASSET-LICENSE](ASSET-LICENSE). This is source-available
software with noncommercial terms.

**You may record, stream and monetize your own gameplay videos**, including the
project's original music as heard during play, under the explicit
[gameplay video permission](GAMEPLAY-VIDEO-PERMISSION.md). That permission does not
cover unrelated third-party videos or music.
Engine, SDK and library terms remain separate; retain the
[third-party notices](THIRD-PARTY-NOTICES.md).
The [product end-user terms](Docs/END-USER-TERMS.md) explain the packaged Unreal
technology permission and Epic disclaimer.

Built with Unreal Engine and assistance from GPT-6 Astra in Codex. See
[contributing](CONTRIBUTING.md), [architecture](Docs/ARCHITECTURE.md), and the
[release verification checklist](Docs/RELEASE-CHECKLIST.md). Report reproducible
problems through [Issues](https://github.com/booster-onigiri/SkyCorridor/issues)
with the release, Windows/GPU details, and steps to reproduce.
