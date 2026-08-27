# QindaQt product task list

This is the outcome-oriented source of truth for active product work. It does
not count assignments, processes, reviews, or partially implemented code as
completion. Architectural detail and long-range milestone state remain in the
[implementation roadmap](wiki/development/implementation-roadmap.md).

## Active outcomes

### Bounded Audio1 platform service

Applications and future shell/settings consumers can observe and mutate a
bounded audio graph through one asynchronous, versioned Qt boundary without
loading PipeWire or WirePlumber into presentation processes.

Completion requires all of the following on the integrated branch:

1. `org.qindaqt.Audio1` exposes bounded, validated output, input, and
   application-stream state plus mutation results; malformed or stale values
   fail closed without partial publication.
2. WirePlumber and GLib ownership remains confined to a worker boundary. Only
   immutable typed values cross to the Qt service thread, and run generations
   fence queued values across stop, restart, and authority replacement.
3. The public client is asynchronous, exact-owner and epoch/revision aware,
   non-reentrant, cancellation safe, and never replays an uncertain mutation.
4. PipeWire loss, service-bus loss, rapid start/stop, repeated recovery, and
   malformed backend outcomes have deterministic tested behavior with no
   surviving worker resources or orphaned services.
5. Focused Debug, Release, and sanitizer tests; the complete QindaQt registry;
   strict documentation/source gates; and staged private-D-Bus activation all
   pass on an independently accepted exact candidate and again after
   integration with QST-1.
6. The owning architecture, protocol, testing, and ADR pages distinguish the
   backend/service boundary from the later settings page, applet, mixer, and
   real-host audio qualification.

### Live notification interaction qualification

The installed production shell must prove its notification center shortcut,
keyboard/focus behavior, Do Not Disturb persistence across settings-service
replacement, and lock-screen privacy in private nested QindaQt sessions at
1080p, WUXGA, and 1440p. This outcome is incomplete until those real nested
paths pass without injecting input into or modifying the developer's active
desktop.

## Completed outcomes

- `05a8636` — QST-1 semantic design tokens, immutable palette/metric
  derivation, accessibility overrides, read-only QML exposure, and installed
  C++/QML consumer boundaries.
- `c498269` — Persistent notification quieting through generic Settings1,
  including the Notifications settings route, shell projection, restart and
  transport-loss behavior, and independent staged-service qualification.
- `11c1f4b` — Notification presentation is denied unless an authenticated
  compositor-bound lock monitor conclusively reports `Unlocked`; transport
  loss revokes visibility immediately.
- `c93c45e` — Session-scoped Do Not Disturb policy and presentation behavior.
- Hybrid interaction, Compositor MVP, and Foundation are complete as recorded
  in the implementation roadmap.

## Later outcomes

- Power, brightness, Bluetooth, network, clipboard, display, color, font,
  portal, and policy platform services.
- The complete applet-based settings center and remaining first-party desktop
  experiences.
- Physical hardware, performance/memory, packaging, recovery, migration, and
  upgrade qualification.
