# ADR-0293: Store native power policy in Settings1 and import PowerDevil once

- **Status:** Accepted
- **Date:** 2026-09-29
- **Owners:** Native power policy and Settings service
- **Supersedes:** None
- **Superseded by:** None
- **Builds on:** [ADR-0023](0023-split-power-authority-across-service-and-shell.md),
  [ADR-0105](0105-delegate-idle-display-off-to-powerdevil.md),
  [ADR-0132](0132-finish-session-locking.md)

## Context

The Plasma-free power plan gives Power1 and the QindaQt idle engine policy
that is currently stored in PowerDevil's private configuration. Runtime
replacement needs one durable Settings1 schema before PF2 adds policy behavior.
The old configuration has established action numbers and source-specific
groups, while existing QindaQt Settings1 users already have display-off and
screensaver choices that must remain effective.

## Decision

Settings1 schema v2 adds validated power.lid.*, power.idle.*,
power.critical.*, and power.profile.* values. Lid and idle rules are
source-specific. Power profile defaults are none, preserving the existing
PowerDevil behavior when a source has no PowerProfile key; known non-empty
provider profile IDs are imported from the legacy file. The import is a pure mapping from injected legacy entries
plus the current user-override layer. Only source keys documented by the
PowerDevil 6.6.6 KConfig files are admitted. Unknown actions, unsupported
profile IDs, malformed booleans, and out-of-range or non-minute durations
produce no imported value.

Settings1 reads the production powerdevilrc path only after winning the
service name. It commits imported values and the completion marker in one
SettingsRepository transaction. The current native user layer has priority.
The existing global power.idleDisplayOffMinutes preference wins over a
legacy display timeout and initializes all source-specific display-off
durations. Missing or invalid input and a persistence failure leave the marker
unset for retry. Import is configuration migration only; it performs no
logind or Power Profiles action.

The evidence is PowerDevil 6.6.6's PowerDevilProfileSettings.kcfg
(Display, SuspendAndShutdown, and Performance groups),
PowerDevilGlobalSettings.kcfg (BatteryManagement group), and
daemon/powerdevilenums.h. The profile file provides lid, auto-suspend, dim,
display-off, lock-before-off, and per-source profile settings; the global file
provides BatteryCriticalAction. PowerButtonAction values are NoAction=0,
Sleep=1, Hibernate=2, Shutdown=8, LockScreen=32, and TurnOffScreen=64. Only policy-admitted actions are converted. The critical
countdown has no key in the cited global settings schema, so the bounded
QindaQt countdown default remains in force. Existing screensaver keys remain
unchanged.

## Consequences

- Settings1 is the durable preference authority for later Power1 and idle
  policy runtime.
- A user choice already stored by QindaQt wins, including its existing global
  display-off preference.
- Malformed and unsupported legacy values cannot become false, zero, or an
  unreviewed power action.
- The marker and preferences share one atomic write; storage failures are
  retryable on the next service activation.
- Runtime policy, the UI, and PowerDevil retirement remain later PF slices.

## Revisit when

Revisit admitted profile identifiers if Power1 exposes provider profile IDs
outside power-saver, balanced, or performance, or if a future PowerDevil
version changes its KConfig keys or action enum mapping.
