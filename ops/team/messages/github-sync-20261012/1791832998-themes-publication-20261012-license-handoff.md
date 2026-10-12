# Handoff: release original Boot, LLM and Deck code under GPL-3.0-or-later

- Time: 2026-10-12T19:23:18+00:00
- Authority: explicit owner approval relayed by manager: "Release the original code under GPL-3.0-or-later"; retain third-party licenses.
- Requested action: independent exact-commit review, then manager-controlled integration/publication. Overlay owner has the candidate pins and must wait for manager acceptance before final archive freeze.

| Repository | Candidate | Exact parent | Preserved non-README base paths |
| --- | --- | --- | --- |
| QindaBoot | `f778750ea9e9622b76b2abd84f0710486757997f` | `a49f02407d1170c574921fe6bd2c8f9455faff4f` | 42 |
| QindaLLM | `91a294cd967f0331bbef13f95b74920a27cd2d7e` | `977f93f57fa6d2fc15fba9707dedecc012065cf2` | 10 |
| QindaDeck | `38f3731652058114288d96d592f3750a45774ff7` | `2edcd922508b78314fec07b7077dcaf05679e75b` | 103 |

All three candidates are pushed only to `ops/license-gpl3-20261012` in their respective `qinda:~/git/<name>.git` hubs. Clean isolated worktrees are `qinda:~/work_SPaC3/<name>-license-20261012`. At handoff, canonical worktrees and hub main/main/master still equal their reviewed parents. No public remote or normal branch was pushed.

## Exact changed files and scope

Each candidate changes only root `LICENSE` (new) and `README.md` (append-only). GPLv3 text is copied byte-for-byte from installed `/var/db/repos/gentoo/licenses/GPL-3`: 35,149 bytes, SHA-256 `3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`. README expressly grants GPL version 3 or any later version to original code authored for that project. Existing production files need no edits to establish that repository-wide original-code scope.

- Boot README preserves third-party and non-code asset terms. `art/reclaimed.png` is identical to QindaThemes' publicly retained Reclaimed wallpaper (SHA-256 `1484fec2532c6f88fc5db4703078e4e8282452c8c138a3792959ea9c1c956a14`); the original artwork provenance explicitly does not newly GPL-relicense it. Existing generated image/font assets retain their applicable terms. The generator uses separately installed Inter/IBM Plex Mono; no font binaries are bundled. No artwork was regenerated. Overlay owner has this scope and manager-directed composite license/restrictions.
- LLM README preserves separate model-weight, llama.cpp and external component licenses. Models/runtime are not bundled or relicensed by this source change. Config values and service/monitor files are untouched.
- Deck README identifies existing Gradle wrapper launcher `Apache-2.0` notices and verbatim ESP-IDF v5.5.5 `esp_hid_gap.c/.h` notices (`Unlicense OR CC0-1.0`). Vendor files and external ESP-IDF/FatFs/TinyUSB/Android/Jetpack terms remain unchanged; no third-party code is assigned a new license.

The root README and LICENSE are the intended installed documentation for the new source-license package revisions. Existing overlay recipes are outside worker ownership and remain immutable.

## Verification and limits

Static content checks exited 0 for all three candidates:

- Asserted source hub and canonical checkout exact parents before edits, before commits and after candidate-only pushes; canonical checkouts were clean.
- Compared complete base and staged trees by Git object ID and mode: only README differs and LICENSE is added; all 42/10/103 other paths are byte/mode-identical, including runtime, config, tests, assets and vendor notices.
- Confirmed every new README starts with the exact original README bytes and adds an explicit GPL-3.0-or-later declaration.
- Confirmed canonical GPL text byte equality and hash in each new LICENSE.
- Checked all local README links: Boot 1, LLM 1, Deck 5; `git diff --cached --check` passed.
- Confirmed each commit has exactly the reviewed base as parent, the candidate branch resolves to its exact SHA in the hub, and each isolated worktree is clean.

No project tests, source/firmware/Android builds, package installs, model tests/downloads/inference, service operations, runtime or device actions were run. They are unnecessary for this license/documentation-only change, and model/runtime activity was explicitly prohibited. Existing source-readiness audit remains applicable because no executable, configuration or asset blob changed.

## Follow-up help

Available for bounded repairs in the same worktrees if exact review finds a scope, notice or documentation issue. Manager owns public publication and final integration; overlay owner owns new immutable package revisions and installed-document verification.
