# Verification / 検証状況

Results as of **2026-09-24 JST**. The checks below passed within their stated scope; untested areas are identified separately. This does not mean every feature, device or service has passed.

**2026年9月24日時点の検証結果です。** 以下の確認は記載した範囲で通過し、未検証事項は区別しています。自動テストの合格を、すべての機能・機器・外部サービスの動作保証とはしていません。

## v0.1.4 floating elevators / 昇降機の長い支柱を除去

The city-generation change removes full-height elevator guide masts and the airship
port's four decorative support columns. Cabin movement, doors, stops, landing floors,
guards and their collision definitions are retained. See [the change details](LIFT-RODS-REMOVAL.md).

The corrected candidate passed Editor compilation, local engine-derived cloud
material preparation, and a fresh Shipping build/cook/package. Its exact Shipping
executable SHA-256 is `78190a14f1d4e7e07c0533e5f8bfdd12e0de626c29192519f4e287bbceef61f5`.
The cloud-preparation report confirmed success and a matching generation recipe.

The packaged upper-rail audit passed **62 scripted checks** with an isolated save,
covering all eight hotel rooms, the Sky Theatre seat, the skyport and the return
route. It recorded **1,681.48 m** walked and **zero fall recoveries** over a
**776.96-second audit** (784.81 seconds including the runner). Hotel 101, theatre
and airport captures were visually reviewed; clouds were present. This run recorded
**zero world-origin rebases**, so it does not establish rebasing coverage.

Browser exclusion checks passed. The local Windows-package inventory passed all
**257 files**, and the cooked-container audit matched **495 of 495 approved game
packages**, with no unknown, missing or DevValidation game package. The existing
`M_CloudLayers` cooked chunk differs from the reference; package-list matching
does not establish source-content identity or visual equivalence. The final
Windows ZIP passed full member-hash readback; its identity is listed below.
See [the sanitized receipt](FLOATING-LIFTS-VERIFICATION.json).

The same Shipping executable also passed **270 localization**, **18 restart/language
preference**, **58 public-menu**, and **16 playback-removal checks** with isolated
saves. All four runs observed **zero browser helper processes** and no
`MonitorBrowser` directories in the inspected locations. These checks retain the
public release's disabled playback and experimental menus; they do not certify
zero whole-system network activity.

These are local scripted checks and sampled image review, not a manual full
playthrough, physical listening/HDR verification, or a full network capture.
Broader historical checks below were not rerun in full. Public-download verification
is separate and is recorded in the release notes after publication.

The first candidate compiled and packaged, but the cooked-content audit found
**493 of 495 expected game packages**: engine-derived `M_CloudLayers` and
`MI_CloudSea` were missing. **That candidate was rejected and was not published.**
The initial failure evidence is retained. `build.ps1` now prepares those materials
from the installed Unreal Engine for Shipping as well as Editor. The successful
corrected candidate above was cooked and packaged again after that repair.

都市を縦断する昇降機のガイド支柱と、空中港の装飾支柱4本を除去しました。
かごの移動・扉・停止階・乗り場の床・安全柵とその当たり判定の定義は維持しています。

修正後の候補版は、Editorのコンパイル、エンジン由来の雲マテリアルのローカル生成、
Shippingのビルド・新規Cook・梱包を通過しました。雲の生成記録も成功と生成条件の一致を示しています。
実行ファイルのSHA-256は上記のとおりです。

配布版を使う上層線の検証では、独立したセーブで**自動62項目**が通過しました。
ホテル全8室、天空シアターの座席、空中港、帰路を含み、移動距離**1,681.48 m**、
**落下復旧0回**、検証内の時間**776.96秒**（実行手順を含めると784.81秒）でした。
ホテル101号室・シアター・空中港の描画画像3枚を目視確認し、雲の表示も確認しました。
今回は**原点移動0回**で、原点移動の検証まで通過したという意味ではありません。

ブラウザー除外の検査、Windows配布物**257ファイル**の台帳検査、Cook済み資産**495/495件**の
照合が通過しました。不明・欠落資産とDevValidationの混入はありません。
既存の `M_CloudLayers` のCook済みチャンクは参照版と異なります。一覧照合は元素材の同一性や
見た目の同等性まで保証しません。最終Windows ZIPは全メンバーのハッシュ読み戻しを通過し、
識別情報を下表に記載しています。

同じShipping実行版で、独立したセーブによる**言語表示270項目・再起動と言語保存18項目・
公開メニュー58項目・再生無効化16項目**も通過しました。4回の実行で観測したブラウザー補助プロセスは
**0件**、検査先の `MonitorBrowser` フォルダーも**0件**でした。公開版の再生機能と実験メニューの
無効化を維持する検査であり、PC全体の通信が0であることの証明ではありません。

今回の結果はローカルの自動操作と抽出画面の確認であり、
手動の通しプレイ、実音声・HDR画面、全通信の取得とは別です。以下の旧版の広範な検証をすべて
再実行したものではありません。公開先からの取得確認は、公開後のリリースノートへ別途記録します。

最初の候補版はコンパイル・梱包まで完了しましたが、Cook済み資産の検査で期待する495件のうち
**493件**しかなく、エンジン由来の `M_CloudLayers` と `MI_CloudSea` の欠落を検出しました。
**この候補版は不合格として公開せず、初回の失敗記録を保持しています。** `build.ps1` を修正し、
ShippingでもEditorと同じ雲マテリアル生成を導入済みUnreal Engineから行うようにしました。
修正後に新たにCook・梱包した候補版が、上記の検証を通過しました。

## v0.1.3 playback removal / 公開版の動画再生中止

In-game YouTube playback is discontinued; world screens remain scenery. The exact
packaged Shipping executable passed **270 localization**, **18 restart/language preference**,
**58 public-menu**, and **16 playback-removal checks** using isolated saves. Its SHA-256 is
`721cbc6ac50db0eb35ea39e9c38714a3c231d32d9627c86b04733a3e43f5164e`.

The checks exercise production Slate labels and disabled states, language switching,
local apps and memory recording, and direct requests to the three world screens and
handheld video surface. Browser initialization, search, resume, synchronized playback
and diagnostic playback remain inert. The title, phone unavailable screen and plaza
tablet were visually reviewed for readable unavailable states. These are scripted
game checks and sampled image review, not physical keyboard/mouse operation or a full playthrough.

Generated Editor/Shipping compile definitions both disable YouTube. The Shipping target
receipt and staged/archive filename inspection found no browser runtime dependencies
or runtime files. All four audit runs observed zero browser helper processes and no
MonitorBrowser directories in their inspected locations. This is **not a full network
capture** or proof that all game/OS network activity is zero. An earlier filename scan
flagged the retained CEF license notice; the corrected scan distinguishes notices from
runtime files, and the original diagnostic is retained.

The local packaged-file audit passed **257 files**. Official cooked-container listings
matched **495 approved game packages**, with no unknown/missing game package or
DevValidation package. Container bytes matched between stage and archive. The existing
`M_CloudLayers` cooked chunk differs from the reference; the listings do not establish
source-content identity or visual equivalence. See the sanitized
[playback-removal receipt](PLAYBACK-REMOVAL-VERIFICATION.json).

公開版のYouTube再生は中止し、モニターは景観として残します。上記SHA-256のShipping実行版で、
独立したセーブを使い、**言語表示270項目・再起動と言語保存18項目・公開メニュー58項目・
再生無効化16項目**が通過しました。実際のSlateの表示と無効状態、言語切替、探索用アプリと
痕跡の記録を確認しました。常設モニター3台とスマホへの直接要求でも、ブラウザー初期化・検索・
再開・同期再生・診断再生は作動しません。タイトル・スマホの利用不可画面・広場端末の描画画像で
表示の読みやすさを確認しました。物理入力による操作や全編の手動プレイを確認したものではありません。

Editor・Shippingの生成済み定義はYouTubeを無効化し、Shippingの依存情報とステージ・梱包先の
ファイル名検査でブラウザーの実行依存・実行ファイルは検出されませんでした。4回の実行で観測した
ブラウザー補助プロセスは0件、検査先のMonitorBrowserフォルダーも0件です。全通信の取得や
PC全体の通信が0であることを示す検査ではありません。最初のファイル名検査は保持したCEFの
ライセンス文書を検出したため、実行部品と表記を区別する検査へ修正しました。初回の記録も保持しています。

ローカルの梱包先**257ファイル**の監査と、Cook済みゲーム資産**495件**の一覧照合が通過しました。
不明・欠落資産とDevValidationの混入はありません。ステージと梱包先のコンテナーは一致しました。
既存の`M_CloudLayers`のCook済みチャンクには参照版との差異があります。一覧照合は元素材の同一性や
見た目の同等性まで保証するものではありません。

The final Windows ZIP passed readback of all **257 members**; its identity is listed below.
This section covers the local candidate, before public-download verification. Broader
gameplay, physical audio/HDR and external-network results were not rerun here. The older
media/browser results and JSON receipts below remain **historical evidence**, not current
playback support or a compliance approval. / 最終Windows ZIPの**257件すべて**の読み戻しが通過しました。
識別情報は下表に記載します。この節は公開先からの取得確認前のローカル候補版の結果です。
広範なゲームプレイ、実音声・HDR、外部回線は今回再検証していません。以下の旧版メディア検証とJSON記録は
**過去の事実**として保持し、現行版の再生対応や規約上の承認を示すものとはしません。

## v0.1.2 English/Japanese update / 日本語・英語対応の確認

The standard Editor and Shipping targets compiled with UE 5.8.2, followed by a fresh cook/package. The final Shipping executable passed **256 localization checks**, **17 fresh-process language preference checks**, and **55 public-menu regression checks**, all with isolated saves.

Coverage includes the real title/settings language buttons, phone home/map/records, all 15 memory texts and directions, hotels, rail and loaded elevator destinations, fishing descriptions, live language switching on the three persistent media tablets, and recording the first memory through the normal observation/save path. Rendered title, settings, phone and tablet images were reviewed. The English selection survived restart without a command-line language override. Experimental online/voice features remained inactive.

UE 5.8.2の標準構成でEditor・Shippingをビルドし、新たにCook・梱包しました。最終実行版で**言語表示256項目・別プロセス再起動17項目・公開メニュー回帰55項目**が通過しました。検証用セーブは既存データから隔離しています。

タイトルと設定の実際の言語ボタン、スマホのホーム・地図・記録、痕跡15件の本文と案内、ホテル、電車、読込み済み昇降機の行き先、釣り、常設の再生端末3台の日英切替を確認しました。最初の痕跡は通常の観察・記録処理を通して保存しています。描画画像を目視確認し、言語の起動引数を使わない再起動でも英語設定が保持されました。オンラインと音声は起動していません。

All 328 distribution files passed the stage/hash audit. Cooked content matched all 495 approved game packages, with no unknown/missing package or DevValidation building. No new creative asset was added; the development asset archives remain pinned to v0.1.0. See [the localization receipt](LOCALIZATION-VERIFICATION.json).

配布328ファイルとCook済み資産495件の台帳・ハッシュ照合が通過しました。不明・欠落資産、検証用建物の混入はありません。新しい創作素材は追加せず、開発用素材はv0.1.0を継続使用します。

The broader gameplay and optional NVIDIA results below belong to earlier releases and were not rerun in full. Physical input, live online/voice and external-video playback are not covered by the localization checks. External YouTube content and experimental developer tools are outside the translation scope.

下記の広範なゲームプレイ・任意NVIDIA構成の検証は過去の版の結果です。今回の言語検証で全項目を再実行したものではありません。物理入力、オンライン・音声、外部動画の実再生は対象外です。外部YouTubeコンテンツと開発用実験機能は翻訳範囲に含みません。

## v0.1.1 menu update / メニュー更新の確認

The baseline Editor and Shipping targets compiled and linked, followed by a fresh
Shipping cook/package. The actual Shipping executable passed **55 scripted checks**
using isolated saves: production Slate button labels/enabled states on title and
pause menus, the phone Friends app, blocked direct menu/page/invite requests, and
preservation of photo mode and working local app menus. Four rendered images were
captured; the title and phone screen were visually reviewed. EOS login/voice and
cinema hosting remained inactive. Physical key presses and the online/workshop
developer opt-in modes were not exercised.

標準構成のEditorとShippingをコンパイル・リンクし、新たにCook・梱包しました。
実行版で、タイトル・ポーズ・スマホの実ウィジェットを含む**自動55項目**が通過しました。
「開発中」の表示と無効化、直接メニュー・友人ページ・招待操作の拒否、撮影モードや
一人用アプリの画面を維持することを確認しました。4枚の描画画像を保存し、タイトルと
端末画面を目視確認しました。EOSの接続・音声と共有映画館ホストは作動していません。
物理キーの入力や、開発用フラグを付けた実験機能の動作は今回の対象外です。

All **328 packaged files** passed the staged allowlist and hash audit. Official
container listings matched all **495 approved game packages**; unknown/missing
packages and the isolated DevValidation building were absent. One cooked export
hash differs from the original baseline: `M_CloudLayers`, regenerated by the
existing engine-material preparation helper. No new art source was added.
Unchanged development asset archives remain pinned to v0.1.0 in the manifest.

配布328ファイルの台帳・ハッシュ検査と、Cook済みゲーム資産495件の照合が通過しました。
不明・欠落資産と検証用建物の混入はありません。既存の準備スクリプトが再生成する
雲マテリアル `M_CloudLayers` のCook済みハッシュ1件に旧版との差異があります。
新しい美術素材は追加していません。開発素材はv0.1.0のアーカイブを継続使用します。

See [the 55-check menu receipt](MENU-VERIFICATION.json). The broader gameplay results
below belong to **v0.1.0** and were not rerun in full for this menu-only update.
以下の広範なゲームプレイ検証は**v0.1.0時点**の結果であり、今回すべてを再実行したという意味ではありません。

## v0.1.0 baseline / 旧版で確認済み


| Check / 確認項目 | Result and scope / 結果と範囲 |
| --- | --- |
| Windows distribution / Windows配布物 | Shipping build and packaging passed. The final ZIP passed audit and readback of all 328 member hashes. Only the two player guides changed from the tested runtime package; all other 326 files, including executable and content containers, are unchanged. / Shippingビルド・梱包と、最終ZIPの監査・全328ファイルのハッシュ読み戻しが通過。実行検証版との差分は遊び方2文書のみ。実行ファイル・コンテンツを含む残り326ファイルは同一。 |
| Development assets / 開発用素材 | All three ZIPs and all 894 members passed hash readback. / 素材ZIP3本と全894ファイルを展開・ハッシュ照合。 |
| Fresh project restoration / 開発環境の新規復元 | Restored from the source candidate and matching assets; baseline Editor and Shipping targets built with UE 5.8.2. The fresh project completed cooking, staging and packaging. Engine-derived cloud materials were recreated from the installed engine. / ソース候補版と同版素材から復元し、UE 5.8.2の標準構成でEditorとShippingをビルド。復元先でクック・ステージング・梱包まで通過。雲マテリアルは利用者側のエンジンから再生成。 |
| Building edit persistence and rendering / 建物編集の保存と描画 | A commandlet added a building, saved a map and reopened it with matching properties. The actual non-commandlet Editor then reopened that map using D3D12, read back the saved actor and rendered a reviewed 1280×720 viewport. This covers an isolated building, not a complete procedural-city edit or manual mouse/keyboard operation. / コマンドレットで建物を追加・保存・再読込。その後、実際のEditorをD3D12で起動して保存済み建物を読み戻し、1280×720の描画画像を確認。単独の建物を対象とし、都市全体の編集や手動のマウス・キーボード操作の確認ではない。 |
| Cooked content / クック済み内容 | Official container listings matched 495 game packages to the approved inventory; no unknown game package was found. 505 engine packages were classified separately as runtime dependencies. / 正式なコンテナー一覧のゲーム資産495件が承認台帳に一致。不明なゲーム資産なし。エンジン資産505件は技術的な実行依存として区別。 |
| Internal media controls / 内部メディア操作 | 29 scripted checks passed; actual public YouTube playback was not tested by this case. / 自動29項目通過。公開YouTube動画の実再生を確認するテストではない。 |
| Cinema, clock and lighting / 映画館・時計・照明 | 236 checks passed: the cinema route, 32 seats and sightlines, real-time clock configuration, 30-minute-day setting and sampled lighting states. Synthetic browser tones measured in the audio submix confirmed left/right direction and zero measured output/queued audio at the tested outside boundaries. This does not establish external-video playback or physical listening quality. / 動線・32席と見通し・実時刻時計・1日30分の設定・各時間帯の照明を含む236項目通過。ブラウザーの試験音を音声ミキサーで測定し、左右方向と検査した室外境界での出力・音声キュー0を確認。外部動画再生や実際の試聴品質を証明するものではない。 |
| Actual YouTube playback / YouTubeの実再生 | One user-provided video passed a silent packaged-browser test: 2160p was selected, 3840×2160 source frames were decoded and playback advanced to 3.97 seconds without an ad. The world-screen texture remains 1280×720. This does not verify every video, provider, physical audio or YouTube HDR processing. / 提供された動画1本を配布版ブラウザーで無音検証。2160pの選択、3840×2160ソースのデコード、広告なしで3.97秒まで再生を確認。ゲーム内スクリーンの表示テクスチャは1280×720。すべての動画・配信元、実音声、YouTubeのHDR処理の確認ではない。 |
| Memories / 痕跡記録 | 83 checks passed; three journal-only restart checks also passed. The latter does not verify saved player position. / 83項目と、記録帳の再起動後復元3項目が通過。記録帳の復元はプレイヤー位置の復元とは別。 |
| Public interiors / 公共施設の探索 | 239 scripted checks passed, including walking, stairs, seats, lifts and discoveries. / 歩行・階段・着席・昇降機・発見記録など自動239項目通過。 |
| Saved position restart / 保存位置からの再開 | A fresh exploration run passed 239 checks; its restart through the normal title/Continue path passed 22 checks. / 新規の探索239項目と、その保存データを通常のタイトル画面から「続きから」で再開する22項目が通過。 |
| Upper railway / 上層電車 | 62 scripted checks passed; this run does not establish world-origin-rebasing stress coverage. / 自動62項目通過。この実行で原点移動の負荷検証まで通過したとはしていない。 |
| Airships and safe restart / 飛行船と安全な再開 | 69 checks passed, including boarding, a full 245-second flight, eight world-origin rebases, disembarking and boarding the second ship. Maximum local drift was 7.57 cm with zero fall recoveries. Two restart checks restored a moving-ship save to the reachable rooftop port with a physical floor. / 乗船・245秒の一周飛行・原点移動8回・下船・2隻目への乗船を含む69項目通過。相対位置のずれは最大7.57cm、落下復旧0回。再開2項目では飛行中の保存を足場のある屋上港へ安全に復元。 |
| Original music files / 独自音楽ファイル | Deterministic regeneration and full audio decoding passed. Listening acceptance is separate. / 再生成時の一致と全音声デコードを確認。実際の試聴評価は別。 |
| Downloadable trailer / 配布用紹介動画 | The 3840×2160, 60 fps, 10-bit BT.2020/PQ file passed full decoding of 3,624 frames and audio, frame-timing checks and sampled visual review. The airship shot was recaptured from the public assets. Physical HDR-display and YouTube HDR-transcode acceptance remain separate. / 3840×2160・60fps・10bit BT.2020/PQの動画で、全3,624フレームと音声のデコード、時刻間隔、抽出画面の確認が通過。飛行船カットは公開版素材で再撮影。実HDRディスプレイとYouTubeのHDR変換は別途未確認。 |
| Optional NVIDIA configuration / 任意のNVIDIA構成 | Editor and Shipping compilation and linking passed using unmodified stock SDK source. Optional-configuration cooking, packaging, feature runtime and performance were not tested. / 提供元SDKソースを改変せずEditorとShippingをコンパイル・リンク。同構成のクック・梱包・機能の実動作・性能は未検証。 |

## Known limits and not tested / 確認範囲の限界・未確認事項

- **Manual Editor operation and full procedural-city editing: NOT TESTED.** The saved-building render above is narrower in scope. / **手動Editor操作・都市全体の編集：未確認。** 上記の保存済み建物の描画確認とは範囲が異なります。
- **NOT TESTED:** complete playthrough with physical keyboard/mouse/controller input; listening on speakers/headphones; HDR display output; other GPU configurations; frame-rate/latency benchmarks; live online rooms and voice over external networks.
  **未確認：** 物理的なキーボード・マウス・コントローラーによる通しプレイ、実機での試聴、HDR画面出力、他GPU構成、FPS・遅延測定、実サービスでのオンライン参加・外部回線の音声通信。

The public default is **baseline / TSR**, with experimental online rooms disabled. No minimum or recommended hardware specification has been established.

公開版の標準構成は **baseline / TSR** で、実験的なオンラインルームは初期状態で無効です。最低・推奨ハードウェア要件は未確定です。

The original failed test receipts are retained. The exploration-resume runner initially auto-started and overwrote its own isolated position fixture before Continue; the corrected title/Continue run passed. The first airship runner omitted the original test's forced-rebasing prerequisite; the corrected full-flight and restart cases also passed. These are test-runner changes, with no change to the game save format or normal launcher behavior.

初回の失敗記録も保持しています。位置再開では検証用の自動開始が独立したテスト用位置データを先に保存していましたが、通常のタイトル画面から再開する修正後の実行は通過しました。飛行船では元の監査に必要な強制原点移動の指定が抜けており、修正後の一周飛行・再開も通過しました。変更は検証手順のみで、ゲームの保存形式・通常ランチャーは変更していません。

## Test machine / 検証に用いた実機

Windows 11 Home Insider Preview **build 26220**; **AMD Ryzen 7 9800X3D**; **NVIDIA GeForce RTX 5070 Ti**; **31.4 GiB RAM**; display-driver version **32.0.16.1692**.

This identifies the test machine only. It is not a minimum specification or a performance guarantee. / 検証環境の記録であり、最低仕様や性能保証ではありません。

## Verified archive identities / 照合した配布ファイル

These identities apply to the locally checked final Windows archive and development assets. See the release's `SHA256SUMS.txt` for the source archive and other published files. This report does not certify a download from the public host. / 以下はローカルで照合した最終Windows版と開発用素材です。ソースZIPとその他の配布物はリリースの`SHA256SUMS.txt`を参照してください。この報告は公開ホストからのダウンロード検証を意味しません。

| Archive | Bytes | SHA-256 |
| --- | ---: | --- |
| `SkyCorridor-v0.1.4-Windows.zip` | 1,752,326,357 | `7c12bcbb5e9564c2d067c855e51f9dc69ad6dfef647fcadd0cf7a187722c5070` |
| `SkyCorridor-v0.1.3-Windows.zip` | 1,752,315,384 | `b5e7541c5427fe7032610264fd2a255082400477a62d9dfd67bd656e7e1640bb` |
| `SkyCorridor-v0.1.1-Windows.zip` | 1,901,370,009 | `00fd0934164e3e148c52096a86d62a32c5ac10d59d45faa18225cab89a06a2eb` |
| `SkyCorridor-v0.1.0-Windows.zip` | 1,901,348,041 | `85b407efcd793299582686870e406edd231d7fc8fc0909e49048d67e2be92b0c` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-01.zip` | 1,642,626,580 | `b4b6f2a8503b1ba2d458e58e2e1483c624853189a87e5b4af03522696411e04f` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-02.zip` | 1,509,077,651 | `4475670372353c0cd6e029d8a2533ce5095802d586be7453edcaecbf78eee69a` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-03.zip` | 383,368,759 | `9b78247dd33e5e1a49512951e4506a73d8d05a2e13dc9d954d4fa73ab92c67db` |

The automated runtime checks use isolated saves and scripted movement. Restoring an archive, compiling a target and checking a rendered game are distinct acceptance steps. / 自動実行テストは独立したセーブとスクリプト操作を使用します。配布物の復元、コンパイル、実際の描画・操作はそれぞれ別の確認段階です。
