# Experimental online features / 実験用オンライン機能

## English

**v0.1.2 defaults to solo play.** The online room and EOS integration code is
available for development experiments. It is not a promise of a hosted service or
verified multiplayer across arbitrary networks.

The source includes a blank `Project/Config/EOS.template.json` with these fields:
`ProductId`, `SandboxId`, `DeploymentId`, `ClientId`, and `ClientSecret`.
Use your **own EOS application's client configuration**. The project supplies no
shared service IDs, client credentials, user accounts or production service.
Keep actual configuration outside the checkout where possible. Never commit portal
account credentials or server administration secrets.

To test a packaged build with your own configuration:

```powershell
.\PLAY.cmd -EWEnableExperimentalOnline -EWEOSConfig="D:\SkyCorridor-private\EOS.json"
```

The explicit command-line config takes precedence over `EW_EOS_CONFIG`, which in
turn can override `Project/Config/EOS.json`. In every case the experimental flag is
required before the EOS connection is configured. `Project/Config/EOS.json` is
ignored by Git; keep `EOS.template.json` blank and do not include real configuration
in source or release archives.

Cinema-room hosting is a separate experimental path. The default distribution
does **not** include Node.js, `ws`, or `cloudflared`, and setup does not install a
tunnel or start a room server. Merely setting the flag does not provision those
dependencies or make hosting ready. Any separately provisioned runtime keeps its
own licensing and operational responsibilities. Solo exploration and packaged
YouTube playback do not require this room-host runtime or EOS credentials.

For a meaningful online test, record the build/profile, separate device and network
conditions, connection/join/leave result, reconnection behavior, and voice/screen
behavior actually observed. A localhost result is not evidence of successful
external-network play. Publish only redacted diagnostics; invitation codes,
configuration, browser cookies and user data are not public test artifacts.

## 日本語

**v0.1.2の標準動作は一人用です。** オンラインルームとEOSのコードは開発者向けの実験用です。
運営サービスの提供や、任意の回線での多人数接続を保証するものではありません。

`Project/Config/EOS.template.json` は空欄の見本です。
`ProductId`、`SandboxId`、`DeploymentId`、`ClientId`、`ClientSecret` には、
**自分で用意したEOSアプリのクライアント設定** を使います。
本プロジェクトの実サービスIDや共通の認証情報は配布しません。
実際の設定は可能ならリポジトリー外へ置き、ポータルのログイン情報やサーバー管理用の秘密を含めないでください。

上記の起動例のように **`-EWEnableExperimentalOnline`** と **`-EWEOSConfig`** を指定します。
設定ファイルの優先順位はコマンドライン、環境変数 `EW_EOS_CONFIG`、
`Project/Config/EOS.json` の順です。EOSの接続設定には実験用フラグも必要です。
`EOS.json` はGit管理対象外とし、テンプレートは空欄を維持します。

シネマルームのホスト機能は別の実験用経路です。標準配布にはNode.js、`ws`、`cloudflared`を
含めず、セットアップがトンネルを作成したりサーバーを起動したりすることもありません。
フラグだけでホスト環境が完成するわけではありません。一人の探索とパッケージ版YouTube再生には、
このホスト用ランタイムやEOS認証情報は不要です。

接続の検証では、版・構成、別端末と回線の条件、参加・退出・再接続、音声・画面の実際の結果を
記録してください。同じPC内の接続だけで、別回線接続の成功とは扱いません。
招待コード、設定、Cookie、ユーザーデータは公開せず、必要な診断情報を伏せ字にして共有してください。

## Developer-only menu access / 開発者向けメニュー

Since v0.1.1, online entry buttons and the Friends app are greyed out and marked
開発中 unless `-EWEnableExperimentalOnline` is explicitly supplied. The same gate
applies to the T shortcut and direct menu requests. The flag alone does not supply
service configuration or establish working multiplayer.

The separate local world-egg/chess proof of concept can be enabled with:

```powershell
.\PLAY.cmd -EWEnableExperimentalWorkshop
```

This enables only the existing fixed world generator and one local chess table.
It does not enable arbitrary plug-ins or shared online add-ons.

v0.1.1以降、通常起動ではオンラインの入口・端末の友人を「開発中」として無効化します。
Tキー・直接メニュー呼出しにも同じ制限を適用します。開発者のオンライン実験では
上記の明示的フラグと別途設定・依存関係が必要です。
世界の卵・チェスの一人用実証には `-EWEnableExperimentalWorkshop` を指定します。
任意プラグインやオンライン共有が完成したという意味ではありません。
