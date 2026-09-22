# Release verification / 公開前の検証

This is a **checklist**, not a claim that a particular build passed. Record results
against the exact tag, source commit, asset-manifest hash, toolchain, graphics
profile and packaged archive hash. Publish the applicable evidence with the release.

- [ ] Source and dependency inventory: no personal saves, browser state, real EOS
  configuration, machine-specific credentials, unrelated media or unlicensed assets.
- [ ] License/provenance review: project originals and technical dependencies are
  identified; upstream notices remain intact; gameplay-video permission is included.
- [ ] Fresh asset restoration: download/local-archive paths both use the final
  `release-assets.json`, verify hashes, and preserve differing local files.
- [ ] Editor build using UE 5.8.2 and the documented Windows toolchain.
- [ ] Shipping build/cook/package: inspect process result and material/shader errors.
- [ ] Fresh extraction: launch `PLAY.cmd`, enter the world, move/jump, use the device,
  collect/read a memory, save, exit, relaunch, and check that progress persists.
- [ ] Travel/content smoke check: water city, upper transport, interiors, day/night,
  camera and menus relevant to the release.
- [ ] Packaged YouTube check when claimed: provider access, video, controls and sound.
  Editor/PIE does not substitute for this check.
- [ ] Audio technical checks: full PCM decode, level/clipping, duration, reproducible
  source and correct imported tracks. Track physical listening separately.
- [ ] Final trailer checks: complete decode, dimensions/frame rate/color metadata,
  soundtrack provenance, file hashes and review of the actual final image/audio.
- [ ] Save/update instructions, bilingual guides, licenses, notices and launcher are
  included in the final downloadable package.
- [ ] Optional NVIDIA profile: identify SDK versions and verify separately when included.
- [ ] Experimental online/voice: report separate-device and separate-network results
  only when actually measured. Default release must remain usable without configuration.

Use explicit outcomes such as **PASS**, **FAIL**, **NOT TESTED**, or **NOT MEASURED**.
A compiler result, a prerecorded trailer, a same-machine test and a physical-device
test answer different questions. Do not infer a minimum GPU specification from a
single development PC or an offline-rendered trailer.

## 日本語

これは確認項目の一覧であり、特定のビルドの合格報告ではありません。
タグ、コミット、素材マニフェスト、使用環境、グラフィックス構成、配布ZIPのハッシュを
特定したうえで結果を記録してください。

個人データと権利表記、素材の新規復元、EditorとShippingのビルド、別フォルダーへの新規展開、
起動・移動・端末・記録・保存再開、街の主要経路、パッケージ版YouTube、音源、最終トレーラー、
同梱案内を確認します。NVIDIA構成とオンライン・音声は、それぞれ別に条件を記録して検証します。

**合格・失敗・未確認・未測定** を明示してください。ビルド成功、録画映像、同一PC内の接続、
実機での確認は異なる結果です。開発PC1台の結果やオフラインで描画したトレーラーから、
最低GPU要件を推定して公表しないでください。
