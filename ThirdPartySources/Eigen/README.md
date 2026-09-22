# Eigen source accompanying Sky Corridor v0.1.0

This directory provides the Eigen source corresponding to the **Eigen 3.4.0**
dependency in the installed **Unreal Engine 5.8.2** used for this release. It is
available in the distribution and at the
[versioned source location](https://github.com/booster-onigiri/SkyCorridor/tree/v0.1.0/ThirdPartySources/Eigen).

The 191 headers are exact copies from `Engine/Source/ThirdParty/Eigen/Eigen`.
Resonance Audio includes `Eigen/Core` and `Eigen/Dense`. This subset follows their
quoted includes for the Win64 CPU configuration, retaining generic, SSE, AVX and
AVX512 alternatives. The engine enables `EIGEN_MPL2_ONLY` and its allocator
override. File sizes, SHA-256 hashes, selection evidence and excluded conditional
includes are recorded in [source-manifest.json](source-manifest.json).

Sky Corridor has made **no changes to these headers**. They retain the engine's
existing changes, including the allocator override in
`Eigen/src/Core/util/Memory.h`, whose original MPL 2.0 notice remains intact.
The installed `Macros.h` is also preserved byte for byte. This is the engine's
corresponding source, not a claim that every file is identical to an upstream
Eigen release.

Eigen's covered source is provided under [MPL 2.0](MPL-2.0.txt). Individual notices
are preserved in every copied file. `Eigen/src/Core/arch/Default/BFloat16.h` carries
the TensorFlow Authors' [Apache 2.0 terms](Apache-2.0.txt).
`Eigen/src/Core/util/MKL_support.h` carries
[Intel's BSD notice](Intel-MKL-support-BSD.txt), even though the optional MKL backend
is not enabled. The project's noncommercial licenses do not restrict any rights
granted by these third-party licenses.

This is a source-availability subset for the shipped Windows configuration, not a
standalone SDK. Engine module rules, engine binaries, `unsupported`, sparse/solver
modules outside Core/Dense, non-x86/GPU branches and optional BLAS/LAPACKE/MKL
backend headers are excluded. Their guarded include references remain unchanged.
Development builds use the separately installed engine. Enabling another platform
or backend requires its corresponding source and license review.

## 日本語

UE 5.8.2 のインストールに含まれる **Eigen 3.4.0** のうち、本リリースの
Resonance Audio が参照する Core / Dense の Win64 CPU 用ソースを同梱しています。
191 ヘッダーはエンジン内の元ファイルと完全一致し、各ハッシュと対象範囲を
`source-manifest.json` に記録しています。

Sky Corridor 独自の変更はありません。`Memory.h` のアロケーター処理など、
エンジンに既存の変更と各著作権・ライセンス表示をそのまま保持しています。
MPL 2.0、BFloat16 の Apache 2.0、MKL_support の Intel BSD 条件がそれぞれ適用され、
本作独自の非商用ライセンスでこれらの権利を制限しません。

配布物と上記のバージョン固定 URL からソースを取得できます。エンジン本体の SDK、
ビルドルール、バイナリー、unsupported、未使用の外部ソルバーや GPU 用分岐は含みません。
開発時には別途導入したエンジンを使用してください。
