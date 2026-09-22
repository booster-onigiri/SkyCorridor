# Contributing / 開発への参加

Use the matching release tag, [setup guide](Docs/SETUP.md) and **baseline / TSR**
profile first. Small, focused fixes with reproducible evidence are easiest to review.

- Describe the problem, expected behavior, and actual result. Include the release,
  graphics profile, Windows/GPU, and reproduction steps.
- Keep unrelated changes separate. Preserve source files and existing third-party notices.
- Keep personal saves, browser data, credentials, machine paths, SDK archives and
  generated build/cache directories out of commits. `EOS.template.json` stays blank.
- Include only code/assets you have the right to contribute. Identify third-party
  material and its terms explicitly. Original code contributions use the project's
  PolyForm Noncommercial terms; original asset contributions use CC BY-NC 4.0.
  The documented gameplay-video permission should also cover contributed original assets.
- Explain what you actually tested. Separate Editor compilation, packaged launch,
  gameplay, external services and hardware checks. Do not infer multiplayer or
  physical audio/HDR support from a build or automated report.
- For asset changes, include the editable source or generator and update the
  relevant provenance. Large generated Content/SourceArt belongs in versioned
  release archives, with hashes updated by the release maintainer.

Open an issue before a substantial feature or license-sensitive dependency change.
Do not post secrets or personal data in issues. Gameplay reports and Japanese/English
documentation improvements are welcome even without a development environment.

## 日本語

まず同じ版のタグと素材を使い、[構築手順](Docs/SETUP.ja.md) に従って **baseline / TSR** で
確認してください。変更は小さくまとめ、問題・期待する動作・実際の動作・再現手順を記載します。

無関係な変更を混ぜず、元のソースと第三者の表記を保持してください。セーブ、ブラウザーデータ、
認証情報、個人のパス、SDKアーカイブ、ビルド・キャッシュはコミットしません。
`EOS.template.json` は空欄を保ちます。

提供できる権利のあるコード・素材だけを追加し、第三者の素材は出典と条件を明示してください。
独自コードはPolyForm Noncommercial、独自素材はCC BY-NC 4.0と、本作の実況・配信許可に
適合する条件で提供してください。素材の変更には編集可能な元データや生成スクリプトを添えます。

確認した項目と未確認の項目を分けて報告してください。Editorのビルド成功だけでは、
配布版の起動、外部サービス、複数端末接続、実機の音・HDRまで確認したことにはなりません。
大きな機能や依存関係の変更は、先にIssueで相談してください。
