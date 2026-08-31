# S3 exact Qt declarative package is staged; recursive closure proves Qt SVG is required

- From: Dorothy Vaughan
- To: Program Manager
- Time: 2026-08-31T01:50:50-06:00
- Feature: QQ-006.09 private WUXGA/fractional-scale/theme/multi-output runtime
- State: stopped at the first broader dependency; no WUXGA rerun

The preserved KWin 6.6.5-4 BUILDINFO proves its exact Qt build boundary used
`qt6-base 6.11.1-1` and `qt6-declarative 6.11.1-3` on `x86_64`. The matching
official package declares only `glibc`, `libgcc`, `libstdc++`, and `qt6-base` as
mandatory runtime dependencies; it declares `qt6-svg` optional specifically for
QtQuickVectorImage and `svgtoqml`.

With prior authorization I downloaded only:

- `https://archive.archlinux.org/packages/q/qt6-declarative/qt6-declarative-6.11.1-3-x86_64.pkg.tar.zst`
  SHA-256 `28fa2eb2246e6b5dadcbd29cf9aaa1218b6b04137ef92e496ce3e969d720af48`
- the adjacent `.sig`, SHA-256
  `060a391da6617e3f161bdc2f3629f901deadb13a7d84637576250713b7ddd92a`

HTTPS remained verified end to end. No `pacman-key` or Arch keyring is present.
An isolated GPG verification therefore terminates with
`NO_PUBKEY 7A4E76095D8A52E4`; the exact log is
`/tmp/qindaqt-arch-665/pkgs/qt6-declarative-6.11.1-3.signature-verification.log`
(SHA-256 `de65de45…`). The signature is not claimed verified.

The before manifest proves all 5,055 non-directory package payload paths were
absent. Private-only extraction of the package `usr` payload produced exactly
4,955 files and 100 symlinks with zero missing or unexpected-type paths. The
private root metadata hash remained identical. The exact after manifest is
`/tmp/qindaqt-arch-665/pkgs/qt6-declarative-6.11.1-3.after-manifest.txt`
(SHA-256 `62340283…`).

Private `qmlimportscanner` proofs now resolve QtQuick.Controls, settings,
file-manager, audio, shell, and staged QindaQt imports only from the private
Arch QML root, staged QindaQt modules, or the shell's generated embedded module:
zero host QML paths and zero unresolved modules. The exact Controls plugin is
private and has SHA-256 `7d908ddc…`.

The required recursive ELF proof stopped the lane. Its 248 target identities
have zero `not found` entries, but ten resolution lines covering nine distinct
target identities resolve `libQt6Svg.so.6` from host `/usr/lib64`, not the
private/staged closure. The affected identities include private KWin, the
staged `qindaqt_compositor.so`, `libQt6QuickVectorImage*`, `svgtoqml`, and the
QtQuick VectorImage QML plugin. Exact evidence:

`/tmp/qindaqt-arch-665/pkgs/qt6-declarative-6.11.1-3.recursive-elf-closure.log`
(SHA-256 `8cfcdc23…`).

The host Qt SVG happens to be 6.11.1, so this is not a version mismatch, but it
does violate the required entirely-private/staged Qt closure. It also proves the
effective runtime set is broader than qt6-declarative's mandatory dependency
list. Per the stop condition, I did not run units, WUXGA, or download/extract
another package. Smallest coherent next step is exact Arch Archive
`qt6-svg 6.11.1` matching the KWin boundary, after its precise package release
is proven from preserved BUILDINFO/Archive metadata and separately authorized.
