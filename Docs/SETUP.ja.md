# ソースから構築する

[English](SETUP.md) · [プロジェクト](../README.ja.md)

## 使用する環境

| ツール | 版・用途 |
|---|---|
| Windows | 64bitの開発環境。公開ゲームはWin64 / DirectX 12用 |
| Unreal Engine | **5.8.2**。Epicの条件に従い別途導入 |
| Visual Studio | **2022**。C++によるゲーム開発とUnrealのC++ビルド環境 |
| Windows SDK | **10.0.26100.0** |
| MSVCツールセット | リリースのUATビルドで選択された **14.44.35227** |
| Python | セットアップ・制作スクリプト用に **3.13**。セットアップは標準ライブラリーで動作 |
| Blender | **4.5**。Blenderで制作した元素材を再生成するときに使用 |
| NumPy | 独自音源用は **2.3.5** を `Tools/Audio/requirements.txt` から導入。任意のテクスチャ生成ではNumPyとPillowも使用。[素材の説明](ASSETS.md)を参照 |
| FFmpeg / ffprobe | 音源・トレーラーの検証時に任意導入し、PATHへ登録 |

SDKとMSVCは、リリースのUATビルドで実際に選択された版を再現用の推奨環境として記載しています。
構築スクリプトによる固定指定はなく、Unrealが導入済みの環境から選択します。

例にある `UE_5.8` フォルダーの中身は **5.8.2** が必要です。
構築スクリプトは `Engine/Build/Build.version` の値を確認します。
最低動作環境のCPU・メモリー・GPU要件は未確定です。素材の構築には配布版より多くの空き容量を使用します。

## 同じ版の素材を準備する

**v0.1.5** のソースと、その `release-assets.json` が指定する素材を組み合わせます。
今回はNVIDIA対応Windows版と、本作のHDR設定・SDR専用色調処理の修正を含みます。
公開ソースの既定はTSRのままで、開発用NVIDIA SDKは[任意導入](NVIDIA.md)です。公開版のYouTube再生中止を維持し、
変更のないv0.1.0の素材アーカイブを再利用します。
リポジトリーには独自ソースと生成スクリプトを置き、大容量のContent/SourceArtは
ルートの `release-assets.json` に記録してGitHub Releasesから配布します。

```powershell
git clone --branch v0.1.5 https://github.com/booster-onigiri/SkyCorridor.git
cd SkyCorridor
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
```

セットアップは対象アーカイブをGit管理外の `Downloads` へ取得し、アーカイブ全体と
各ファイルのSHA-256を検証して `Project/Content` と `Project/SourceArt` を復元します。
既にある同じファイルは再利用します。内容が異なるファイルは上書きせずエラーにするため、
編集した素材は先にバックアップするか、別の作業フォルダーへ移してください。

取得済みのアーカイブを使う場合は、マニフェストと同じファイル名で揃えたフォルダーを指定します。

```powershell
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -AssetsDirectory "D:\SkyCorridor-assets"
```

Unreal、Visual Studio、Python、ベンダーSDKを自動導入する処理ではありません。
Pythonを明示する場合は `-Python` を指定します。`-SkipAssets` は素材を配置済みの環境用です。
Releaseのアーカイブが未公開の場合は、準備済みのローカルアーカイブを `-AssetsDirectory` で
指定するか、その版の公開を待ってください。異なる版の素材を混ぜないでください。

## EditorとShippingの構築

```powershell
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Editor
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Shipping
```

実行ごとに `Local/Editor-<日時>` または `Local/Shipping-<日時>` を作り、`build.log` を保存します。
`-OutputRoot` を指定する場合、既にあるフォルダーは使用できません。
`-ParallelActions` の標準値は4です。開発PCの余裕に応じて下げられます。
処理の終了コードに加え、マテリアル・シェーダーの失敗メッセージも検査します。

どちらのターゲットでも、最初にEditorモジュールをビルドし、導入済みのUnreal Engineで
`Tools/Art/ensure_cloud_materials.py` を実行します。ソース・開発用素材アーカイブには含めない
エンジン由来の雲マテリアル `M_CloudLayers` と `MI_CloudSea` を、ここで生成します。
v0.1.4以降は `-Target Shipping` を直接指定してもCook前にこの処理を行うため、
Editorビルドを手動で先に実行する必要はありません。準備処理のログも指定した出力先へ保存します。

Editorのビルド後は、UE 5.8.2で `Project/EndlessWorld.uproject` を開きます。
Shippingの出力先は作成したフォルダーの `Archive` で、`PLAY.cmd`、遊び方、権利表記、
`Windows` が入ります。そこで `PLAY.cmd` を起動すると、隣の `PlayData` に保存します。

公開ビルドはブラウザー再生をコンパイル対象から外します。Editor/PIE・パッケージ版とも
YouTube再生はできず、モニターは景観として残ります。再生を有効にする起動引数はありません。
オンライン・追加要素の実験用フラグでも再生は復活しません。過去のメディア検証は旧版の記録であり、
現在の公開版が再生を提供するという証拠には使用できません。

## グラフィックス構成

標準は `-Graphics baseline` で、プロジェクト設定の `EWGraphicsProfile` により
TSRの構成を選択します。任意のSDKフォルダーが存在しても、自動でNVIDIA構成へ切り替わりません。

公式SDKの任意導入と `nvidia` 構成は [NVIDIAの手順](NVIDIA.md) に従ってください。
SDKのソースは公開リポジトリーに含めません。標準へ戻す場合は次を実行し、再ビルドします。

```powershell
python Tools/configure-graphics.py baseline
```

性能・互換性の報告には構成名を含めてください。任意機能によるFPS改善は最低動作環境の保証ではありません。

## 変更と確認

[素材の構成](ASSETS.md)、[コードの構成](ARCHITECTURE.md)、[実験用オンライン設定](ONLINE.md) を参照してください。
変更内容に対応する確認項目は [公開前のチェックリスト](RELEASE-CHECKLIST.md) にまとめています。
ソース、生成素材、ビルド結果、実機での動作確認を分けて記録し、PlayData、ブラウザーキャッシュ、
実際の認証情報を公開しないでください。
