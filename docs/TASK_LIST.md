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

## Completed outcomes

- `a5528f8` — Resident `org.qindaqt.Display1` service and exact-owner
  compositor inventory adapter with restart-unique process lineage, hostile
  A/B/A epoch-reuse rejection, private-D-Bus owner replacement, deadline
  re-arm, and complete observer/name/object teardown. Output mutation remains
  fail-closed pending the later writer/client/UI slices.
- `1b4e284` — Installed live notification interaction qualification: real
  `Meta+N` registration/remapping, keyboard/focus traversal, Settings1
  persistence/failure/replacement, Do Not Disturb and critical bypass, shell
  restart, authenticated private lock privacy, teardown, the complete
  1080p/WUXGA/1440p/125%/150% matrix, and ten repeated 1080p lifecycles.
- `1cd5dab` — Native QindaQt Text Editor S1 with one local UTF-8 document,
  optimistic conflict detection, atomic persistence, QST/Controls presentation,
  keyboard and accessibility metadata, installed packaging, and bounded
  large-document behavior.
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

- Power and brightness implementation from the accepted PB-0…PB-5
  architecture; Bluetooth, network, clipboard, remaining display, color, font,
  portal, and policy platform services.
- The complete applet-based settings center and remaining first-party desktop
  experiences.
- Physical hardware, performance/memory, packaging, recovery, migration, and
  upgrade qualification.
