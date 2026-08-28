# QindaQt product task list

This is the outcome-oriented source of truth for active product work. It does
not count assignments, processes, reviews, or partially implemented code as
completion. Architectural detail and long-range milestone state remain in the
[implementation roadmap](wiki/development/implementation-roadmap.md).

## Active outcomes

### Interactive virtual desktop integration

The integrated QindaQt session must boot beneath an isolated parent Wayland
compositor, render the compositor, shell, resident platform services, and test
applications, accept synthetic input confined to its private nested seat, and
produce reviewable screenshots. Acceptance covers 1920x1080, 1920x1200 WUXGA,
and 2560x1440 with representative 100%, 125%, and 150% scale, light/dusk/dark
themes, and at least one multi-output arrangement. The workflow must prove that
it neither connects to nor moves the host desktop pointer.

The initial aggregate idle PSS ceiling is 1,024 MiB. A measured overage remains
a real defect, but optimization beyond that starting ceiling follows reliable
end-to-end boot, interaction, screenshot, teardown, and repeatability evidence.

### Live notification interaction qualification

The installed production shell must prove its notification center shortcut,
keyboard/focus behavior, Do Not Disturb persistence across settings-service
replacement, and lock-screen privacy in private nested QindaQt sessions at
1080p, WUXGA, and 1440p. This outcome is incomplete until those real nested
paths pass without injecting input into or modifying the developer's active
desktop.

## Completed outcomes

- `fac2756` — Bounded `org.qindaqt.Audio1` protocol, asynchronous Qt client,
  resident service, confined WirePlumber worker, deterministic reset
  lifecycle, and isolated null-device runtime and package qualification.
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
