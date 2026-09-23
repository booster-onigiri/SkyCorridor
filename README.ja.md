# 空の回廊 / Sky Corridor

[English](README.md) · **v0.1.3** · 無料のWindows探索ゲーム試作版 · ゲーム内表示は日本語・英語

誰もいなくなった天空都市。水は流れ、列車は今も走っています。
水路を歩き、上層の街へ移動し、小さな端末でかつての暮らしの痕跡を集めます。

[確認済みの検証結果と未確認の範囲](Docs/VERIFICATION.md)はこちらです。

## 遊ぶ

[Releases](https://github.com/booster-onigiri/SkyCorridor/releases) からWindows版を入手し、
ZIP全体を書き込みできるフォルダーへ展開してください。**Windows** フォルダーと並ぶ
**PLAY.cmd** で起動します。Unreal Editorのインストールは不要です。
保存先はランチャーの隣にある **PlayData** です。更新前にフォルダー全体をバックアップしてください。

**WASD** で歩き、**マウス** で見回し、**E** で使い、**Q** で端末を開きます。
時計広場で **Q → 観測 → 観測を始める** を選ぶと、最初の記録を探せます。
詳しくは [遊び方](Docs/PLAY-JA.md) と [英語の操作案内](Docs/PLAY-EN.md) を参照してください。

公開版の中心は、64bit Windows・DirectX 12環境での一人の探索です。
最低GPU・CPU・メモリー要件は未確定です。タイトル画面または「画質と操作」で日本語・英語を切り替えられます。選択は PlayData/language.txt に保存されます。初回は日本語Windowsなら日本語、それ以外なら英語です。
「街を探す・街を開く」「世界の卵・追加要素」と端末の「友人」は
**開発中**と表示し、標準では選択できません。Tキーからも開きません。
「みんなで映画を見る」は **公開版では利用不可** と表示し、実験用フラグでも有効になりません。
一人用の探索・釣り・撮影は引き続き利用できます。**公開版のゲーム内YouTube再生機能は中止しました。**
広場・映画館・天空のモニターは景観として残り、オンライン動画は再生しません。
以前の紹介映像に映るYouTube再生は過去の試作機能で、v0.1.3では利用できません。

## ソースから作る

**Unreal Engine 5.8.2**、C++ゲーム開発環境を入れたVisual Studio 2022、
Windows SDK **10.0.26100.0**、セットアップ用のPython **3.13** を使用します。
Blenderで制作した素材を再生成するときは **Blender 4.5** も使用します。
セットアップはPython標準ライブラリーで動作し、音楽の再生成だけは
`Tools/Audio/requirements.txt` の固定版NumPyを追加で使用します。

```powershell
git clone --branch v0.1.3 https://github.com/booster-onigiri/SkyCorridor.git
cd SkyCorridor
.\setup.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
.\build.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8" -Target Editor
```

大容量の `Project/Content` と `Project/SourceArt` はReleaseの別アーカイブに収録します。
セットアップが `release-assets.json` に従ってダウンロードし、ハッシュを検証します。
GitHubのソースZIPだけでは素材が揃いません。取得済みのアーカイブを使う場合は
`-AssetsDirectory` を指定してください。素材はこのソースの `release-assets.json` が指定するものを使用します。
v0.1.3は素材を変更していないため、v0.1.0の素材アーカイブを再利用します。
[構築手順](Docs/SETUP.ja.md) と [素材の構成](Docs/ASSETS.md) に詳細があります。

標準のグラフィックス構成は **baseline / TSR** です。NVIDIA SDKプラグインは任意導入で、
公開ソースには含みません。[NVIDIAの導入手順](Docs/NVIDIA.md) に従い、
`EWGraphicsProfile` を明示的に切り替えます。
オンライン機能は [実験用設定](Docs/ONLINE.md) を参照してください。

## 利用条件と実況・配信

本プロジェクト独自のソースコードは [PolyForm Noncommercial 1.0.0](LICENSE)、
独自の画像・モデル・音楽などは [CC BY-NC 4.0](ASSET-LICENSE) で公開します。
ソースを閲覧・利用できる、非商用条件のプロジェクトです。

**自分でプレイしたゲームの録画・配信・収益化は許可します。**
プレイ中に流れる本作独自の音楽も含めた扱いは [実況・配信の許可](GAMEPLAY-VIDEO-PERMISSION.md) を確認してください。
無関係な第三者の動画・音楽には、この許可は及びません。
エンジン・SDK・ライブラリーは各提供元の条件が適用されます。[第三者の表記](THIRD-PARTY-NOTICES.md) を保持してください。
パッケージに含むUnrealの技術の利用範囲とEpicに関する免責は、[製品利用条件](Docs/END-USER-TERMS.md) に記載しています。

Unreal Engineと、Codex上のGPT-6 Astraによる制作支援を使用しています。
[開発への参加](CONTRIBUTING.md)、[構成](Docs/ARCHITECTURE.md)、
[公開前の検証項目](Docs/RELEASE-CHECKLIST.md) も参照できます。
不具合は [Issues](https://github.com/booster-onigiri/SkyCorridor/issues) に、
版・Windows/GPU・再現手順を添えて報告してください。
