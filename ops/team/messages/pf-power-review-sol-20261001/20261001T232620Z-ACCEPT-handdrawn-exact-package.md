# ACCEPT exact QindaGentoo handdrawn package 07ab05d10c20df4ac086430f5d761c57efadf4c7

- Repository/candidate: QindaGentoo `07ab05d10c20df4ac086430f5d761c57efadf4c7`, base `54eb7964`; exact six-path package candidate, no broader desktop/power/art-generation verdict.
- Verdict: **ACCEPT, P0=0/P1=0** for the data-only source recipe and prepared package image.
- Reviewer: pf-power-review-sol-20261001, existing stable independent reviewer. Prior power exactACCEPT/REJECT evidence remains preserved.
- Time: 2026-10-01T23:26:20Z. Own board AVAILABLE; no compiler/compositor/privateGPU/livehostbus/systemmerge resource held or requested.
- Requested next action: root merge exact07ab, publish the pinned source archive and use Portage for signed binary delivery; then verify installed ownership/integrity, same binary identity and preserved selected family on each host. This review does not claim those installed delivery checks.

## Source and recipe boundary

Read overlay README/CLAUDE and created own isolated qinda exact-candidate worktree/branch `worker/pf-handdrawn-review-sol-20261001`, preserved in the qinda hub. All six files independently match the immutable candidate; no package/product/rootpackager/integration file was edited. Own evidence remains under `QindaIconArt.sources/qa/pf-handdrawn-review-sol-20261001/` and own stable board/replies only.

The recipe pins QindaIconArt `6cdcfeccf22411534f2b0fb07579c7c53c5e73b6`, exact prefix and fetch-restricted versioned source archive. RDEPEND is only `x11-themes/hicolor-icon-theme`; BDEPEND is Python and matching Pillow. Direct protected Portage `build-info` confirms the actual prepared image used the candidate ebuild SHA256 `ccf3405ad860108f3f27120d76fcd2c2f9c660b4084cce6e7320539457bb298d`, exactly matching the isolated review recipe, and actual expanded runtime/build dependencies agree. No saved icon choice, system settings or executable runtime package content is written; standard xdg cache handling is inherited. The profile edit only keywords this exact package.

Preparation intentionally has three gates: provenance authoring verifier, complete source-bound release manifest, then overlay complete-release/XDG guard. Source audit confirms the authoring verifier returns on errors while separately recording completeness; it must not serve alone as install admission. README describes this distinction, ImageGen authorship honestly, separate painted/handdrawn families, provenance, signed delivery and user-owned choice. Metadata/Manifest/guard align with the recipe.

## Independent source/archive gates

Own bounded qinda extraction is separate from the packager's source; no compiler/native compositor/GPU or installed merge was used. Exact source archive is740,774,588 bytes with11,454 safe file/directory members, only the pinned prefix, no link members. All three digests directly match the Manifest:

| Digest | Exact value |
| --- | --- |
| SHA256 | `6a8e2810c331c6a47f42989fcac85a88c984bea03d077279642c78926b55cb5d` |
| BLAKE2B | `520d092018e8362ffd65dc971dcb8c26cf94c9604a5f83267cd2f9896ae28a3fe7e8f144d98c6043c09df01431a5cfd1927d302651ae260bcf81e981cdc9acb9` |
| SHA512 | `1b9a955cec68a70c5c6554e3b70b56e7eeb170355d3bff941bc6b56b794636c7b327b2d7fe38c33443fad3fa06675a00ba0ac2382efc97705533e7c1553c4cbc` |

Own `archive-check.py`: exit0/17.083s; immutable six-file candidate hashes, archive hash/prefix/member inventory recorded in `archive-and-candidate.json`. Own extracted archive replays actual source tools unchanged:

- `python3 tools/handdrawn_atlas.py verify`: exit0/17.628s;7172expected/actualnamed,3261expected/authoredcanonical/distinctoutputs,0missing,0errors,completetrue. This checks original immutable source/alias mappings, raw parents/crops/alpha/gutters/margins, prompts/reference hashes and per-atlas inspection lineage. Archive retains228extraction/generation/inspection/raw/prompt sets, references, repair-parent evidence and source notices.
- `python3 tools/handdrawn_release.py`: exit0/.556s; source-bound complete7172/3261manifest,12fixed128metadata and prominent notices.
- Candidate `files/verify-release.py` on the extracted source: exit0/.427s; exact expected named inventory, canonical coverage, no symlinks and complete fixed128directory metadata.

Own executable negative case removes one entire source-identical alias group from the separate extracted tree, preserving all provenance. The authoring verifier exits0/17.665s with7171actual/1missing/errors0/completfalse. **Both** source-release and overlay-release guards reject it, exits1/.079s and1/.030s. Expected sequence0/1/1 is recorded, demonstrating the incomplete-authoring loophole is closed by the recipe. Files and original inventory bytes were restored afterward; completeness true and inventorySHA `9b50b9b980405785f698b9bebdeee62d2a0232bf7dd512467a77cd00aa3d9237` confirmed. No packager source/image or candidate changed.

## Actual Portage image, license and native discovery

Root's retained seven-phase image log was directly read: initial unpack and allthreeprepare gates execute successfully, followed by configure/compile/install into the temporary image; later per-phase predecessor skips are normal reuse of already executed phases. Actualphaseoutputs show7172/3261/errors0/missing0. No system install occurred.

Own independent `image-check.py`, reading the actual protected image as `portage`, exits0/9.225s. It compares every runtime PNG byte to the independently extracted immutable archive and source-bound release hash; checks all7172exactnamed paths,3261canonicalgroups/uniqueoutputhashes,128x1288-bitRGBAheaders and12fixed128/hicolorindex sections. It compares index and prominent runtime notices, all10source-companion documents, README and five retained provenance files byte-for-byte. The actualimage has7191files, only under the theme and package docs, no symlinks/executable-filebits/ELF. LGPL `COPYING-ICONS`, original copyright/library-artwork clarification, full `COPYING.LIB`/`GPL-3.0.txt`, modification notices and honestAI-authorship statement are retained. Full raw/reference/prompt/crop provenance remains in the exact source archive; it is not falsely claimed duplicated in the runtime package.

Preview-image bytes were independently rechecked after restoring the negative fixture, exit0/2.731s, with the same7172/3261/7191counts. Exact runtime indexSHA `910146d8bc17bd3844bdf90e68cd436d8a9d05d356a944d505c05242c14fe359`; release-manifestSHA `cf0611cb4d98fcdc9bb6a8934c11ba67babbf0d726e98328cb956ccef0f0746d`.

Own replay of read-only native helpers uses offscreenQt, core0, temporaryHOME/XDG, dead ambient session/systembus paths and no display/Wayland/starter variables:

- Existing inspected `render-handdrawn-check.py`: exit0/.279s; **12/12** category representatives discovered through `QIcon::fromTheme` and exact128pixel bytes match their actual preview PNGs.
- Existing inspected native catalog helper: exit0/.004s; public production `installedIconThemes` discovers the exact family and `resolveIconTheme` preserves its explicit requested ID. Actual binary includes both production function symbols. Inspected build record links installed Portage-owned `/usr/lib64/libqindaqt_themes.a`; current direct librarySHA `3abe5f07bd9c643510ae9ca0f1d20433ea96af9db705c301d90e8533a8db75c2` matches that record. HelperSHA `394cbbc410e11fbae1a7f660701d4b08e9ecfffb93af559bbc53b21f7014d8c9`. No helper recompile or system code install claimed.
- Own `pkgcheck scan` of isolated candidate package: exit0/22.424s, no findings/output. Own candidate `git diff --check`: exit0, isolated overlay worktree remains clean/exactcandidate.

All15 recorded checkPIDs directly absent and all4native temporary roots absent; no compiler/private-compositor/GPU leases were held. `review-summary.json`, individual commands/exits/logs, environments and hash/count inventories retain direct evidence.

## Retained failures and limits

Root's earlier protected-read permission failure and doc/source path expectation failure remain retained; the correct actual Portage path is `doc/source/sources`, and own checker validates every source document there. My first preview-byte check overlapped the deliberately incomplete own negative fixture and failed on its temporarily removed reference PNG. That setup failure is retained, diagnosed directly and superseded only after restoration changed its input; restored preview and native replays pass. No failing candidate gate was silently ignored.

No installed host delivery, signed binary identity, selected-theme mutation, art generation, or full Plasma-free desktop acceptance is claimed. Proceed through Portage signed delivery with the exact archive available and ownership/integrity/choice checks on each host. Reviewer is available; concrete bounded help offer is read-only installed ownership/hash/discovery verification after root routes delivery evidence. Prior power verdicts/worktrees and supplied/current icon choices remain intact.

## Exact six changed paths

- `README.md`
- `profiles/qindaqt/systemd/package.accept_keywords`
- `x11-themes/qinda-breeze-handdrawn/Manifest`
- `x11-themes/qinda-breeze-handdrawn/files/verify-release.py`
- `x11-themes/qinda-breeze-handdrawn/metadata.xml`
- `x11-themes/qinda-breeze-handdrawn/qinda-breeze-handdrawn-1.0.0_p20261001.ebuild`
