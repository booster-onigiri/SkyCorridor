# Sky Corridor / 空の回廊

[日本語の詳しい説明](README.ja.md) · **v0.1.5** · Free Windows exploration prototype / 無料のWindows探索ゲーム試作版 · English / 日本語

v0.1.5 adds an NVIDIA-enabled Windows build using the official Unreal Engine 5.8
DLSS 4.5 plugin **8.7.2**, while the public source defaults to **TSR**. It corrects
HDR calibration settings and bypasses SDR-only color grading during HDR. The user
reports that the HDR appearance issue is resolved; a possible interaction with
RTX HDR is unconfirmed. See [verification scope](Docs/VERIFICATION.md) and
[HDR findings](Docs/HDR-VALIDATION.md).

v0.1.5は公式DLSS 4.5プラグイン **8.7.2** の実行部品を含むWindows版です。
公開ソースの既定は **TSR** のままです。HDR設定とSDR専用の色調処理を修正し、
利用者から見た目の問題が解消したとの報告を受けました。RTX HDRとの干渉は可能性で、
原因は確定していません。[HDRの使い方](Docs/PLAY-JA.md)も参照してください。

An empty city floats above the clouds. Its water still flows and its trains still
run. Walk its canals, ride to the upper districts, and use a small handheld device
to find faint traces of the lives once lived here.

誰もいなくなった天空都市。水は流れ、列車は今も走っています。
水路を歩き、上層の街へ移動し、小さな端末でかつての暮らしの痕跡を集める探索ゲームです。
Unreal Engineと、CodexのGPT-6 Astraによる制作支援を使って開発しています。
ゲーム内表示は日本語・英語に対応し、非商用で改造・再配布できるソースも公開しています。

- **遊ぶ：** [v0.1.5のWindows版をダウンロード](https://github.com/booster-onigiri/SkyCorridor/releases/tag/v0.1.5) → ZIP全体を展開 → **PLAY.cmd** で起動。[日本語の遊び方](Docs/PLAY-JA.md)
- **開発する：** [日本語のセットアップ・ビルド手順](Docs/SETUP.ja.md)。Unreal Engine 5.8.2と、ソースに対応した素材アーカイブを使用します。

公開版は一人用の探索が中心です。マルチプレイや世界の卵・追加要素は開発中で、標準では選択できません。
ゲーム内のYouTube再生機能は公開版では中止しています。[詳しい仕様・利用条件はこちら](README.ja.md)。

See the [verification results and remaining limits](Docs/VERIFICATION.md).

## Play

Get the Windows package from [the v0.1.5 release](https://github.com/booster-onigiri/SkyCorridor/releases/tag/v0.1.5).
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
Older promotional footage may show the historical YouTube prototype, which is not available in the current game.

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
git clone --branch v0.1.5 https://github.com/booster-onigiri/SkyCorridor.git
cd SkyCorridor
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Editor
```

These commands select the v0.1.5 source.

Large `Project/Content` and `Project/SourceArt` files are distributed as matching
Release archives. Setup downloads and verifies them using `release-assets.json`;
the GitHub source ZIP alone is not a complete project. Already-downloaded archives
can be supplied with `-AssetsDirectory`. Use the assets pinned by this source tag's manifest. v0.1.5 reuses the unchanged v0.1.0 development assets. See [setup/build instructions](Docs/SETUP.md) and the [asset workflow](Docs/ASSETS.md).

The public source checkout defaults to **baseline**, using Unreal's **TSR**.
The v0.1.5 Windows package includes NVIDIA runtimes; supported features depend on
your GPU and driver, and TSR remains available. NVIDIA SDK plugins are optional
for developers and are not included in the public source checkout. Their
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
