# Project map / プロジェクトの構成

The project is a native Unreal C++ application with generated/imported art and a
small amount of local persistent data. Public setup uses relative repository paths;
packaged `PLAY.cmd` chooses a save directory beside the launcher.

| Area | Main entry points under `Project/Source/EndlessWorld` |
|---|---|
| World and traversal | `EWWorld`, `EWCharacter`, `EWChunkManager`; water-city, rail, lift and district modules |
| Time and atmosphere | `EWDayCycle`, `EWWorldClock`, lighting and sky modules |
| Handheld UI and memories | `EWTerminal`, `EWTerminalView`, `EWMemoryPlan`; map, records and photo modules |
| Persistence | `EWSaveStore`, `EWSocialStore`; local data rooted by the launcher |
| Music and video | `EWMusic`, `EWMediaScreen`, `EWBrowserSurface`, `EWBrowserAudioWave` |
| Experimental online work | `EWSocialSession`, `EWCinemaSession`, their protocol/connection helpers |
| Focused diagnostics | `*Audit*` modules and commandlets; each covers its named scope |

`Project/Plugins/EWGamepad` integrates SDL3 for controller input.
`Project/Plugins/EWEditorTools` contains project-specific Editor helpers.
`Tools/setup.py` restores hashed release assets. `Tools/configure-graphics.py`
selects the explicit `EWGraphicsProfile` used by the project descriptor and build
rules. `build.ps1` drives Unreal's Editor or Shipping pipeline.

Browser initialization rejects Editor/PIE and null-renderer contexts. A packaged
build is necessary to check actual YouTube behavior. Online connections are gated
by `-EWEnableExperimentalOnline`; the default public experience remains solo.
An audit commandlet provides evidence only for what it actually exercises.

## 日本語

UnrealのC++コードを中心に、生成・取り込み素材とローカル保存を組み合わせた構成です。
ワールド・移動、時間・照明、端末・記録、保存、音楽・動画、オンライン実験の主な入口は上表のとおりです。

`EWGamepad` はSDL3を用いたコントローラー入力、`EWEditorTools` は制作補助です。
`Tools/setup.py` がハッシュ付き素材を復元し、`Tools/configure-graphics.py` が
`EWGraphicsProfile` を設定します。`build.ps1` がEditor・Shippingの構築を実行します。

パッケージ版の `PLAY.cmd` は隣の `PlayData` を保存先として指定します。
ブラウザーはEditor/PIEや描画なしの環境では起動せず、実際のYouTube動作にはパッケージ版が必要です。
オンラインは実験用フラグで明示的に有効化します。診断用コマンドレットの結果は、
その処理で確認した範囲の証拠として扱ってください。
