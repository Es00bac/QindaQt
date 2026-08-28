# ADR-0020: Authenticate private live-session evidence

- **Status:** Accepted
- **Date:** 2026-08-27
- **Owners:** Shell runtime, compositor integration, and session qualification
- **Supersedes:** None
- **Superseded by:** None

## Context

Notification qualification must distinguish a successful input or D-Bus call
from its user-visible result. The production objects that know whether the
center received focus, QML status settled, or a layer surface actually mapped
live in separate shell and compositor processes. Screenshots cannot
deterministically prove those facts, and a test that reaches into raw QML or
mutates presentation state would bypass the production keyboard, Settings1,
notification-host, and privacy paths it is meant to qualify.

A general production diagnostic would expose user-surface metadata and create
a long-lived introspection API. Trusting only an environment variable would
also let inherited state open that boundary outside the disposable session.

## Decision

QindaQt admits two complementary, read-only evidence views only inside an
explicit launcher-created development scenario:

- Compositor1's `DevelopmentShellSurfaces` reports compositor-owned state only
  for the exact `notification-popup` and `notification-center` scopes. It
  rejects with `control-disabled` before inspecting KWin state in production.
- The production shell conditionally owns `org.qindaqt.ShellDevelopment` at
  `/org/qindaqt/ShellDevelopment` with interface
  `org.qindaqt.ShellDevelopment1`. Its sole `Snapshot()` method reports bounded
  presentation/DND state, actual notification-window state and focus chains,
  and cumulative observations of user-visible transitions. It accepts no
  commands and cannot mutate QML, settings, notifications, or privacy.

The shell endpoint is excluded unless all of these checks succeed: the
launcher-supplied development marker is exactly enabled, the session bus is
connected, the well-known compositor service resolves to the supervisor-
provisioned exact KWin PID, and live Compositor1 capabilities advertise both
`development-test` control and enabled development mutations. Failure is fatal
to a development-marked shell so a qualification row cannot silently run with
missing evidence. A production shell never registers this service.

The shell cannot attest that its connected bus is private. That containment is
an external harness obligation: the runner creates a new `dbus-run-session`
under a fresh HOME/XDG tree and refuses inherited session-bus and display
addresses before it launches KWin. The environment marker selects development
mode; it is not treated as proof of bus provenance.

One shell replacement may briefly overlap its predecessor's D-Bus name
release. In development mode only, the supervisor supplies the exact prior
shell PID. The replacement accepts only that current owner, waits at most one
bounded interval for its exact unique-owner-to-empty transition, then attempts
registration with `DontQueueService` and `DontAllowReplacement`. An initial
collision, different PID, racing new owner, timeout, queued result, or replace
attempt fails startup. External PID authentication of the newly registered
shell remains mandatory.

The external driver independently authenticates the evidence service's bus PID
against the supervisor-observed shell PID. It combines shell snapshots with
the compositor view; neither view alone can prove ownership. Every decoded
shell snapshot must still report that authenticated shell PID, so an endpoint
replacement cannot satisfy later observations with a different process.
Counters sample
settled presentation state on a later event-loop turn where QML binding order
matters. Settings1 Saving, confirmed rejection, uncertain loss, and outage are
exercised through the production DND transaction. Notification-host action and
dismiss error counters remain observational only; qualification does not add a
host mutation hook merely to manufacture a rejection.

KScreenLocker 6.6.5 treats a D-Bus `Lock()` as an immediate lock, so its normal
grace interval cannot make an unattended nested unlock deterministic. Before
KWin starts, the harness writes the exact KScreenLocker configuration
`[Daemon] RequirePassword=false` only beneath its fresh temporary
`XDG_CONFIG_HOME`, proves that path remains under the harness root, and reads
the value back byte-for-byte. The source for that exact file/group/key contract
is KScreenLocker tag `v6.6.5`'s `settings/kscreenlockersettings.kcfg`; its
`interface.cpp` selects `EstablishLock::Immediate` for a D-Bus caller and
`ksldapp.cpp` permits `userActivity()` to unlock when password authentication
is disabled. The test still invokes the actual KWin-owned
`Lock()`/`GetActive` methods through the already authenticated unique bus owner,
not a re-resolved well-known name, and unlocks only through compositor-device user
activity. The complete temp root is deleted afterward. No host configuration,
credential, PAM conversation, or password input participates.

## Consequences

The nested workflow can prove mapped role/output/geometry, focus and complete
forward/reverse keyboard traversal, no-focus-steal popups, visible status
results, lock-time clearing, and post-restart PID replacement without relying
on call success or screenshots. Inputs still enter only through the
development compositor device, while notification, KGlobalAccel, Settings1,
token, session-supervisor, and KScreenLocker behavior remains production code.

This adds a deliberately unsupported development D-Bus surface and a small
amount of shell observation state. Its payload is bounded by the production
models and focus traversal has an explicit 128-item cycle limit. The evidence
service has no activation file, installation metadata, mutation method, signal,
or persistence. Production exclusion, exact PID authentication, duplicate-role
rejection, harness-enforced private-runtime/config containment, exact locker-
policy readback, per-snapshot PID binding, unique-owner locker calls, bounded
non-queueing predecessor release, and schema parsing are qualification
requirements.

## Revisit when

Remove or replace this seam if KWin and Qt expose a standardized authenticated
testing protocol that proves the same compositor- and shell-owned facts without
production introspection, or if the notification UI moves into a single process
where an equally narrow process-local observer can provide deterministic
evidence.
