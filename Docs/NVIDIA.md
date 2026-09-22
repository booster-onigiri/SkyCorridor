# NVIDIA は任意のローカル構成です

公開ソースと既定の Windows 版は Unreal Engine の TSR を使用します。HDR、垂直同期、FPS 上限は NVIDIA SDK がなくても利用できます。保存済みの DLSS / フレーム生成 / Reflex 設定は、対応しない構成でも保持します。

NVIDIA の SDK、プラグインソース、DLL、過去の私的な改造版は、このリポジトリには同梱していません。以下は公式の未変更 SDK を自分の PC に導入する手順です。ゲーム本体のライセンスが NVIDIA SDK に適用されるわけではありません。

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

SDK の内容、`Local/`、ダウンロード ZIP は公開する Git や配布アーカイブに含めないでください。対象プラグインと `Local/` は `.gitignore` で除外しています。スクリプトは SDK の導入だけを行い、描画構成の変更は `configure-graphics.py nvidia` で明示的に行います。その後、通常のビルド手順で再ビルドしてください。

TSR に戻すには、次を実行して再ビルドします。ローカル SDK ファイルは削除しません。

```powershell
python Tools/configure-graphics.py baseline
```

## 標準 SDK と計測の範囲

この ZIP の公開ヘッダーには、ゲームが呼ぶ DLSS / DLAA / Ray Reconstruction / Frame Generation / Reflex の API と列挙値が存在します。この確認は静的な API 照合です。任意構成のコンパイル成功や、GPU ごとの動作・画質・遅延の検証結果とは区別してください。

2026年9月23日の独立コピーでは、公式 ZIP から481ファイルの展開・全件 SHA-256 読戻し、および `EndlessWorldEditor Win64 Development` のコンパイルとリンクが成功しました。公開側の最終ソース185ファイルとの一致、並行修正された4ファイルの再コンパイル、SDK のソース等211ファイルが未変更であることも確認しています。Engine や SDK への追加修正は行っていません。

この検証コピーにはゲームの `Content` を入れておらず、NVIDIA 構成のゲーム起動・GPU 動作・画質・HDR 出力・FPS・遅延・Shipping パッケージは未検証です。コンパイル成功は、以前の私的な改造版と同じ性能や描画結果を保証しません。

標準 SDK には、以前の開発環境にあった以下の独自機能はありません。

| 独自機能 | 標準 SDK での扱い |
| --- | --- |
| `EWRayReconstructionEvidence.h` | ネイティブ評価回数を取得できません。設定済みと動作確認済みを区別します。 |
| `EWPresentationEvidence.h` | DXGI の直接計測は未取得です。HDR は UE の画面設定から確認します。 |
| `t.Streamline.Reflex.PresentationMaxFPS` | FPS 設定を UE に渡します。生成後の表示 FPS の上限は保証しません。 |
| `r.Streamline.DLSSG.DynamicTargetFrameRate` | 動的フレーム生成の選択と適用を無効にします。保存済みの設定は保持します。 |
| `r.Streamline.DLSSG.UIRecomposition` | 独自の UI 再合成制御はありません。 |

取得できない計測値は証跡 JSON で `null` と未取得の状態を示します。SDK の表示フレーム数が取得できても、外部測定による FPS、遅延、HDR 出力、画質の合格を意味しません。過去の改造版の結果を、この標準 SDK 構成の検証結果として扱わないでください。
