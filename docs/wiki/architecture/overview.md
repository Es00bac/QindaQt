# Architecture overview

QindaQt separates compositor authority, desktop presentation, system policy,
and ordinary applications. Components may restart independently where the
Wayland session permits it, and no presentation component owns compositor or
system-service state.

## Runtime shape

| Component | Owns | Does not own |
| --- | --- | --- |
| `qindaqt-wm` | Wayland composition, input routing, outputs, workspaces, client lifecycle, and window-container transactions | Panels, settings UI, applet rendering, or device policy |
| `qindaqt-session` | Essential host/shell startup, descriptor-only notification authentication, parent-death-witnessed KWin PID provisioning, and coupled process lifetime | Compositor internals, desktop policy, or token persistence |
| QindaQt Shell | Panels, docks, overview, task presentation, global menu, privacy-gated notifications UI and a Settings1-fed interruption-policy projection, direct customization, and shell-wide presentation actions | Authoritative window/output/lock state, global-shortcut conflict/remapping policy, settings persistence, or privileged hardware changes |
| `qindaqt-settings-service` | Active schema v2, v1 migration, copy-on-write user persistence, optimistic revision order, and change notification | Settings presentation, executable attestation, compositor/lock/presenter authority, or session supervision |
| `qindaqt-settings` | Ordinary notifications settings page and honest async save/conflict/error presentation | Shell internals, settings files, or notification host authority |
| Notification host | Standard application submission, bounded active state, expiration, and authenticated presentation snapshots | Popup/history QML or shell authority |
| Audio service | Bounded typed PipeWire graph snapshots and validated controls through the running WirePlumber authority | Samples, devices, WirePlumber policy, PipeWire configuration, or UI |
| Display foundation (D1) | Bounded values, privacy-preserving identity/registry, topology validation, and injected-port preview/revert model | A runtime service, KWin/Wayland mutation, Settings persistence, timers, or UI |
| Session and platform services | Session restore, portals, metrics, audio/network/power/device adapters | Shell layout and application UI |
| Applet hosts | Applet lifecycle and capability mediation | Unrestricted access to shell internals or the compositor |
| First-party applications | User-facing file, terminal, editor, viewer, archive, monitor, and software workflows | Desktop-global authority unavailable to third-party clients |

The detailed source and dependency rules are in
[Module boundaries](module-boundaries.md).

The current compositor slice is narrower than the final ownership shown above.
`qindaqt-wm` is a launcher that replaces itself with pinned KWin 6.6.5, and a
release-matched plugin provides window discovery, experimental D-Bus container
transactions, and an in-process Hybrid topology/input/chrome runtime. Exact
ABI, backend, discovery, proof, and limitation details are in
[Compositor and session integration](compositor-session.md).

## Interaction boundaries

The shell receives authoritative window and output state from the compositor,
then requests atomic mutations rather than editing compositor objects directly.
System UI talks to focused platform services, which adapt PipeWire/WirePlumber,
NetworkManager, BlueZ, UPower, logind, colord, and related freedesktop services.
Applications remain ordinary Wayland or XWayland clients.

Cross-process boundaries are versioned from their first external use:

- `org.qindaqt.Compositor1`, currently experimental, for window discovery and
  atomic container operations; window/output/input inventory is readable in a
  normal session, while external mutations are disabled outside explicit
  isolated development scenarios because caller authentication is not yet a
  supported security boundary;
- `org.qindaqt.Settings1` for transactional settings;
- `org.qindaqt.Audio1` for exact-owner, epoch/revision-bound device and
  application-stream snapshots plus typed controls;
- `org.qindaqt.Session1` for snapshot and restore coordination;
- `org.qindaqt.Metrics1` for shared sensor streams; and
- private `org.qindaqt.NotificationPresentation1` for one token-bound shell
  presenter, with owner-bound asynchronous host/client transport enabled only
  by supervisor descriptor provisioning and projected through bounded popup
  and active/recent shell models plus an injected shell-local interruption
  policy; and
- `QindaQt.Applets 1.0` for manifest-defined extensions.

`org.qindaqt.Display1` is an activated cross-process read/service foundation.
Its resident process projects exact-owner D0 inventory through the pure D1
values and transaction machine, publishes bounded snapshots and invalidation
hints, and owns monotonic preview/confirm/revert scheduling. The packaged port
remains deliberately fail-closed: it has no journal persistence or public KWin
output-management writer, so KWin remains live/restore authority and no
production display mutation is enabled by this slice. See
[Display service](display-service.md).

Private Wayland protocols may transport compositor-owned surfaces or efficient
state, but public desktop integrations prefer freedesktop protocols and D-Bus.
All boundaries define failure behavior and version negotiation before they are
treated as stable.

Shell-wide actions use the KGlobalAccel client boundary supplied by the KWin
session. The shell owns action identity and callbacks; KGlobalAccel owns the
active user mapping and conflicts. Built-in applet QML can reach the same shell
presentation through narrow, action-specific facades without gaining the
underlying notification model or service authority.

Do Not Disturb remains an injected shell-side projection policy and does not
change notification-host admission or the private presentation wire. Settings1
now persists its dedicated v2 key. Shell composition fails quiet before the
first exact-owner baseline and then retains the last confirmed value across
loss; the policy module remains persistence-neutral. See
[ADR-0012](../adr/0012-persist-notification-quieting-through-settings1.md).

Full notification presentation is independently gated by a fail-closed lock-
privacy boundary. The shell accepts `Unlocked` only after a dedicated
asynchronous client proves that the QindaQt compositor service and both
KScreenLocker services have one unique owner whose bus PID equals the KWin PID
provisioned by its direct child supervisor. Unknown, locking, locked, and
transport-failure states expose no notification projections or actions. This
does not create a lock-screen presenter; it protects the ordinary private shell
channel. See
[ADR-0011](../adr/0011-gate-notifications-on-authenticated-lock-state.md).

The implemented Compositor1 methods, signals, and transaction encoding are
tracked in the
[Compositor control protocol reference](../reference/compositor-control-v1.md).

## Design rules

- Resident hot paths use C++ and narrow Qt dependencies. Compiled Qt Quick is
  used for shell presentation; Qt Widgets may be used where it is measurably
  simpler and lighter.
- Declarative profile and theme packages contain data, not executable code.
- Third-party applets run outside the shell process with declared capabilities;
  audited built-ins may run in process.
- Optional work such as XWayland, indexing, persistent history, effects, and
  third-party hosts starts lazily.
- Every drag interaction has a keyboard-accessible command path.

The KWin foundation and its downstream-maintenance constraints are recorded in
[ADR-0001](../adr/0001-use-kwin-as-compositor-base.md). The native extension
boundary is recorded in
[ADR-0002](../adr/0002-native-qindaqt-applet-api.md). Process-local Hybrid
authority and composed member/shared chrome are recorded in
[ADR-0004](../adr/0004-process-local-hybrid-topology.md). Shell-owned global
shortcut registration is recorded in
[ADR-0009](../adr/0009-use-kglobalaccel-for-shell-shortcuts.md). Injected
shell-side interruption policy is recorded in
[ADR-0010](../adr/0010-inject-shell-notification-interruption-policy.md).
Authenticated fail-closed lock privacy is recorded in
[ADR-0011](../adr/0011-gate-notifications-on-authenticated-lock-state.md).
Persistent notification quieting is recorded in
[ADR-0012](../adr/0012-persist-notification-quieting-through-settings1.md).
The Audio1 Qt/GLib ownership boundary is recorded in
[ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md).
Display transaction authority and persistent identity are recorded in
[ADR-0016](../adr/0016-display1-transaction-authority.md) and
[ADR-0017](../adr/0017-persistent-output-identity.md).
