# v0.1.0 verification / 検証状況

Results as of **2026-09-23 JST**. The checks listed below passed within their stated scope. This does not mean every feature, device or service has passed.

**2026年9月23日時点の検証結果です。** 以下の確認は記載した範囲で通過しました。自動テストの合格を、すべての機能・機器・外部サービスの動作保証とはしていません。

## Completed / 確認済み

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
| `SkyCorridor-v0.1.0-Windows.zip` | 1,901,348,041 | `85b407efcd793299582686870e406edd231d7fc8fc0909e49048d67e2be92b0c` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-01.zip` | 1,642,626,580 | `b4b6f2a8503b1ba2d458e58e2e1483c624853189a87e5b4af03522696411e04f` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-02.zip` | 1,509,077,651 | `4475670372353c0cd6e029d8a2533ce5095802d586be7453edcaecbf78eee69a` |
| `SkyCorridor-v0.1.0-DevelopmentAssets-03.zip` | 383,368,759 | `9b78247dd33e5e1a49512951e4506a73d8d05a2e13dc9d954d4fa73ab92c67db` |

The automated runtime checks use isolated saves and scripted movement. Restoring an archive, compiling a target and checking a rendered game are distinct acceptance steps. / 自動実行テストは独立したセーブとスクリプト操作を使用します。配布物の復元、コンパイル、実際の描画・操作はそれぞれ別の確認段階です。
