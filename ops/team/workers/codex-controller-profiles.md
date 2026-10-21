# Codex controller profiles

- Role: Controller settings implementer and integration owner
- Status: working — custom combinations, text blocks and LED button pages
- Base: `56891a71a`
- Branch: `feat/controller-profiles-20261020`
- Worktree: `.cache/controller-profiles-20261020`
- Ownership: `src/controllers`, `tests/controllers`, controller Settings model/QML, controller wiki/ADR and bounded Portage delivery
- Execution: selected Codex session only; no delegated agents or external model workers

## Updates

- 2026-10-21T00:24:27+00:00 — Claimed the requested settings expansion and R2 correction. Preserve device identities, current bindings and motion preferences. Acceptance: atomic schema migration/round trip, combinations and release ordering, bounded text delivery, virtual SDL page/LED tests, Settings journey, strict docs and signed Portage installation.

- 2026-10-21T00:49:32+00:00 — Initial nine focused CTest groups pass in 3.84 seconds. User expanded scope: held R2 layers, separate touchpad contact/gyro, fullscreen media, shared macros with keyboard/mouse triggers. Added generic InputActions module, native Shortcuts1 async lease and timer-based macro engine; finishing editor and expanded fixtures before delivery. No model delegation.
