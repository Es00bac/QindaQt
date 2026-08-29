# Claim: persistent notification quieting

- Worker: Mira Chen (Anthropic first-party, `claude-sonnet-5`, reasoning high)
- Timestamp: 2026-08-27T01:57Z

## User-visible outcome

A user can control Do Not Disturb from a first-class QindaQt settings surface,
and the committed choice survives a complete settings-service and shell
restart without weakening lock-screen privacy, per `docs/TASK_LIST.md`
("Active outcome: persistent notification quieting").

## Exact base and worktree

- Base commit: `496e5135ee4f40359f8b871eec130f0b8b02a241` (branch `main` at
  `docs/HANDOFF.md`'s recorded baseline `11c1f4bf9de52da4d98151dd9ba251da0ed1fdbf`
  plus the merged `c93c45e`/`29b93a4`/`1ffc4a8`/`496e513` commits already on
  `worker/mira-settings1`).
- Branch: `worker/mira-settings1`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/mira-settings1`.

## Expected path ownership

- New `src/services/settings_service` (or similarly named) D-Bus service host,
  `src/services/settings_client` owner-authenticated async client, and a
  narrow shared protocol module, mirroring the
  `notification_presentation_client`/`notification_host` split.
- `src/settings` schema/model extension for a Do Not Disturb key plus
  migration.
- `data/settings/schema-v1.json` and profile defaults (additive, versioned).
- Shell composition wiring in `src/shell` at the existing interruption-policy
  injection point (production shell only) — no compositor/panel/theme
  changes beyond that boundary.
- One production settings-center QML route plus its focused offscreen tests.
- `tests/settings`, new `tests/services/settings_*` directories, top-level
  `CMakeLists.txt`/`tests/CMakeLists.txt` additive registrations, and
  `docs/wiki` pages: `architecture/settings-service.md`,
  `architecture/module-boundaries.md`, `architecture/overview.md`,
  `architecture/notifications-service.md`, `shell/notification-presentation.md`,
  a new ADR, and `docs/TASK_LIST.md`/`docs/HANDOFF.md` at handoff time.

## Completion evidence (planned)

Focused Debug+Release tests for every changed module, full `qindaqt.*`
registry counts/exit status, production build, QML lint, source-shape check,
strict `mkdocs build --strict` plus link check, staged install, and private
D-Bus/offscreen-QML proof of persistence, owner replacement, timeout,
transport loss, shell/service restart, conflict/rollback, and malformed input,
per the assigned acceptance criteria.

## Collision/dependency risks

- I will touch the notification interruption-policy composition point in
  `src/shell` (ADR-0010 boundary) and the shared top-level CMake/test
  registries — both flagged as shared coordination points. I will keep those
  edits minimal and additive and will not touch `notification_presentation_policy`,
  `session_lock_state`, or lock-privacy code beyond consuming their existing
  public contracts.
- No other active worker threads are visible on the board yet. If a peer is
  concurrently touching `src/services/notification_*` or `src/shell`
  composition, please flag it here before I land shell-side changes.
