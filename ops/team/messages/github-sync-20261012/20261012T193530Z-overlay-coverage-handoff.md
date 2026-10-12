# Overlay coverage candidate handoff

- Timestamp: 2026-10-12T19:35:30+00:00
- Exact candidate: `c16790ee51a3998688f189f279fe3164bafb155b`
- Exact base: `18eb802cbab542c856194be7e1d466b4abd5f419` (rebased from assigned 422c024; concurrent Trellis changes preserved)
- Pushed branch: `qinda:~/git/QindaGentoo.git` → `codex/overlay-coverage-20261012`
- Clean isolated worktree: `qinda:~/work_SPaC3/.worktrees/QindaGentoo-overlay-coverage-20261012`
- Requested next action: independent exact-candidate review, then manager-only fast-forward integration/publication. No host installation/default delivery/activation is part of this handoff.

## Outcome and scope

Every named non-game software project is mapped to its existing or new package boundary in `docs/first-party-coverage.md`. Added optional FoxAgent, static QindaTK guide, JevReader and QindaDeck host tools. The initial conservative Deck recipe/archive remains immutable; latest r1 pins the newly licensed source. Separate Boot-r1 and LLM-r2 license/source snapshots preserve every accepted runtime/configuration file and activation function. Source LICENSE and README provenance ship in all three licensed revisions. Boot retains composite GPL-3+ / reserved-artwork terms and fetch/mirror/bindist restrictions; LLM's reproducible source-hub archive retains fetch restriction. Original Boot/LLM recipes, prior distfile rows, profile packages/defaults, exact delivery, and Trellis content are unchanged. New optional entries add availability keywords only.

Source pins: FoxAgent `9316a5aecf0eb75105cece809eb372eea27a859e`; guide `3d515432d65fedffe8ce047c455e4316c092063b`; JevReader `843cae73e43a31d084edc809d9c67ae58ddf358c`; Boot `f778750ea9e9622b76b2abd84f0710486757997f`; LLM `91a294cd967f0331bbef13f95b74920a27cd2d7e`; licensed Deck `38f3731652058114288d96d592f3750a45774ff7`. Manager accepted the latter three exact source license candidates before archive freeze.

## Exact changed paths

```text
CLAUDE.md
README.md
app-doc/qindatk-agent-guide/Manifest
app-doc/qindatk-agent-guide/files/README.gentoo.md
app-doc/qindatk-agent-guide/metadata.xml
app-doc/qindatk-agent-guide/qindatk-agent-guide-0.1.0_p20261012.ebuild
app-misc/qindadeck-tools/Manifest
app-misc/qindadeck-tools/files/README.gentoo-licensed.md
app-misc/qindadeck-tools/files/README.gentoo.md
app-misc/qindadeck-tools/metadata.xml
app-misc/qindadeck-tools/qindadeck-tools-0.1.0_p20261012-r1.ebuild
app-misc/qindadeck-tools/qindadeck-tools-0.1.0_p20261012.ebuild
dev-util/jevreader/Manifest
dev-util/jevreader/files/README.gentoo.md
dev-util/jevreader/jevreader-0.1.0_p20261012.ebuild
dev-util/jevreader/metadata.xml
docs/first-party-coverage.md
profiles/categories
profiles/qindaqt/systemd/package.accept_keywords
sci-ml/qinda-llm-server/Manifest
sci-ml/qinda-llm-server/metadata.xml
sci-ml/qinda-llm-server/qinda-llm-server-1.0.0-r2.ebuild
sys-boot/qinda-boot/Manifest
sys-boot/qinda-boot/metadata.xml
sys-boot/qinda-boot/qinda-boot-1.0.0-r1.ebuild
www-plugins/qindafox-agent/Manifest
www-plugins/qindafox-agent/files/qindafox-agent-0.1.0-installed-remote.patch
www-plugins/qindafox-agent/files/setup.md
www-plugins/qindafox-agent/metadata.xml
www-plugins/qindafox-agent/qindafox-agent-0.1.0_p20261012.ebuild
```

## Verification and reproducible evidence

All retained scripts, distfiles, image snapshots and logs are on qinda under `/var/tmp/qindagentoo-overlay-coverage-20261012/`. The isolated `repos.conf` changes only this process's overlay location. `stage.py` runs the explicit recipe's Manifest/source/image phases; it never merges or resolves/install dependencies. Executed commands:

```sh
python3 stage.py www-plugins/qindafox-agent/qindafox-agent-0.1.0_p20261012.ebuild 'test launcher'
python3 stage.py www-plugins/qindafox-agent/qindafox-agent-0.1.0_p20261012.ebuild 'test -launcher' -minimal
python3 stage.py app-doc/qindatk-agent-guide/qindatk-agent-guide-0.1.0_p20261012.ebuild test
python3 stage.py dev-util/jevreader/jevreader-0.1.0_p20261012.ebuild test
python3 stage.py app-misc/qindadeck-tools/qindadeck-tools-0.1.0_p20261012.ebuild test
python3 stage.py app-misc/qindadeck-tools/qindadeck-tools-0.1.0_p20261012-r1.ebuild test -licensed
python3 stage.py sys-boot/qinda-boot/qinda-boot-1.0.0-r1.ebuild -test -licensed inert
python3 stage.py sci-ml/qinda-llm-server/qinda-llm-server-1.0.0-r2.ebuild -test -licensed inert
python3 verify.py
python3 verify_licensed.py
python3 -m unittest discover -s tests -v
git diff --check 18eb802cbab542c856194be7e1d466b4abd5f419
```

All completed with exit 0. FoxAgent: 3 offline framing/payload/MCP tests; optional launcher images differ by exactly one wrapper and all common files are byte-identical. Guide: 11 deterministic behavior checks, static references, DOM targets, JS syntax, 17 existing recordings and narrated film; complete installed static tree matches its source bytes. JevReader: 15 deterministic fake-scorer fixtures, no provider requests. Each Deck revision: help-parser and JSON-schema checks, no discovery/device action. Boot/LLM: **tests explicitly skipped**, no model/provider work; their compile functions are no-ops and install phases only copy scripts/config/assets into isolated images. Overlay suite: 40/40 tests passed; its unchanged code was not rerun after license-only additions. Ebuild shell syntax, XML, new Markdown links, preserved metadata/defaults/Trellis, unchanged `MAKEOPTS=-j24 -l24`, exact source inventories and Manifest digests pass. No overlay MkDocs configuration exists; source/installed link and static-guide checks cover these documentation changes.

Source/archive/image inventories pass for 76 FoxAgent blobs/25 image paths, 45 guide blobs/39 paths, 13 JevReader blobs/36 paths, 104 conservative Deck blobs/9 paths, 44 licensed Boot blobs/39 paths, 12 licensed LLM blobs/9 paths, and 105 licensed Deck blobs/10 paths. Installed GPL/README bytes match source. Boot/LLM/Deck source diffs contain only LICENSE and append-only README; every runtime/config asset remains identical apart from Portage's existing interpreter shebang rewrite. No API key, weights/runtime binary, ESP deployment or service-enable links appear in the new staging images.

`pkgcheck scan --repo <worktree>` on the seven exact recipe versions exited 0; actual advisories are retained in `logs/pkgcheck-final.log`: Python 3.15 compatibility suggestions (qualified Python here is 3.14); guide's test-only NodeJS dependency is unavailable in two x32 dev profiles; Boot's inherited systemd dependencies fail unrelated non-systemd profiles; LLM's inherited `sci-misc/llama-cpp[vulkan]` is outside this overlay's `gentoo`-only masters. The configured `::guru` provider is directly present at `/var/db/repos/guru/sci-misc/llama-cpp/llama-cpp-0.4.1.ebuild`, and read-only `portageq match / sci-misc/llama-cpp` reports installed 0.4.1. The documented dependency was not changed or exercised.

Initial environment-only failures are preserved: an unprivileged install-image phase could not assign root ownership, then private `~/.cache` blocked Portage's userfetch traversal. Final staging used task-only `/var/tmp` directories and privileged image-file ownership with ordinary sandbox/userpriv behavior. Early one-off verifier issues (tar root directory spelling, Portage build-root permissions, python-exec staging symlink) were corrected in the checker, not package runtime. No failed model test or provider attempt occurred.

## Licensed archive freeze and manager publication

| Archive | Bytes | SHA-256 | Source |
| --- | ---: | --- | --- |
| `qinda-boot-1.0.0-r1.tar.gz` | 5952208 | `b97cfea02489d7a3d44dc51b6ad8db9bcba02428e39f5cdab5be1358fa39e68e` | Exact public GitHub archive; reserved-artwork fetch/mirror/bindist restrictions retained |
| `qinda-llm-server-1.0.0-r2.tar.gz` | 30492 | `693a8c3689787a60ce744faa8e7299fc856266b6ddf0d860fdfce0a81dd6c51b` | Source-hub `git archive --format=tar --prefix=QindaLLM-<commit>/ <commit> \| gzip -n`; two independent cuts match |
| `qindadeck-tools-0.1.0_p20261012-r1.tar.gz` | 216205 | `2e99ad6e87a12c5939e9b2eaca18e84c187cf317acbdb6454e45a37a835cc868` | Exact public GitHub licensed archive |

The frozen LLM archive must **not** be replaced with GitHub-generated bytes when repository creation clears GitHub's temporary content-creation limit. After exact candidate acceptance/integration, manager may publish this already verified source archive through normal `qinda-sync publish`; do not run package installs or services as a publication side effect. Images are in `images/*-licensed`, and exact verification/stage records in `logs/licensed-image-verification.log` and `logs/*-licensed-stage.log`.

## Bounded follow-up

No model tests/inference/downloads/runtime builds, service actions, boot deployment, browser activation, live hardware journey, user agent registration, host merge or software installation outside Portage was performed. Those qualifications remain explicitly deferred. Latest platform queue and source-license peer handoff/review were read; no unrelated queue work is claimed. Concrete help offer: repair exact independent-review findings in this same preserved worktree and rerun only the affected source/image gate.
