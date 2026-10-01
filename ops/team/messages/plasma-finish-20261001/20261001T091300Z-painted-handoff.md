# Supplied Breeze Painted exact review handoff

- Worker: pf-packaging-sol-20261001.
- Base: QindaGentoo c78ac162a28f23039d117df37046b25465619a9a.
- Exact candidate: **54eb796478a67f9665637e346ac9a776d5f1fcfe**.
- Hub branch: `worker/breeze-painted-20261001` (pushed exit0).
- Isolated qinda worktree: `~/work_SPaC3/QindaGentoo.worktrees/breeze-painted`.
- Changed paths: new `x11-themes/qinda-breeze-painted/{Manifest,metadata.xml,
  qinda-breeze-painted-1.0.0_p20260929.ebuild}`, `README.md`, one exact profile
  `package.accept_keywords` entry. No profile packages/desktop RDEPEND or current
  shared delivery/selection changes.

## Exact immutable artifact

qinda `~/work_SPaC3/QindaIconArt.sources/packaging/qinda-breeze-painted-1.0.0_p20260929.tar.gz`

- Bytes: **94,243,325**.
- SHA256: `c92ca981df77279751c089e27b0f2ea115f1105d41b72ef15cc0b825a05176ef`.
- SHA512: `1917cd60b0268c0b49729983aee8e30d565d16b9242ce13513eb3acf63c8c7f306a4004191f41a24983405b8b7fa76498f869c995390a8663a5285cd7b06e150`.
- BLAKE2B and same SHA512/size committed in Portage Manifest.
- All eleven original input ZIP SHA256 identities explicitly pinned in overlay
  README and archive INPUT-SHA256SUMS. Original ZIPs remain in laptop Downloads;
  complete extracted theme/source and immutable artifact also preserved on qinda.
- Deterministic sorted USTAR archive: original7230 files, fixed2026-09-29 UTC
  timestamps, uid/gid0, mode0644, empty owner/group names, deterministic gzip header.
  No source transformation, resizing, artwork replacement or renaming performed.

## Package behavior

Installs original `index.theme` and7172named128RGBA PNG paths under
`/usr/share/icons/QindaQt-Breeze-Painted`. Original12directory contexts,
Fixed128metadata and `Inherits=breeze,hicolor` preserved byte-for-byte.
Only runtime dependency is `x11-themes/hicolor-icon-theme`; Breeze remains
optional ambient fallback, so no Plasma dependency is introduced.

All original theme metadata/manifests/previews/licenses and full18-file editable
source companion are preserved uncompressed under `/usr/share/doc/${PF}/theme`
and `/source`. The two supplied install.sh copies are documentation with no
executable mode; never executed. LGPL-3+package license follows supplied notices.
No host theme selection, Settings transaction or service action in package hooks.

## Executed evidence

All stages are on qinda; no package merge yet.

- Direct archive member byte comparison against qinda authoritative supplied
  theme/source/provenance stage: **7230/7230 PASS, exit0**.
- Final exact recipe data-only Portage image phases:
  `ebuild <exactrecipe> clean setup unpack prepare configure compile install`:
  **exit0**, log `~/work_SPaC3/QindaIconArt.sources/qa/final-ebuild-image.log`.
  `PORTAGE_REPOSITORIES` maps this invocation's qindaqt repo to isolated worktree;
  DISTDIR and PORTAGE_TMPDIR use ignored source packaging/build directories.
  Configured MAKEOPTS inherited unchanged `-j24 -l24`; default compile phase has
  no native product sources. A preexisting nonroot Portage hook reports an
  unsuccessful `/etc/gitconfig` lock attempt; no system config edit succeeded.
- `python3 .../qa/verify-image.py <image>`: **exit0**. Exact7172PNG inventory,
  SHA256,128x128RGBA8mode, original index, all theme metadata and full source
  companion bytes pass; documentary installer modes are nonexecutable.
- Image **7173runtime files /7231total files**; actual installed filesystem
  collision comparison: **zero**, exit0. No image symlinks.
- Native production library QA helper `.../qa/catalog-check <image/usr/share/icons>`:
  **exit0**, actual installed `libqindaqt_themes.a` discovers public family id/name
  and resolves explicit installed family preference. Helper compiled only into
  ignored QA directory, not installed.
- Native Qt `.../qa/render-check.py <image/usr/share/icons>` using PyQt6QtGui:
  **exit0**,12distinct category representatives load via QIcon.fromTheme and
  match actual source128pixmap bytes after identical premultiplied conversion.
  Offscreen process has private runtime, no host session bus or platform theme;
  its in-process QIcon theme selection is not a user preference write.
- `pkgcheck scan --repo . x11-themes/qinda-breeze-painted`: **exit0, clean**.
- `git diff --check`, candidate commit and hub push: **exit0**.
- Before-selection snapshots saved locally on qinda in QA directory for both
  hosts (only relevant appearance fields, no private records output).

Image root:
`~/work_SPaC3/QindaIconArt.sources/build/portage/x11-themes/qinda-breeze-painted-1.0.0_p20260929/image`
QA sources/logs:
`~/work_SPaC3/QindaIconArt.sources/qa/{verify-image.py,catalog.cpp,catalog-check,render-check.py,final-ebuild-image.log}`

## Requested next action and bounded gates

Root independently review exact recipe/archive/image, then integrate accepted
candidate and direct authorized installer to continue. Publication through
qinda-sync waits for accepted Manifest in hub master. Then build/sign/merge
through Portage on qinda, binary delivery to laptop or data-only Portage merge,
inspect all7172installed named PNGs and7231owned package files/integrity,
repeat native installed catalog and12representative renders on both hosts,
and compare current appearance selection with captured before values. No forced
selection, supplied installer, artwork edit or desktop restart. No broad desktop
upgrade, unmerge or depclean is part of this independent package task.
