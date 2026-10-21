# Codex controller profiles

- Role: Controller settings implementer and integration owner
- Status: working — final focused verification and signed settings/controller delivery
- Base: `56891a71a`
- Branch: `feat/controller-profiles-20261020`
- Worktree: `.cache/controller-profiles-20261020`
- Ownership: `src/controllers`, `tests/controllers`, controller Settings model/QML, controller wiki/ADR and bounded Portage delivery
- Execution: selected Codex session only; no delegated agents or external model workers

## Updates

- 2026-10-21T00:24:27+00:00 — Claimed the requested settings expansion and R2 correction. Preserve device identities, current bindings and motion preferences. Acceptance: atomic schema migration/round trip, combinations and release ordering, bounded text delivery, virtual SDL page/LED tests, Settings journey, strict docs and signed Portage installation.

- 2026-10-21T00:49:32+00:00 — Initial nine focused CTest groups pass in 3.84 seconds. User expanded scope: held R2 layers, separate touchpad contact/gyro, fullscreen media, shared macros with keyboard/mouse triggers. Added generic InputActions module, native Shortcuts1 async lease and timer-based macro engine; finishing editor and expanded fixtures before delivery. No model delegation.

- 2026-10-21T01:33:34+00:00 — Core/platform build passes; Unicode, pages/LEDs, layers, gyro contact, macro playback and native lease fixtures pass. Native Register was verified to overwrite keys, so the lease now reads/preserves existing alternatives and deactivates persistent assignments on unload via the existing public release wire. QML editors are rendered and checked, keep drafts until confirmed save, and separate layer editing from activation. Completing the cold-backend fixture and route-search gate. The signed r39 base image is reverified (87,296,000 bytes, SHA-256 2edd3ee9…, 2,693 unchanged base paths).

- 2026-10-21T02:01:00+00:00 — All 14 focused groups passed (126 QtTest results, no failures/skips). Verified kernel Mute side effect; Create now cycles profiles, Options remains Escape and Mute remains unassigned. Freezing candidate for signed r40 delivery over installed Harmony image. No external/model workers.

- 2026-10-21T02:12:58+00:00 — The bounded Portage build reached the final plugin link and exposed missing PIC on the production shortcut client. Preserving all build outputs; set PIC at the owning client target and replace the standalone substitute with that actual production target. Resume r40 via a versioned Portage patch without recutting its immutable source archive.

- 2026-10-21T02:13:41+00:00 — Production shortcut target now builds and links into the focused plugin; all 14 groups pass again (126 QtTest results), strict docs and 576-document link gate pass. Portage resumed from the retained outputs and completed r40 in 37.326 seconds. Verifying its signed image before merge/install.

- 2026-10-21T02:22:45+00:00 — r40 is Portage-installed; all 2,420 installed file/symlink checks pass, original compositor/shell alive, five DualSense profiles applied and five other profiles preserved. Live InputActions1 native assignment/wait-playback/cleanup pass. Installed Controllers probe exposed one shutdown geometry warning; add positive confirmation widths and a zero-size-window/open-close fixture, then package only QML over signed r40 as r41.
