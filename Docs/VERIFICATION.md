# v0.1.0 verification / 検証状況

Status as of **2026-09-23 JST**. **Final validation is incomplete.** A passing automated case does not mean every feature, device or service has passed.

**2026年9月23日現在、最終検証は未完了です。** 以下は実際に確認した範囲です。自動テストの合格を、すべての機能・機器・外部サービスの動作保証とはしていません。

## Completed / 確認済み

| Check / 確認項目 | Result and scope / 結果と範囲 |
| --- | --- |
| Windows distribution / Windows配布物 | Shipping build and packaging passed. A fresh ZIP extraction matched all 328 file hashes. / Shippingビルド・梱包と、新規展開先の328ファイルのハッシュ照合が通過。 |
| Development assets / 開発用素材 | All three ZIPs and all 894 members passed hash readback. / 素材ZIP3本と全894ファイルを展開・ハッシュ照合。 |
| Fresh project restoration / 開発環境の新規復元 | Restored from the source candidate and matching assets; baseline Editor target built with UE 5.8.2. Engine-derived cloud materials were recreated from the installed engine. / ソース候補版と同版素材から復元し、UE 5.8.2の標準構成でEditorをビルド。雲マテリアルは利用者側のエンジンから再生成。 |
| Building edit persistence / 建物編集の保存 | A commandlet added a building, saved a map and reopened it with matching properties. This was not an interactive Editor or visual test. / コマンドレットで建物を追加・保存・再読込し、値の一致を確認。対話式Editor操作・描画確認は別。 |
| Cooked content / クック済み内容 | Official container listings matched 495 game packages to the approved inventory; no unknown game package was found. 505 engine packages were classified separately as runtime dependencies. / 正式なコンテナー一覧のゲーム資産495件が承認台帳に一致。不明なゲーム資産なし。エンジン資産505件は技術的な実行依存として区別。 |
| Internal media controls / 内部メディア操作 | 29 scripted checks passed; actual public YouTube playback was not tested by this case. / 自動29項目通過。公開YouTube動画の実再生を確認するテストではない。 |
| Memories / 痕跡記録 | 83 checks passed; three journal-only restart checks also passed. The latter does not verify saved player position. / 83項目と、記録帳の再起動後復元3項目が通過。記録帳の復元はプレイヤー位置の復元とは別。 |
| Public interiors / 公共施設の探索 | 239 scripted checks passed, including walking, stairs, seats, lifts and discoveries. / 歩行・階段・着席・昇降機・発見記録など自動239項目通過。 |
| Upper railway / 上層電車 | 62 scripted checks passed. / 自動62項目通過。 |
| Original music files / 独自音楽ファイル | Deterministic regeneration and full audio decoding passed. Listening acceptance is separate. / 再生成時の一致と全音声デコードを確認。実際の試聴評価は別。 |
| Optional NVIDIA Editor configuration / 任意のNVIDIA構成 | Editor compilation and linking passed using unmodified stock SDK source. No GPU, gameplay or performance result is implied. / 提供元SDKソースを改変せずEditorをコンパイル・リンク。GPU実動作・ゲームプレイ・性能は未検証。 |

## Pending and not tested / 未完了・未確認

- **Position restart: PENDING.** The first exploration-resume test failed because its test runner auto-started and saved the plaza before requesting Continue. The runner was corrected to use the ordinary title/Continue path. The failed evidence is retained; a corrected independent run is still pending. No game save format or normal launcher behavior was changed.
  **位置の再開は再試験待ちです。** 初回は検証用の自動開始が「続きから」より先に広場を保存したため失敗。検証手順を通常のタイトル画面からの再開へ修正しました。失敗記録を保持しており、修正後の独立した再試験は未完了です。ゲームの保存形式・通常ランチャーは変更していません。
- **Airship traversal and restart: PENDING. / 飛行船の移動・再開：検証中。**
- **Actual YouTube/cinema playback: NOT TESTED. / YouTube・映画館の実際の外部動画再生：未確認。**
- **Interactive Editor and rendered building inspection: PENDING. / 対話式Editor操作と建物の描画確認：未完了。**
- **Optional NVIDIA Shipping compilation: PENDING.** Cooking, packaging and runtime of that optional configuration are not verified. / **任意のNVIDIA構成のShippingコンパイル：未完了。** 同構成のクック・梱包・実動作も未確認です。
- **NOT TESTED:** complete playthrough with physical keyboard/mouse/controller input; listening on speakers/headphones; HDR display output; other GPU configurations; frame-rate/latency benchmarks; live online rooms and voice over external networks.
  **未確認：** 物理的なキーボード・マウス・コントローラーによる通しプレイ、実機での試聴、HDR画面出力、他GPU構成、FPS・遅延測定、実サービスでのオンライン参加・外部回線の音声通信。

The public default is **baseline / TSR**, with experimental online rooms disabled. No minimum or recommended hardware specification has been established.

公開版の標準構成は **baseline / TSR** で、実験的なオンラインルームは初期状態で無効です。最低・推奨ハードウェア要件は未確定です。

## Test machine / 検証に用いた実機

Windows 11 Home Insider Preview **build 26220**; **AMD Ryzen 7 9800X3D**; **NVIDIA GeForce RTX 5070 Ti**; **31.4 GiB RAM**; display-driver version **32.0.16.1692**.

This identifies the test machine only. It is not a minimum specification or a performance guarantee. / 検証環境の記録であり、最低仕様や性能保証ではありません。

## Verified archive identities / 照合した配布ファイル

These identities apply to the checked archives. The final source archive will be identified separately after documentation and validation are finalized. / 以下は照合済みアーカイブです。最終ソースZIPの識別情報は文書・検証の確定後に別途提示します。

| Archive | Bytes | SHA-256 |
| --- | ---: | --- |
| `SkyCorridor-v0.1.0-Windows.zip` | 1,901,347,699 | `59c0327fcf07cb90dcd23582b9e4fce81c526c4667029f363cd54e4a6a02b0a5` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-01.zip` | 1,642,626,580 | `b4b6f2a8503b1ba2d458e58e2e1483c624853189a87e5b4af03522696411e04f` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-02.zip` | 1,509,077,651 | `4475670372353c0cd6e029d8a2533ce5095802d586be7453edcaecbf78eee69a` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-03.zip` | 383,368,759 | `9b78247dd33e5e1a49512951e4506a73d8d05a2e13dc9d954d4fa73ab92c67db` |

The automated runtime checks use isolated saves and scripted movement. Restoring an archive, compiling a target and checking a rendered game are distinct acceptance steps. / 自動実行テストは独立したセーブとスクリプト操作を使用します。配布物の復元、コンパイル、実際の描画・操作はそれぞれ別の確認段階です。
