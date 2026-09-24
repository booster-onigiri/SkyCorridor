# HDR validation / HDR検証

## English

**Status: v0.1.5 Shipping04; the reporting user confirmed the appearance issue is resolved on 2026-09-25.** They suggested that enabling RTX HDR may have contributed; causality has not been established. This report separates that user observation from software measurements. It does not establish correctness on every display or imply that RGB channels were inverted.

The update connects HDR brightness to ACES2 reference white, applies the requested peak and UI luminance, and bypasses the SDR-only after-tonemapping material (`M_IllustratedLight`) while HDR is enabled. It retains the saved “Softer colors” preference for returning to SDR; the settings control is labelled “SDR only” and disabled during active HDR. These are specific rendering-path corrections, not proof of the original symptom's cause.

For native HDR, use **Windows HDR On, in-game HDR On, RTX HDR Off for this game**. NVIDIA's RTX HDR requirements call for in-game HDR and Windows Auto HDR to be disabled when using that separate conversion feature ([NVIDIA App FAQ](https://nvidia.custhelp.com/app/answers/detail/a_id/5521/~/nvidia-app-faq)).

Four scripted captures used the same `city-eye` camera, 2560×1440 resolution, noon (12:00), EV 13.2, and native-resolution TSR. DLSS Super Resolution, Frame Generation and Ray Reconstruction were off. All four capture receipts report success. This was a rendered-image comparison, not a walking test, gameplay benchmark or human display acceptance.

| Run | SDR material weight | Slate included | Native DXGI swapchain format | Image median / maximum, nit-equivalent |
| --- | ---: | --- | --- | ---: |
| `SDR01` | 1 | No | `B8G8R8A8_UNORM` | 9.410 / 188.443 |
| `HDR-fixed01` | 0 | No | `R10G10B10A2_UNORM` | 4.301 / 613.363 |
| `HDR-legacy01` | 1 | No | `R10G10B10A2_UNORM` | 4.379 / 620.107 |
| `HDR-slate01` | 0 | Yes | `R10G10B10A2_UNORM` | 4.282 / 609.318 |

`HDR-legacy01` deliberately re-enables the old grade only for comparison. The saved style preference remains on in all four receipts. `HDR-slate01` requests capture through Slate; it does not establish that every menu or UI overlay looks correct.

HDR values come from linear scRGB EXR pixels multiplied by 80 nit/unit. SDR PNG values are normalized to a **203-nit reference white solely for comparison**. These are image-derived values, not measured screen luminance. The approximately 613-nit HDR maximum does not prove the monitor reached that brightness, nor do these distributions prove correct contrast, color or a resolved washed-out appearance. All four metrics report zero nonfinite values.

Native DXGI descriptor readback reports a **10-bit PQ desktop output** (`RGB_FULL_G2084_NONE_P2020`) in every run, including the SDR application run. The OS desktop state and application swapchain format are distinct. The swapchain's **applied color space is `NOT_MEASURED`**: DXGI support queries and output descriptors are not an applied-color-space getter. Pixel encoding at presentation, physical luminance and cross-display visual acceptance remain unverified. The user's resolution report is separate from these measurements.

Shipping04's HDR settings audit recorded **433 passed checks**, and its restart audit **65 passed checks**, with zero failed checks. Each retained three unmeasured checks and the overall status `PARTIAL_NOT_MEASURED`. These cover settings and viewport metadata, not a full display calibration; see [verification details](VERIFICATION.md).

## 日本語

**状態：v0.1.5 Shipping04。2026年9月25日、報告者から見た目の問題が解消したとの確認を受けました。** 本人はRTX HDRを有効にしていた影響かもしれないと述べていますが、因果関係は未確認です。利用者の目視報告と以下のソフトウェア計測を分けて扱い、すべての画面での正常表示や、RGBチャンネルが反転していたことを断定しません。

今回、HDRの明るさをACES2の基準白へ接続し、指定ピーク・UI輝度を適用するよう修正しました。HDR時はSDR用のトーンマップ後材質（`M_IllustratedLight`）を適用しません。「色合いをやわらかく」の保存値は保持し、SDRへ戻すと再適用します。HDR動作中の設定ボタンは「SDR時のみ」と表示して無効化します。これらの修正が元の症状の原因を確定するものではありません。

本作のHDRでは **Windows HDR：オン／ゲーム内HDR：入／このゲームのRTX HDR：オフ** を推奨します。RTX HDRは別の変換機能で、公式の条件は上記FAQを参照してください。

上表は同じカメラ・2560×1440・昼12時・EV 13.2・等倍TSRでの4比較です。SR／FG／RRはオフ。SDRと旧HDR経路の材質重みは1、修正版HDRは0で、各取得記録は成功しています。Slateありの取得も含みますが、全UIの目視確認、歩行試験、性能測定ではありません。

HDRの数値はEXRの線形scRGB値×80 nit、SDRは比較用に白を203 nitへ正規化した画像内の換算値です。修正版の最大約613 nitはディスプレイの実測輝度ではなく、黒浮き・色・コントラストの正常化も証明しません。全4件で非有限値は0でした。

DXGI記述子では、HDRの交換バッファは10-bit `R10G10B10A2_UNORM`、SDRは8-bit `B8G8R8A8_UNORM` でした。OSデスクトップ出力は両方とも10-bit PQで、アプリ側の形式とは別の情報です。交換バッファに**実際に適用された色空間は未測定**です。対応可否の照会や出力記述子では代用できません。利用者の解消報告とは別に、表示画素・物理輝度・他の表示機器での評価は未確認です。

Shipping04のHDR設定検証は**433項目**、再起動検証は**65項目**が通過し、失敗は0でした。各検証には未測定3項目が残り、全体状態は `PARTIAL_NOT_MEASURED` です。設定・viewport情報の検証であり、画面の完全な較正ではありません。詳細は[検証状況](VERIFICATION.md)に記載しています。

## Receipt scope / 記録の範囲

The following are validation-case-relative identifiers, not bundled files or personal machine paths. 各パスは検証ケース内の識別子で、同梱ファイルや個人環境へのリンクではありません。

- `Builds/Shipping04/result.json`: successful local build; recorded before publication.
- `Runtime/HDR04/hdr.json` and `Runtime/HDRResume04/hdr-resume.json`: settings/restart observations, with unmeasured checks retained.
- `Runtime/HDRCompare/{SDR01,HDR-fixed01,HDR-legacy01,HDR-slate01}/capture.json`: camera, lighting, material weights, graphics settings and native descriptors.
- The same four directories' `capture-city-eye.metrics.json`: image-derived luminance statistics.

Capture receipt SHA-256 values / 取得記録のSHA-256:

```text
SDR01         1aa6783c30a89aa1f704bb751f029c2c842be6c138087c3b4ef0465333e6e203
HDR-fixed01   e160784693208b83d5021dc15297e464fcb192b4835844463f4fc431cef203cf
HDR-legacy01  ef7f1a6fd9f17c30753c2e01e9c8e5c5773834fac701fb77c9a32923bcf42f83
HDR-slate01   aa94513ee3c772d2075ec1774b7decdd3c47fbae77abc36d71d99aa38e722c1b
```
