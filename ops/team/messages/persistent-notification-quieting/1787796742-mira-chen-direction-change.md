# Direction change: adopting the manager boundary decision

- Worker: Mira Chen (Anthropic first-party, `claude-sonnet-5`, reasoning high)
- Timestamp: 2026-08-27T02:12:22Z

I read `1787796161-codex-ui-audit-finding.md`, `1787796317-codex-contract-audit-finding.md`,
and `1787796417-manager-boundary-decision.md` in full. The manager decision is
authoritative and I am realigning to it before writing more code.

## State at interruption

Only three tracked files had uncommitted edits, plus one new untracked
directory:

- `data/settings/schema-v1.json` — additive `services.doNotDisturb` key
  appended directly to v1. **Reverted by hand via `git checkout --` to the base
  commit's content** (verified via `git diff` before reverting; no other
  in-progress work touched this file).
- `tests/settings/tst_settings_persistence.cpp`,
  `tests/settings/tst_settings_schema.cpp` — DND-specific test additions built
  against the same mutate-v1-in-place assumption. **Reverted the same way**;
  I will re-add equivalent coverage against schema v2 plus dedicated migration
  tests instead.
- `src/services/settings_protocol/` (new, untracked) — wire contract
  (`settings_wire_contract.h`) and a wire status enum
  (`settings_wire_status.h/.cpp`) plus `CMakeLists.txt`. **Keeping this
  module** as the shared protocol boundary the manager decision also calls
  for; I will revise the wire shape (method names, conflict reply carrying
  current lineage, no PID/token fields) to match the decision below rather
  than discard it.

No other files were touched. No build, test, or commit had been run yet, so
there is nothing else to reconcile.

## Realignment I am carrying forward

1. **Schema**: keep `schema-v1.json` immutable; add `data/settings/schema-v2.json`
   (v1 plus `services.doNotDisturb`, boolean, default `false`) as the active
   schema. Add an explicit `SettingsSchema::fromFile/fromJson` optional
   `expectedVersion` parameter (defaults to the existing compile-time active
   version, so every current call site is unaffected) so a v1-to-v2 migrator
   in `src/settings` can load both schemas at once. Bump
   `QINDAQT_SETTINGS_SCHEMA_VERSION` to 2. Existing `tests/settings/*` schema
   loads move to `schema-v2.json`; new `tst_settings_migration.cpp` covers
   the v1-to-v2 document path plus corrupt/unsupported/idempotent cases.
2. **Service**: `qindaqt-settings-service` is independently D-Bus-activatable,
   not a `qindaqt-session` child. Copy-on-write commits (clone model, apply,
   save candidate document, then swap the authoritative in-memory model and
   publish). Exact-unique-owner fencing only — no PID/token authentication,
   since this is ordinary same-user session authority, not a privacy boundary.
3. **Client/shell bridge**: new `src/shell/runtime/notificationquietingsettingsbridge.*`
   drives the existing `NotificationInterruptionPolicy` from Settings1. Before
   the first authenticated baseline it fails quiet (suppresses low/normal
   popups) rather than trusting the policy's off default, so a persisted
   `true` is never briefly violated at startup. After a baseline, owner/bus
   loss holds the last confirmed value; replacement owners apply only after a
   full authenticated snapshot; lock privacy stays untouched and always wins.
4. **UI**: a separate ordinary `qindaqt-settings` Qt Quick app
   (`src/apps/settings_center`) with a `notifications` route and
   `--page notifications`, consuming only the public Settings1 client. No
   settings applet, layer-shell window, or global shortcut. Discoverability
   is an installed desktop entry plus a fixed, focusable **Notification
   settings…** action in the existing notification center that starts the
   exact app command (no general process-launch capability exposed to QML).
5. **Existing quick toggle**: rewire `NotificationCenter.qml` to submit
   through the same Settings1 transaction bridge; make
   `NotificationPresentationController::doNotDisturbEnabled` read-only to QML
   (drop `WRITE` from the property, keep the C++ setter for shell composition
   only). A new ADR-0012 will record this as superseding the "notification
   center owns the writable control" clause in ADR-0010.

I am dropping the settings-applet/layer-shell-window/global-shortcut design I
had started scoping (no code had been written for it yet, so nothing to
revert there).

## Next

Continuing the original outcome under this corrected direction toward one
verified candidate commit. Will post test evidence and the handoff separately.
