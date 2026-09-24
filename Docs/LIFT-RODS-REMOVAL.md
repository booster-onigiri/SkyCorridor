# v0.1.4 — Floating elevators / 浮遊するエレベーター

[English README](../README.md) · [日本語README](../README.ja.md)

## What changed

The city-generation code no longer places full-height elevator guide masts or
rails across the skyline. This includes city-access elevators, the upper railway,
hotel and Sky Theatre extensions, the airship-port extension, and the water-city
landmark lifts. The airship port's four decorative columns extending down to the
old rooftop are also removed. Cabins now travel with a floating appearance.

Cabin geometry and movement, doors, stop positions, landing floors, guards and
their collision definitions are retained. This is an appearance change; the
generation change does not modify save formats or require a new game. Back up
`PlayData` and extract the new Windows ZIP into a separate folder before moving
your saved progress, as described in [the player guide](PLAY-EN.md).

No new art asset is added. The source tag continues to use the v0.1.0 development
assets pinned by `release-assets.json`. The removed mast asset may remain in those
archives for source compatibility; the public city generator no longer places it.
The public YouTube playback removal and disabled experimental menu items are retained.

The build workflow also prepares engine-derived cloud materials for Shipping as
well as Editor. A direct Shipping build from freshly restored source and assets
now runs the same preparation before cooking. These material sources remain
excluded from downloadable source/assets and are generated using the installed
Unreal Engine; see [the build instructions](SETUP.md).

## 変更内容

都市の生成処理から、空を縦断するエレベーターのガイド支柱・レールを取り除きました。
都市内の昇降機、上層線、ホテル・天空シアター・空中港への延長部、水都のランドマークの
昇降機が対象です。空中港から旧屋上へ延びていた装飾支柱4本も除去し、かごが浮遊する外観にしました。

かごの形状・移動、扉、停止位置、乗り場の床、安全柵とその当たり判定の定義は維持しています。
外観の変更で、保存形式の変更や最初からのやり直しはありません。
[遊び方](PLAY-JA.md)に従って `PlayData` をバックアップし、更新版は別フォルダーへ展開してください。

新しい美術素材は追加していません。開発用素材は `release-assets.json` で固定している
v0.1.0のアーカイブを継続使用します。元の支柱素材が互換性のためアーカイブに残っていても、
公開版の都市生成では配置しません。YouTube再生の公開中止と、実験機能のメニュー無効化も維持します。

また、エンジン由来の雲マテリアルの準備処理を、EditorだけでなくShippingビルドにも適用しました。
新たに復元したソース・素材から直接Shippingを作る場合も、Cook前に雲を生成します。
雲マテリアルのソースは配布アーカイブへ含めず、導入済みUnreal Engineから生成する方針を維持します。
詳しくは[構築手順](SETUP.ja.md)を参照してください。

## Verification / 検証

The corrected public candidate passed a fresh build/cook/package, **62 scripted
upper-rail checks**, sampled hotel/theatre/airport image review, the **257-file**
Windows inventory, **495-package** cooked-content matching, and full Windows ZIP
member-hash readback. The first candidate was rejected for two missing cloud
materials and was not published; its failure evidence is retained.

The same executable also passed **270 localization**, **18 restart/language
preference**, **58 public-menu** and **16 playback-removal checks**. See
[VERIFICATION.md](VERIFICATION.md) and [the receipt](FLOATING-LIFTS-VERIFICATION.json)
for exact scope and identities. These local checks do not establish manual full
gameplay, physical HDR/audio, rebasing stress or public-download acceptance.

修正後の公開候補版は、新規ビルド・Cook・梱包、**上層線の自動62項目**、ホテル・シアター・空中港の
抽出画面確認、**配布257ファイル**と**Cook済み資産495件**の照合、Windows ZIP全メンバーの
ハッシュ読み戻しを通過しました。雲マテリアル2件が欠けていた初回候補は不合格として公開せず、記録を保持しています。

同じ実行版で**言語表示270項目・再起動と言語保存18項目・公開メニュー58項目・再生無効化16項目**も
通過しました。詳細な範囲とファイル識別情報は
[検証状況](VERIFICATION.md)と[検証記録](FLOATING-LIFTS-VERIFICATION.json)を参照してください。
ローカルの検証であり、手動の通しプレイ・実HDR/音声・原点移動の負荷検証・公開先からの取得合格を示すものではありません。
