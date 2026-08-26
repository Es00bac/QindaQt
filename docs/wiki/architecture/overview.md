# Architecture overview

QindaQt separates compositor authority, desktop presentation, system policy,
and ordinary applications. Components may restart independently where the
Wayland session permits it, and no presentation component owns compositor or
system-service state.

## Runtime shape

| Component | Owns | Does not own |
| --- | --- | --- |
| `qindaqt-wm` | Wayland composition, input routing, outputs, workspaces, client lifecycle, and window-container transactions | Panels, settings UI, applet rendering, or device policy |
| QindaQt Shell | Panels, docks, overview, task presentation, global menu, notifications UI, and direct customization | Authoritative window/output state or privileged hardware changes |
| Settings service | Versioned schemas, preview/commit/rollback, migrations, and change notification | Settings presentation or compositor implementation details |
| Notification host | Standard application submission, bounded active state, expiration, and authenticated presentation snapshots | Popup/history QML, session supervision, or shell authority |
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
- `org.qindaqt.Session1` for snapshot and restore coordination;
- `org.qindaqt.Metrics1` for shared sensor streams; and
- private `org.qindaqt.NotificationPresentation1` for one token-bound shell
  presenter, currently server-side only and disabled without supervisor token
  provisioning; and
- `QindaQt.Applets 1.0` for manifest-defined extensions.

Private Wayland protocols may transport compositor-owned surfaces or efficient
state, but public desktop integrations prefer freedesktop protocols and D-Bus.
All boundaries define failure behavior and version negotiation before they are
treated as stable.

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
- Optional work such as XWayland, indexing, history, effects, and third-party
  hosts starts lazily.
- Every drag interaction has a keyboard-accessible command path.

The KWin foundation and its downstream-maintenance constraints are recorded in
[ADR-0001](../adr/0001-use-kwin-as-compositor-base.md). The native extension
boundary is recorded in
[ADR-0002](../adr/0002-native-qindaqt-applet-api.md). Process-local Hybrid
authority and composed member/shared chrome are recorded in
[ADR-0004](../adr/0004-process-local-hybrid-topology.md).
