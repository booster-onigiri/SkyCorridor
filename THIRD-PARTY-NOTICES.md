# Third-party components / 第三者コンポーネント

Project licenses cover project-owned original work only. Existing third-party
headers, attribution and license files must be retained. Installing, building or
packaging this project does not relicense its technical dependencies.

| Component | Use and distribution boundary | Applicable notices |
|---|---|---|
| Unreal Engine 5.8.2 | Installed separately for development; authorized runtime components are staged by Unreal packaging | Epic's Unreal Engine terms and the engine/runtime notices supplied with that installation and package |
| Epic Online Services / EOS Voice | Engine-supplied SDK integrations; project online features are disabled by default | Epic's SDK/service terms and included SDK notices; developers supply their own application configuration |
| CEF / Chromium and dependencies | Engine-supplied browser runtime for the packaged game | CEF/Chromium and bundled dependency notices supplied with the engine/browser runtime |
| Resonance Audio | Engine-supplied spatialization and room-acoustics implementation | [Apache 2.0 license](Licenses/ResonanceAudio-LICENSE.txt) and [upstream copyright notice](Licenses/ResonanceAudio-COPYRIGHT-NOTICE.txt); original engine component terms remain applicable |
| SADIE HRTF data | Technical directional-acoustics data used by Resonance Audio; not project-authored music or artwork | [Apache 2.0 license](Licenses/SADIE-HRTF-LICENSE.txt) |
| PFFFT / FFTPACK | FFT implementation within the engine's Resonance Audio library | [Unmodified upstream copyright, redistribution conditions and disclaimer](Licenses/PFFFT-FFTPACK-NOTICE.txt) |
| Eigen 3.4.0 | Engine linear algebra dependency used by Resonance Audio; covered files retain MPL 2.0 rights | [MPL 2.0 text](Licenses/Eigen_License.txt) and [corresponding source, hashes and individual Apache/BSD notices](ThirdPartySources/Eigen/README.md); the project's noncommercial terms do not restrict recipients' rights in Eigen |
| Ogg / Vorbis | Engine audio codec libraries; Win64 module rules select libogg 1.2.2 and libvorbis 1.3.2 | [Engine-provided Ogg/Vorbis notices](Licenses/OggVorbis1.2.0_License.txt) |
| Droid / Roboto / Noto fonts | DroidSansFallback is included in project Content; engine Slate supplies default and fallback typefaces | [Android/Droid](Licenses/AndroidOpenSourceProject_License.txt), [Roboto](Licenses/ROBOTO_License.txt), [Noto](Licenses/Noto_License.txt) |
| FreeType 2.14.1 / HarfBuzz 2.4.0 | Engine font rasterization and text shaping; these are the installed Win64 module selections | [FreeType](Licenses/Freetype2.14.1_License.TXT), [HarfBuzz](Licenses/HarfBuzz_2-4-0_License.txt), [UCDN](Licenses/HarfBuzz_UCDN_License.txt) |
| SDL3 | Controller integration; third-party headers, library and runtime retain their upstream terms | [SDL3 license](Licenses/SDL3-LICENSE.txt), also retained beside `Project/Plugins/EWGamepad/ThirdParty/SDL3` |
| NVIDIA DLSS / Streamline / Reflex | Optional local SDK installation; vendor SDK source is not part of the public repository | Vendor licenses accompanying the official SDK archives and any permitted redistributed binaries; [setup notes](Docs/NVIDIA.md) |
| Python / NumPy | Development and asset generation; installed separately | Their upstream terms; the music generator pins NumPy 2.3.5, licensed BSD-3-Clause |
| Blender 4.5 | Optional source-art regeneration; installed separately | Blender's upstream license; the Blender application is not included |
| FFmpeg | Optional media verification/remux tooling; installed separately | The selected FFmpeg distribution's own licenses; no FFmpeg executable is included |

Node.js, `ws` and `cloudflared` are **not bundled in the default release**. If you
independently provision an experimental room-host environment, its dependencies
retain their own notices and licenses. Do not add unrelated binaries or credentials
to a public package. See [experimental online features](Docs/ONLINE.md).

The public procedural soundtrack contains no recorded instrument samples and no
external impulse responses. Its source and provenance are included in the audio
tools and matching source-art archive. External content viewed through YouTube or
other supported browser pages remains the property of its rights holders and is
not supplied or licensed as a project asset.

This statement about the composed soundtrack does not describe the engine's audio
middleware: Resonance Audio uses third-party SADIE head-related transfer-function
data for spatial sound. That technical dependency retains its own terms and is
not offered as an original Sky Corridor asset. The Resonance/SADIE license texts
are copied from the installed UE 5.8.2 component; the PFFFT notice reproduces the
complete opening license comment of its `pffft.h` (including FFTPACK attribution).

These notices describe the relevant technical subset used by this project; their
original terms remain unchanged. The project's PolyForm/CC BY-NC terms apply to
project-owned work and do not replace or narrow any third-party license. Engine
font-rendering portions use the FreeType Project and HarfBuzz under the accompanying
terms. Windows system fonts selected through system APIs are not copied into this repository.

The Eigen source subset is supplied with this distribution and at the
[v0.1.0 source location](https://github.com/booster-onigiri/SkyCorridor/tree/v0.1.0/ThirdPartySources/Eigen).
It matches the installed UE 5.8.2 headers byte for byte, including the engine's
existing allocator changes; Sky Corridor has not modified those headers.

## 日本語

本作のライセンスは、プロジェクト独自の著作物に適用します。Unreal Engine、EOS、
CEF/Chromium、SDL3、任意導入するNVIDIA SDKなどの利用条件を変更するものではありません。
第三者のソースヘッダー、ライセンス、著作権表記を保持してください。

開発用のエンジン、Python、NumPy、Blender、FFmpegは別途導入するツールです。
配布物に含まれるエンジン・ブラウザー・コントローラー用ランタイムには、各提供元の表記を保持します。
標準配布にはNode.js、`ws`、`cloudflared`を含めません。

公開版の独自BGMは波形・倍音・残響を計算で生成しており、録音済みの楽器サンプルや外部の残響素材は
使用していません。ゲーム内で再生する外部の動画や音楽の権利は各権利者にあり、
本作の素材ライセンスや実況・配信許可の対象ではありません。

立体音響の技術依存であるResonance AudioとSADIE HRTFデータは、独自BGM・独自素材とは
区別します。SADIEは音の方向を表現するための第三者の音響データであり、本作独自の創作素材として
再ライセンスしません。Resonance Audio・SADIEのApache 2.0本文と、PFFFT/FFTPACKの
著作権・再配布条件・免責条項を `Licenses` に保持しています。

Eigen、Ogg/Vorbis、Droid・Roboto・Noto、FreeType、HarfBuzz/UCDNの本文も、今回使用する
技術依存の表記として保持しています。これらは元のライセンスが適用され、本作独自の非商用条件で
上書きしません。OSのAPIで参照するWindowsのシステムフォントは、このリポジトリーへ複製していません。

Eigen の対応ソースは配布物の `ThirdPartySources/Eigen` と上記の v0.1.0 固定 URL に
同梱・掲載します。エンジンに既存の変更を含めて元のヘッダーと完全一致し、
Sky Corridor 独自の変更は加えていません。各ファイルの元の条件を保持しています。
