# ADR-0308: Store native lock preferences in Settings1 and import once

- **Status:** Accepted; native preference persistence slice, runtime/UI migration separate
- **Date:** 2026-09-30
- **Owners:** native lock services and Settings service
- **Supersedes:** [ADR-0091](0091-configure-kscreenlocker-preferences-through-settings.md) for native preference authority only
- **Related:** [ADR-0304](0304-native-lock-observation-and-service-policy.md), [ADR-0293](0293-settings1-native-power-policy-and-powerdevil-import.md)

## Context

Removing the Plasma locker leaves its existing settings behind. Native idle and
resume policy need durable bounded preferences without retaining KScreenLocker
as a runtime dependency or using configuration as unlock authority.

## Decision

Settings1 schema v2 owns four native preferences: automatic locking defaults on;
idle timeout is 60–14400 seconds, default 300; resume locking defaults on;
pre-acquisition idle grace is 0–300 seconds, default 5. The separate boolean
`lock.migration.kscreenlockerImported` defaults false. The additive `SettingDomain::Lock`/`lock` domain preserves the schema’s exact
key-prefix rule; existing enum values retain their order. Published Keyring remains ordinal 10;
Lock is explicitly ordinal 11, including older candidate branches without Keyring. Settings route grouping
is independent of this domain and gives Power1 no lock authority.

The public Core-compatible `QindaQt::LockPreferences` decoder/provider exposes
only these four values from a ready, exact-current-owner, nonempty-epoch
SettingsClient snapshot. Invalid, incomplete, replaced or unavailable state
returns no preference snapshot. The provider borrows its same-thread client;
the client outlives it. It owns no storage, timer, lock request or QML capability.

Settings1 reads the injected read-only legacy configuration only after winning
its service name. Production supplies `kscreenlockerrc` from the XDG config root;
tests supply disposable files. A single at-most-one-MiB valid UTF-8 snapshot is
parsed privately, so the configuration parser cannot reopen a growing or
replaced source. Missing, malformed, unsupported and empty sources remain
unmarked for retry. Only `[Daemon]` `Autolock`, `Timeout` (integer minutes 1–240),
`LockOnResume` and `LockGrace` (integer seconds 0–300) are imported. Unknown keys,
including password-policy bypasses, never enter the plan. A malformed supported
value rejects the complete plan; defaults remain available without marking it.

Current native user overrides win independently for every preference. The
completion marker and imported values use one SettingsRepository transaction.
A failed save changes neither values nor marker; a later startup can retry.
A completed import never rereads legacy settings into native choices. The legacy
file is never written or reloaded into a running KDE service.

Grace is cancellable idle time before lock acquisition, never a period in which
an acquired lock may be unlocked without authentication. Manual locking and
sleep ordering have no grace. Runtime policy must consume admitted actual
Protected state as specified by ADR-0304; persistence alone implements no policy.

## Consequences and qualification

Schema, exact-type decoder, owner-loss/provider, private-bus ownership,
read-only source, explicit-value precedence, idempotence and failed-save
round-trip gates qualify this persistence boundary. The existing legacy Settings
route remains unchanged until its separately reviewed native UI migration.
Native Lock1/ScreenSaver, idle/manual/resume/sleep policy, consumer rewiring,
released packaging and physical/live lock/sleep qualification remain open.

See [native session lock](../architecture/native-session-lock.md),
[Settings1](../reference/settings1-v1.md), [module boundaries](../architecture/module-boundaries.md)
and [testing harness](../development/testing-harness.md).
