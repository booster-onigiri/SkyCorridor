# Source art and release assets / 元素材とReleaseアーカイブ

## English

Git contains the editable project source, original generators and import helpers.
Large Unreal **Content** and **SourceArt** files are carried in separately versioned
Release archives. `release-assets.json` is the authoritative file/hash inventory for
the selected tag. `setup.ps1` verifies both archives and extracted files; it preserves
an existing file if its content differs.

| Location | Purpose |
|---|---|
| `Project/Content` | Unreal maps, imported meshes, materials, textures and sound assets |
| `Project/SourceArt` | Editable/generated source assets and provenance in the matching archive |
| `Project/SourceArt/Scripts` | Original art-generation and import scripts retained with source |
| `Tools/Art` | Public-release art import helpers |
| `Tools/Audio` | Original procedural music renderer, verifier and trailer remux tool |

For a normal build, restore the matching archive set rather than running every
historical generator. Asset regeneration is a development operation: use a separate
output directory, inspect the result, then import through Unreal using the
appropriate project helper. Blender-authored work uses Blender 4.5. Do not overwrite
an edited source file simply to make its hash match the release.

The texture generators `build_craft95_paving.py` and `build_quality93_textures.py`
also import NumPy and Pillow (`PIL`); install those separately before using these
optional scripts. The archive-restoration setup itself uses Python's standard
library and does not require these image-generation dependencies.

### Procedural soundtrack

The public edition keeps the three original score sequences:
**CanalAfterglow / 水路のあと**, **WindowWithoutVoices / 声のない窓**, and
**LampOnTheWayHome / 帰り道の灯**. Soft modal tones, an additive harmonic halo,
and a generated diffuse room supply every sound. There are no recorded instrument
samples or external impulse responses.

```powershell
python -m pip install -r Tools/Audio/requirements.txt
python Tools/Audio/compose_music89.py --output Local/Audio-preview
python Tools/Audio/verify_audio89.py --masters Local/Audio-preview --report Local/audio-preview-verification.json
```

Rendering uses NumPy 2.3.5 and Python's standard library. Verification also requires
FFmpeg on PATH. The renderer refuses existing output files; choose a fresh directory
for another run. It writes 48 kHz / 16-bit stereo WAVs, complete note events, fixed
seeds, runtime information, provenance, and SHA-256 hashes. The verifier independently
decodes the WAVs, measures level/clipping and loudness, and checks a complete rerender.
Neither tool plays audio. A measurement report does not establish subjective
listening acceptance.

Original project source follows [LICENSE](../LICENSE); original generated music and
art follow [ASSET-LICENSE](../ASSET-LICENSE). Keep provenance with derivative assets,
provide the required credit, and identify modifications. Third-party tool/runtime
licenses remain separate.

## 日本語

Gitにはプロジェクトのソースと独自の生成・取り込みスクリプトを置きます。
大容量の **Content** と **SourceArt** は同じタグのReleaseアーカイブから復元します。
対象ファイルとハッシュは `release-assets.json` に記録され、`setup.ps1` が検証します。
内容の異なる既存ファイルは上書きしません。

通常のビルドは完成済みの素材を復元して行います。素材を再生成する場合は別の出力先で結果を確認し、
対応するUnreal用の取り込み処理を使用してください。Blenderの元素材にはBlender 4.5を使用します。
リリースのハッシュへ合わせるために、自分の編集済み素材を消さないでください。

`build_craft95_paving.py` と `build_quality93_textures.py` による任意のテクスチャ再生成には、
NumPy と Pillow（`PIL`）も別途必要です。完成済みアーカイブを復元するセットアップは
Python 標準ライブラリーのみで動作します。

公開版BGMは **水路のあと・声のない窓・帰り道の灯** の3曲です。元の音列を維持し、
弦のように減衰する波形、控えめな倍音、計算で生成する残響だけで合成しています。
録音サンプルや外部の残響素材は使いません。上記のコマンドで新しい出力先へ再生成できます。
音源は48 kHz・16 bit・ステレオWAVで、楽譜イベント、乱数シード、出典とハッシュを添えます。
検証処理は音声を再生せず、形式・音量・クリッピング・再生成一致を確認します。
試聴による音質評価とは分けて扱ってください。

独自スクリプトはPolyForm Noncommercial、独自音楽・素材はCC BY-NC 4.0の対象です。
派生素材には出典を残し、クレジットと変更点を示してください。制作ツールやランタイムのライセンスは別途適用されます。
