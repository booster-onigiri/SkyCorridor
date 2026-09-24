# NVIDIA configuration / NVIDIA構成 — v0.1.5

The v0.1.5 Windows game includes runtimes from the official, unmodified UE 5.8 **DLSS 4.5 plugin 8.7.2**. The public source defaults to **baseline / TSR** and excludes vendor SDK source and plugin binaries; developers can optionally install the official SDK below. DLSS/DLAA, fixed or dynamic frame generation, ray reconstruction and Reflex are available only where the SDK reports support. Dynamic frame generation targets display refresh; the game's rendering limit is not a guarantee of final presented FPS.

The HDR update connects brightness to ACES2 reference white, applies the selected peak and UI luminance, and bypasses SDR-only post-tonemapping grading during HDR. The saved style preference is retained for SDR. The reporting user confirmed that the appearance issue is resolved; a possible RTX HDR interaction is **not an established cause**. For native HDR use Windows HDR and in-game HDR On, with RTX HDR Off for this game; see the [player guide](PLAY-EN.md). Physical photometry and general compatibility remain unverified.

[Verification](VERIFICATION.md) separates the preceding build's 16-mode/380-check graphics run from Shipping04's HDR settings and restart checks. Their automated result remains `PARTIAL_NOT_MEASURED`; settings readbacks and SDK counters do not establish native SR/RR evaluations, externally measured FPS or image quality.

## 日本語

v0.1.5のWindowsゲーム版は、公式の未変更UE 5.8用 **DLSS 4.5プラグイン8.7.2** の実行部品を同梱します。公開ソースの既定は **baseline / TSR** です。開発用SDKは任意導入で、SDKソース・プラグインのバイナリーは公開ソースに含みません。DLSS／DLAA、固定・動的フレーム生成、レイ再構成、ReflexはSDKが対応と判定した機能を利用できます。

HDR、垂直同期、FPS 上限は NVIDIA SDK がなくても利用できます。保存済みの DLSS / フレーム生成 / Reflex 設定は、対応しない構成でも保持します。公開用のソースは引き続き `baseline` を既定とし、開発者が NVIDIA 構成を任意で導入します。

NVIDIA の SDK、プラグインソース、DLL、過去の私的な改造版は、公開ソースには同梱しません。以下は公式の未変更 SDK を自分の PC に導入する手順です。NVIDIA 対応 Windows ゲームの配布では、別途確認した条件に従うランタイム部品とライセンス文書を同梱します。ゲーム本体のライセンスが NVIDIA SDK に適用されるわけではありません。

## HDR修正と確認範囲

UE 5.8のACES2が使用するPaper Whiteへ既存のHDR明るさ設定を接続し、指定ピーク輝度とUI輝度を実際の描画設定へ渡します。HDR時はSDR専用のトーンマップ後色調処理を外します。保存値は引き継ぎ、SDRへ戻すと保存済みの色調設定を再適用します。SDR復帰・終了時には、このゲームの一時的なHDR較正の上書きを解除します。

報告者から「治りました」との確認を受けています。本人が挙げたRTX HDRとの干渉は可能性で、因果関係の確認実験は行っていません。本作のHDRを使う際はWindows HDRとゲーム内HDRをオン、このゲームのRTX HDRをオフにします。詳細は[遊び方](PLAY-JA.md)と[HDR検証](HDR-VALIDATION.md)を参照してください。

Shipping04の設定検証433項目・再起動検証65項目は観測項目が通過しました。先行ビルドの16モード・380項目のグラフィックス検証とは分けて記録しています。いずれも未測定項目を残す `PARTIAL_NOT_MEASURED` で、全面的な合格ではありません。

証跡は要求値、実効CVar、viewport情報、RHI情報、DXGI記述子を区別します。DXGIではSDR/HDRのバッファ形式とOS出力情報を観測しましたが、実適用の色空間・表示画素・測光を証明するものではありません。RHIの値にもOS情報やエンジン既定値が含まれます。未取得のネイティブ評価回数などは `null` のままです。他GPU・全画面構成、外部測定FPS・遅延、全表示機器での画質は未確認です。

## 対応する公式 ZIP

- ファイル: `2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip`
- 対象: Unreal Engine 5.8 / Windows x64
- [NVIDIA の配布 URL](https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/UE-DLSS-5.8/8.7.2/2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip)
- SHA-256: `caec541ce620ca60e455151e905e0e17cf01bd7337708ff5d3cae24fc9038bef`

ZIP は自分で NVIDIA から入手し、ZIP 内の `Documentation/NVIDIA RTX SDK EULA.pdf` と `Plugins/DLSS/Source/ThirdParty/NGX/LICENSE.txt` を確認してください。認証、規約への同意、ダウンロードをセットアップスクリプトが代行することはありません。公開・再配布については、自分の配布物に適用される NVIDIA の条件を別途確認してください。

## 導入

Python 3.13 以降を使い、リポジトリのルートで実行します。エディターやビルドは終了しておきます。

```powershell
python Tools/setup-nvidia.py --sdk-zip "C:\Downloads\2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip" --verify-only
python Tools/setup-nvidia.py --sdk-zip "C:\Downloads\2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip"
python Tools/configure-graphics.py nvidia
python Tools/configure-graphics.py nvidia --check
```

展開対象は `DLSS`、`StreamlineCore`、`StreamlineNGXCommon`、`StreamlineDLSSG`、`StreamlineReflex` の5プラグインだけです。`Project/Plugins/` へ未変更で配置し、SHA-256 の読戻し結果を `Local/nvidia-install.local.json` に保存します。既存の同名プラグインがあれば上書きせず停止します。別のプロジェクトコピーで検証する場合は `--project-dir "...\Project"` を指定できます。

SDK の内容、`Local/`、ダウンロード ZIP は公開する Git やソース配布アーカイブに含めないでください。対象プラグインと `Local/` は `.gitignore` で除外しています。Windows ゲームの配布は UAT がステージしたランタイム部品と必要なライセンス文書を個別に監査し、SDK のソースや開発ツールを混ぜないでください。スクリプトは SDK の導入だけを行い、描画構成の変更は `configure-graphics.py nvidia` で明示的に行います。その後、通常のビルド手順で再ビルドしてください。

TSR に戻すには、次を実行して再ビルドします。ローカル SDK ファイルは削除しません。

```powershell
python Tools/configure-graphics.py baseline
```

## 標準 SDK と過去の計測範囲

この ZIP の公開ヘッダーには、ゲームが呼ぶ DLSS / DLAA / Ray Reconstruction / Frame Generation / Reflex の API と列挙値が存在します。この確認は静的な API 照合です。任意構成のコンパイル成功や、GPU ごとの動作・画質・遅延の検証結果とは区別してください。

2026年9月23日の独立コピーでは、公式 ZIP から481ファイルの展開・全件 SHA-256 読戻し、および `EndlessWorldEditor Win64 Development` のコンパイルとリンクが成功しました。公開側の最終ソース185ファイルとの一致、並行修正された4ファイルの再コンパイル、SDK のソース等211ファイルが未変更であることも確認しています。Engine や SDK への追加修正は行っていません。

この9月23日の検証コピーにはゲームの `Content` を入れておらず、当時の記録はコンパイル・リンクまでです。今回のv0.1.5の実行検証とは別の履歴として保持します。コンパイル成功は、以前の私的な改造版と同じ性能や描画結果を保証しません。

標準 SDK には、以前の開発環境にあった以下の独自機能はありません。

| 独自機能 | 標準 SDK での扱い |
| --- | --- |
| `EWRayReconstructionEvidence.h` | ネイティブ評価回数を取得できません。設定済みと動作確認済みを区別します。 |
| `EWPresentationEvidence.h` | この独自拡張はありません。別の読み取り専用プローブでDXGI記述子を観測しますが、実適用色空間・画素・表示タイミングの証明ではありません。 |
| `t.Streamline.Reflex.PresentationMaxFPS` | FPS 設定を UE に渡します。生成後の表示 FPS の上限は保証しません。 |
| `r.Streamline.DLSSG.DynamicTargetFrameRate` | この独自の目標FPS指定はありません。標準SDKの公式APIが対応と判定した場合、動的フレーム生成は画面のリフレッシュレートに追従して動作します。ゲームのFPS設定は、生成フレームを含む表示FPSの上限を保証しません。 |
| `r.Streamline.DLSSG.UIRecomposition` | 独自の UI 再合成制御はありません。 |

取得できない計測値は証跡 JSON で `null` と未取得の状態を示します。SDK の表示フレーム数が取得できても、外部測定による FPS、遅延、HDR 出力、画質の合格を意味しません。過去の改造版の結果を、この標準 SDK 構成の検証結果として扱わないでください。
