# QindaQt product task list

This is the outcome-oriented source of truth for active product work. It does
not count assignments, processes, reviews, or partially implemented code as
completion. Architectural detail and long-range milestone state remain in the
[implementation roadmap](wiki/development/implementation-roadmap.md).

## Active outcomes

### Shell and customization delivery queue

Finish QQ-004 through the durable [Shell queue](../ops/team/queues/shell.md):
global menu; launcher, task list, tray, and remaining system applets; direct
WYSIWYG customization; and whole-shell output, DPI, theme, keyboard, and
accessibility qualification. Existing production panels and notification
qualification remain preserved integrated foundations.

### Platform services delivery queue

Finish QQ-005 through the durable [Platform queue](../ops/team/queues/platform.md):
remaining Display1 transaction UI/writer work, resident Power1 and brightness,
network, Bluetooth, private clipboard history, display color, font application,
and portal/policy interoperability. Existing Audio1, Display1 foundations, and
PB-0 remain preserved integrated foundations.

### First-party experience delivery queue

Finish QQ-006 through the durable
[First-party queue](../ops/team/queues/first-party.md): complete Settings routes,
later File Manager capabilities, Terminal, application migrations, and
cross-app responsive, DPI, visual, keyboard, and accessibility qualification.
QST-1, Controls, AppShell, Text Editor, and the read-only local File Manager S0
remain preserved integrated foundations.

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

- `d71fac4` — First-party Appearance Settings S0 as an ordinary
  `qindaqt-settings --page appearance` route with validated theme, scheme,
  font, smoothing, wallpaper, and logical-scale intent; per-key Settings1
  draft/apply/conflict/no-replay truth; complete QST preview; compact
  keyboard/accessibility traversal; and sanitized installed-route packaging.
  Applying those preferences to the compositor, displays, fonts, wallpaper,
  and other applications remains with later platform and convergence slices.
- `3fd3842` — Native QindaQt File Manager S0 with bounded local-directory
  launch intent, asynchronous listing, navigation history, QST/Controls UI,
  keyboard/accessibility metadata, desktop packaging, and a relocatable
  component-only installed runtime. The slice is deliberately read-only;
  mutation, mounts, trash, search, previews, portals, recovery, nested visual
  matrices, and live assistive-technology qualification remain later work.
- `d08747d` — Contained virtual-desktop S0+S1 source boundary: authenticated
  bubblewrap sandbox and staging, exact production package contract, bounded
  simultaneous topology/readiness proof, application/output/input/dock
  identity checks, aggregate 1,024 MiB PSS accounting, authenticated teardown,
  and failure-safe evidence archival. The registered private 1080p boot row is
  not yet qualified and contributes no live-desktop or screenshot claim.
- `3078386` — PB-0 bounded Power1 values/codecs, deterministic aggregate-
  battery policy, result lineage, and pure brightness composition/math with
  fail-closed identity, mirror collapse, integer conversion, installed public
  headers, and focused tests. This is a WIRED pure boundary; resident service,
  upstream adapters, client, UI, hardware, and mutation remain later slices.
- `5c914a6` — Narrow installed `QindaQt.AppShell 1.0` shared boundary with
  atomic action/menu values, lifecycle and injected integration state,
  fail-closed portal replies, close consent, focus reporting, truthful
  degraded/unavailable presentation, accessibility identity, and an installed
  consumer. Real portal adapters, application migrations, and nested/live-AT
  qualification remain later outcomes.
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
