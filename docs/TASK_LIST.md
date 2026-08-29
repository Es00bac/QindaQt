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
remaining Display1 durable-journal/resident composition, transaction UI and
nested convergence work, production Power/brightness adapters and UI,
resident Network1/NetworkManager transport over the Network N0 boundary,
production BlueZ/UI over the Bluetooth B0 boundary, private clipboard history,
display color, font application,
and portal/policy interoperability. Existing Audio1, Display1 foundations, and
resident Power PB-1 remain preserved integrated foundations; Network N0 and Bluetooth B0 are
executable bounded foundations without production platform backends or UI.

### First-party experience delivery queue

Finish QQ-006 through the durable
[First-party queue](../ops/team/queues/first-party.md): complete Settings routes,
later File Manager and Terminal capabilities, application migrations, and
cross-app responsive, DPI, visual, keyboard, and accessibility qualification.
QST-1, Controls, AppShell, Text Editor, the read-only local File Manager S0, and
the single-session Terminal S0 remain preserved integrated foundations.

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

- `a8a57a9` — Resident Power PB-1 service/client, exact-owner asynchronous
  transport, installed package, private activation/residency lifecycle, and
  fail-closed multi-domain publication are integrated. Independent exact
  rereview passed P0/P1/P2/P3 `0/0/0/0`, Debug and Release selectors 8/8,
  seven hostile mutation paths, and the collision-clear, battery-disappearance
  and malformed non-resurrection probes. Production UPower/logind/profile/
  brightness adapters, Settings/shell UI, persistence, policy, hardware and
  suspend/hotplug qualification remain later outcomes.

- `26bb7f5` — Private interactive desktop S2 boots the production compositor,
  shell, resident services, Settings, and Text Editor beneath an isolated
  Weston parent at 1920x1080; injects exact `Meta+N` through the private nested
  seat; observes a 440x640 active notification center; captures and validates
  its exact framebuffer region; accounts for all eight QindaQt production
  roles below the 1,024 MiB ceiling; and tears down with zero authenticated
  survivors. Independent exact review passed P0/P1/P2/P3 `0/0/0/0`, a fresh
  2,338-action build, 73/73 units, and both private boot/interaction rows.
  WUXGA, 1440p, fractional scales, theme variants, multi-output, broader
  accessibility, and physical-device proof remain the next matrix outcome.

- `d7691ac` — Display D4 adds a bounded public QtWayland KDE
  output-management writer with complete/surviving-value mapping, exact
  owner/lineage/request fencing, synchronous-callback deferral, restart and
  proxy-lifetime safety, pinned protocol inputs, and an installed poison-tested
  boundary. Independent review passed `0/0/0/0`; fresh integrated-tree Debug
  verification built all 23 executable Display targets and passed D0-D4
  26/26. Durable journal and resident composition, authenticated lock/logind
  policy, Settings UI, nested convergence, and hardware remain later.

- `c819db8` — Typed asynchronous Display1 client and reversible transaction
  coordinator with exact-owner activation, validated atomic snapshots,
  owner/epoch/revision and late-reply fencing, bounded operation completion,
  installed public/private package proof, and a real private-bus lifecycle.
  The current-manager replay passed exact Gemini review `0/0/0/0`; fresh
  strict Debug and Release manager builds each completed 81/81 targeted
  actions and passed the seven-row D2/D3 selector. The separately integrated
  D4 writer now supplies the public compositor-mutation boundary; durable
  journal/resident composition, Settings UI, nested convergence, hardware, and
  resource qualification remain later outcomes.
- `0c9f4b0` — Native Settings Center S1 with a typed bounded route registry,
  stable per-route lifetime, responsive wide/compact navigation, guarded
  unavailable-route focus, keyboard and accessibility paths, sanitized
  installed packaging, and ADR-0048 route ownership. The repaired descendant
  passed exact independent review `0/0/0/0`; Debug/Release passed 9/9, the
  direct fatal-warning page test passed 6/6, the external navigation harness
  passed 5/5, package-isolation poison passed, and fresh manager-tree gates
  passed. Remaining platform pages, drag-from-configuration customization,
  cross-app visual matrices, and live assistive-technology proof remain later.
- `2ae29f3` — Display Color C0 pure model with strict bounded ICC header and
  catalog validation, deterministic capability-aware assignment, canonical
  lineage fingerprinting, and atomic revisioned snapshots. The exact GLM
  repair passed independent Gemini Pro review with `0/0/0/0`; all eight hostile
  reproductions are defeated, strict Debug/Release builds pass 6/6 registered
  rows and 46/46 direct cases, and fresh manager-tree gates pass. Live profile
  discovery/import, persistence, compositor application, Settings UI, nested
  HDR/WCG evidence, and physical hardware remain later slices.
- `ea4d986` — Network N0 bounded protocol, canonical identity/codec/redaction,
  pure lineage/lease/intent model, and injected exact-owner asynchronous client.
  Exact independent replay review passed `0/0/0/0`, Debug/Release 13/13,
  direct 118/118, all eight mutation controls, package poison, 49/49 leaf-byte
  equality, and seven additions-only shared registries. The combined manager
  tree passes 64/64 focused build actions, 13/13 rows, source shape, 99-page
  docs, and strict MkDocs. Resident service, NetworkManager/secret transport,
  persistence, UI, radio mutation, and hardware qualification remain N1+.
- `c08b32e` — Bluetooth B0 bounded protocol/model/client/resident service with
  exact unique-owner lineage, deterministic least-authority backend, bounded
  discovery leases, activation and owner-loss lifecycle, configured D-Bus and
  systemd packaging, and BlueZ-owned pairing/trust authority. Exact independent
  replay review passed `0/0/0/0`, 9/9 including staged install, 70/70 direct,
  package poison, and 54/54 blob identity. The manager tree passes all eight
  source/private-bus rows plus source shape, 96-page docs, strict MkDocs, and
  Team Board 17/17. Production BlueZ, physical adapters/rfkill, Agent1 UX,
  Bluetooth audio, suspend/hotplug, UI, and hardware proof remain later.
- `4f99a7f` — Native QindaQt Terminal S0 with an owned child PTY, nonblocking
  teletype bridge, bounded launch policy, deterministic child/PTY teardown,
  QindaQt theme projection, truthful selection/copy behavior, qtermwidget
  confined behind one adapter, desktop metadata, and relocatable installed
  packaging. Exact independent review passed with zero findings; the manager
  tree passes 63/63 build actions, 9/9 registered rows, 7/7 appearance and 4/4
  real-adapter cases, source shape, 93-page docs, and strict MkDocs. Multiple
  tabs/profiles, settings persistence, AppShell/global-menu migration, whole-
  application accessibility, and the nested screenshot matrix remain later.
- `d0e0809` — The native QindaQt Text Editor now consumes
  `QindaQt.AppShell 1.0` through a typed action catalog, fail-closed file
  selection request/result bridge, lifecycle/integration projection, and
  consumer-owned native picker adapter. Exact independent Debug/Release,
  hostile coordinator, component-only package/RPATH, source-policy, adjacent
  application, documentation, and manager-tree verification pass. This does
  not claim a real portal transport or global-menu exporter.
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
  headers, and focused tests. PB-1 now composes this foundation into the
  resident executable boundary recorded at `a8a57a9`.
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
  fail-closed pending durable journal/resident writer composition and UI; the
  typed client is integrated separately at `c819db8` and the bounded writer at
  `d7691ac`.
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

- Production Power and brightness adapters, policy, UI, persistence and
  hardware work from the accepted PB-0…PB-5 architecture; Bluetooth, network,
  clipboard, remaining display, color, font,
  portal, and policy platform services.
- The complete applet-based settings center and remaining first-party desktop
  experiences.
- Physical hardware, performance/memory, packaging, recovery, migration, and
  upgrade qualification.
