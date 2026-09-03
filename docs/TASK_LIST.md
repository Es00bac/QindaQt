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
remaining Display1 nested convergence and hardware work,
production Power/brightness upstream adapters,
Network credential/profile/radio mutation, persistence, external secret-agent
integration, and hardware qualification over the resident Network N1 boundary,
production BlueZ/UI over the Bluetooth B0 boundary, private clipboard history,
display color, font application,
host portal selection/toolkit reaction, and every non-Settings portal family.
The standard Settings appearance backend is now an executable integrated
foundation. Existing Audio1, Display1 foundations, and
resident Power PB-1 remain preserved integrated foundations; Network N1 now
owns the confined production NetworkManager transport and an installed public-
client-only Settings route, while Bluetooth B0 is an executable bounded
foundation without a production platform backend or UI.

### First-party experience delivery queue

Finish QQ-006 through the durable
[First-party queue](../ops/team/queues/first-party.md): complete Settings routes,
later File Manager and Terminal capabilities, application migrations, and
cross-app responsive, DPI, visual, keyboard, and accessibility qualification.
QST-1, Controls, AppShell, Text Editor, the read-only local File Manager S0,
the single-session Terminal S0, and live Notifications, Appearance, Display,
and Network Settings routes remain preserved integrated foundations.

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

- `6cef8b5` — Power PB-2 adds the production upstream adapters behind PB-1's collaborator seams: UPower with
  correct line-power/PowerSupply/battery semantics, logind actions re-authorized at dispatch time with
  generation-fenced exactly-once completion across restart, power-profiles, and an injected-root backlight
  adapter, all selected by the composition root in production with an explicit deterministic mode
  (ADR-0060). Ida Holz (OpenAI Codex, a different worker) rejected the first two candidates at `0/2/1/1`
  and `0/1/0/0` and accepted the second repair at `0/0/0/0`. The fresh merged tree passes the power selector
  26/26 in Debug and Release under an unreachable host system bus, the broad safe Debug suite passes 403/403, and all static gates. QQ-005.03
  stays EXECUTABLE with production adapters; physical hardware, suspend/resume, and the Settings page remain.

- `14f3e67` — The production Audio applet composes only the public AudioClient through a shell-private
  controller with separate read and control grants, audited manifest/registry/host/profile routing, compiled
  keyboard-accessible QML, and exact-owner replacement fences. Integration exposed that the shell's new
  Controls/Tokens link dependency was not staged by the Power, Bluetooth, or default install components;
  the accepted staging-closure descendant ships that closure in every shell-carrying component and adds a
  component-closure row. Reviews: `caaf7d9` accepted `0/0/0/0` on Codex, `b623b00` rejected `0/1/0/0`,
  `14f3e67` accepted `0/0/0/0` by Frances Bilas (OpenAI Codex). The fresh merged tree passes applet 22/22 and
  integrity/runtime/closure 7/7 in Debug and Release, the broad safe Debug suite passes 396/396, and all static gates. This advances
  QQ-004.12 from WIRED to EXECUTABLE; physical devices and nested panel interaction remain.

- `c33b490` — Portal P1 proves host frontend selection and toolkit reaction with the real `xdg-desktop-portal`
  1.20.4 on a private bus: the QindaQt Settings backend is selected only under `XDG_CURRENT_DESKTOP=qindaqt`,
  the frontend's values and `SettingChanged` follow the QindaQt projection, Qt's `xdgdesktopportal` platform
  theme reacts live, and every non-Settings family routes through an explicit fail-closed fallback table
  (ADR-0059). Gertrude Blanch (OpenAI Codex, a different worker) accepted it at P0/P1/P2/P3 `0/0/0/0` with
  9/9 in Debug and Release; the fresh merged tree repeats 9/9, the broad safe Debug suite passes 391/391, and all static gates. QQ-005.09
  stays EXECUTABLE with host selection and Qt reaction now proven; GTK/Flatpak reaction and the other
  portal families remain.

- `63e884c` — The Clipboard C1 service adds a bounded `ext-data-control-v1` capture adapter, the private-bus
  `org.qindaqt.Clipboard1` protocol and exact-owner client with bounded remembered-request eviction, and a
  resident host that captures only after an explicit user-override opt-in (the Settings1 schema default is
  now `false`), withdraws truth unless the authenticated lock state is Unlocked, and ships activation
  artifacts. Evelyn Berezin (Kimi K3-256k) rejected the first candidate at `0/1/1/3` for first-start capture
  without consent and permanent request-cache exhaustion; Ruth Lichterman (OpenAI Codex) accepted the
  repair at `0/0/0/0`. The fresh merged tree passes clipboard 14/14 and Settings-related 29/29 in Debug and
  Release, the broad safe Debug suite passes 389/389, and all static gates; the candidate's ADR is renumbered to 0058. This advances
  QQ-005.06 from WIRED to EXECUTABLE; applet composition, live nested capture, and persistence remain.

- `f44919a` — The production BlueZ adapter reaches `org.bluez` through an injected direct-QtDBus
  connection behind the accepted AdapterBackend port, driving only adapter power, the reference-counted
  discovery lease, and paired-device connect/disconnect while BlueZ keeps pairing and trust authority;
  owner loss retires truth, hostile properties are bounded, and the composition root defaults to production
  with an explicit deterministic mode. Betty Holberton's independent Kimi K3 exact review accepted it at
  P0/P1/P2/P3 `0/0/0/2` with 14/14 rows in Debug and Release under an unreachable host system bus. The fresh
  merged tree passes 15/15 Bluetooth rows including the whole-repository staged-install row in both profiles,
  the broad safe Debug suite 379/379, and all static gates; the candidate's ADR is renumbered to 0057 to
  follow the already integrated ADR-0056. QQ-005.05 stays EXECUTABLE with a production backend; physical
  radios, pairing UX, Settings UI, and hardware qualification remain.

- `882cc0c` — The production Bluetooth applet B1 composes only the public Bluetooth client
  through a shell-private controller with separate read and control grants, audited
  manifest/registry/host/profile routing, compiled keyboard-accessible QML, and exact-owner
  replacement fences. Its controller surface is now proven by a compiled QMetaObject contract
  with token-paste, public-slot, and enum negative controls instead of the former regex gate,
  while textual composition-chain and dependency-policy contracts keep their independent
  poisons. After a 2/2 split on the regex ancestor, the same reviewers Cecilia Payne (Kimi K2.7)
  and Chien-Shiung Wu (Kimi K3-256k) accepted the repair descendant at P0/P1/P2/P3 `0/0/0/0`.
  The fresh merged tree passes Bluetooth 8/8 and adjacent 6/6 in Debug and Release, direct
  boundary gates 7+6 and 5+4, the broad safe Debug suite 373/373 (nested-compositor and host-font visual rows excluded), and all static gates. This advances
  QQ-004.14 from ABSENT to EXECUTABLE; the B0 service still runs the deterministic unavailable
  backend, so BlueZ, pairing UX, nested interaction, and hardware remain later outcomes.

- `7c27ee5` — Global Menu G1 adds the production transports behind the G0 foundation: an exact-owner
  `com.canonical.AppMenu.Registrar` service on an injected bus keyed to caller unique names with owner-loss
  retirement, an asynchronous `com.canonical.dbusmenu` client whose decoder bounds depth, counts, and lengths and
  rejects hostile or stale layouts atomically, and a transport coordinator bound to the proof-bound G0 lineage.
  Elizabeth Feinler's independent Kimi K3 exact review accepted it at P0/P1/P2/P3 `0/0/0/1` after reproducing
  Debug and Release 16/16 and staging hostile second-owner, decoder, and stale-revision reproductions. The
  fresh merged tree repeats 16/16 in both profiles; docs, strict MkDocs, shape, diff, and Team Board gates pass.
  Shell runtime instantiation, applet wiring, submenu popups, and installed-session proof remain later work, so
  QQ-004.06 stays EXECUTABLE with a wider stopping point.

- `4d4b3dc` — The private S3 desktop executes the production compositor,
  shell, resident services, Settings, and Text Editor across WUXGA, 1440p at
  125%, 1080p at 150%, light/dusk/dark themes, and a dual-output arrangement.
  Exact shell-readiness joins prove the active GlobalAccel component/action,
  stable unique shell owner and PID, closed/hidden center before the sole
  private-seat Meta+N batch, open/visible center with an increased counter
  after it, and one mapped compositor surface on the selected output. Mina
  Shah's external Claude source/archive review accepted the immutable
  candidate at P0/P1/P2/P3 `0/0/0/2`; Lise Meitner's independent fresh
  static+dynamic review accepted it at `0/0/0/0` after an 882/882 build,
  focused 5/5, formerly failing 1080p@150% 2/2, and package-plus-matrix 5/5.
  Four fresh archives prove containment 12/12, bounded PSS, nontrivial
  captures, empty teardown, and exact dual `[WL-1, WL-0]` authority with
  interaction and capture on WL-1. Fresh merged-tree replay also builds
  882/882, passes focused 5/5, formerly failing 1080p@150% plus package 2/2,
  and an unretried package-plus-four-row matrix 5/5; its four captures are
  byte-authenticated and visually coherent, all PSS values remain below the
  1,024 MiB ceiling, and final process inspection is empty. This advances
  QQ-004.09 and QQ-006.09 to
  EXECUTABLE without claiming complete screen-reader/keyboard coverage,
  heterogeneous mixed scaling, physical devices, GPU/DRM, hotplug, or
  perceptual baseline qualification.

- `6f5d0ba9` — The installed Network Settings route composes only the public
  Network1 client into bounded, secret-free inventory and admitted saved-
  profile actions. Barbara Liskov's external Claude exact rereview accepted
  the repair at P0/P1/P2/P3 `0/0/0/3` after directly closing all four former
  P2 findings. Fresh manager Debug and Release each build 2,036/2,036 and pass
  mutation 5/5, affected 14/14, public package/policy 5/5, Network 25/25, and
  Settings 9/9. Credentials, secret-agent ownership, profile editing, radio
  mutation, persistence, session-runtime proof, and physical qualification
  remain separate outcomes, so QQ-006.05 remains WIRED.

- `9f59a77a` — The standard Settings v1 portal backend exports confirmed
  QindaQt Settings1/QST appearance through the standard desktop-portal
  endpoint, with exact owner/epoch withdrawal, bounded standard values,
  activation, hardened service packaging, and complete Settings-only source
  and staged metadata. Frances Allen's exact rereview accepted the repaired
  descendant at P0/P1/P2/P3 `0/0/0/0` after independently proving the former
  installed Background escape is closed. Debug and Release review and fresh
  manager gates each build 97/97 and pass the contained 7/7 selector; docs,
  strict MkDocs, shape, package, provenance, and residue gates pass. This
  advances QQ-005.09 from ABSENT to EXECUTABLE without claiming host frontend
  selection, toolkit reaction, or any non-Settings portal family.

- `89557a0a` — Notification popup and center output selection now consumes the
  exact-owner ordered public compositor-output authority and accepts it only
  when generation and complete output-ID sets match accepted shell visibility
  and Qt inventory. Missing, stale, malformed, replaced, or unmatched truth
  clears both roles instead of falling back to Qt's stale primary screen.
  Charles Babbage's independent review accepted the immutable repair with
  P0/P1/P2/P3 `0/0/0/1`; fresh manager-tree Debug and Release each build
  458/458 bounded actions and pass the complete 20/20 adjacent selector. The
  remaining P3 prose precision is corrected here. The real dual-output
  layer-surface transfer and broader whole-shell matrix remain S3 work, so
  QQ-004.09 stays WIRED.

- `9a7872ae` — Display D6 packages the authenticated resident composition of
  D2 service, D4 public writer, D5 durable journal, compositor peer, lock, and
  logind authorities. Typed observation disposition preserves live truth and
  active transactions across benign same-owner rejection while failed owner
  replacement still withdraws stale authority. Mary Jackson's exact rereview
  accepted the repaired descendant after both former P1 defects closed.
  Fresh manager Debug/Release targeted builds complete 197/197 and 228/228;
  both pass hostile service 19/19, focused D6 7/7, and adjacent
  D0-D6/session-lock 40/40. Nested convergence, mixed/physical
  outputs, resources, suspend/hotplug, and hardware qualification remain.

- `fa22af50` — GCC 15.3 strict Release portability repair for the
  customization-editor panel-step value. The accepted two-path change
  aggregate-initializes the exact panel/zone/null-anchor tuple and avoids the
  diagnosed nested-optional inactive-storage move without suppression or
  semantic drift. Grace Hopper's exact review found P0/P1/P2/P3 `0/0/0/0`,
  reproduced the parent failure at action 59/85, and passed strict Debug and
  Release 85/85 builds, the full selector 6/6, and the direct repaired row 3/3
  in each profile. QQ-004.08 remains WIRED pending its Settings canvas,
  live-shell binding, reveal presentation, rendered matrix, and installed
  session outcomes.

- `aebc4fd3` — Resident Network N1 ownership, public Qt transport, confined
  libnm NetworkManager adapter, activation/package lifecycle, exact upstream
  owner-generation retirement, queued delayed-reply dispatch, and conservative
  scan-lease truth. Katherine Johnson's independent rereview passed with
  P0/P1/P2/P3 `0/0/0/0`, strict Debug and Release 21/21 each, 40/40 repeated
  mutation-sensitive executions, and boundary/six-poison/installed lifecycle
  3/3. Fresh manager Debug and Release each build 111/111 focused actions and
  pass the complete 21/21 Network selector. UI, persistence, external
  secret-agent/credential interaction, physical radios, distribution policy,
  and hardware qualification remain later.

- `9e4b7a60` — Production Power applet composition over the public PB-1 client,
  with separate read/control grants, exact-owner and pending-operation fences,
  audited manifest/registry/host/profile routing, compiled keyboard-accessible
  QML, and installed relocation/source-poison proof. Ada Lovelace's exact
  descendant rereview passed with P0/P1/P2/P3 `0/0/0/0`; fresh manager-tree
  Debug and Release each build 327/327 focused actions and pass 14/14 combined
  applet/host/Power rows plus 11/11 direct manifest cases. Live upstream
  providers, successful host mutations, nested interaction, and physical
  hardware remain later outcomes.

- `2f429c11` via manager merge `7277771a` — First-class Display Settings route
  over the public D3 client/coordinator with bounded draft/topology validation,
  preview/confirm/revert, authoritative coordinate refresh, keyboard-accessible
  output selection and integer coordinate commits, and installed route/package
  composition. The exact docs-only descendant passed terminal independent
  rereview P0/P1/P2/P3 `0/0/0/0`; its fully exercised parent passed strict
  Debug/Release builds 328/328 each, focused 12/12 each, compiled page 10/10
  each, interaction probe 7/7, and four mutation controls. Fresh merged-tree
  verification repeats 328/328 and 12/12. Resident writer composition, nested
  convergence, physical displays, and live assistive technology remain later.

- `acd0168` — Display D5 adds a separate crash-safe filesystem journal adapter
  with an injected, ownership-validated state root; fixed bounded paths;
  canonical hostile-input decoding; mode-0600 exclusive temporary writes;
  file and directory durability barriers; atomic replacement; safe load and
  clear behavior; and typed post-commit durability uncertainty propagated
  through D4 into D1. An already-absent clear retries the directory barrier,
  and composed recovery stays cleanup-only `Stuck` with zero compositor apply
  requests until durable absence is proven. Independent exact rereview passed
  P0/P1/P2/P3 `0/0/0/0`, strict Debug and Release 12/12, direct lifecycle 4/4,
  adjacent D2/D3 10/10, package, docs, shape, lineage, provenance, and residue
  gates; manager replay built 130/130 focused actions and passed 12/12 plus
  adjacent client 5/5. D6 now supplies resident startup recovery/writer
  composition and authenticated lock/logind safety; nested convergence, mixed
  outputs, resource proof, and hardware qualification remain later outcomes.

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
  26/26. Resident writer/journal composition, authenticated lock/logind policy,
  nested convergence, and hardware remain later.

- `c819db8` — Typed asynchronous Display1 client and reversible transaction
  coordinator with exact-owner activation, validated atomic snapshots,
  owner/epoch/revision and late-reply fencing, bounded operation completion,
  installed public/private package proof, and a real private-bus lifecycle.
  The current-manager replay passed exact Gemini review `0/0/0/0`; fresh
  strict Debug and Release manager builds each completed 81/81 targeted
  actions and passed the seven-row D2/D3 selector. The separately integrated
  D4 writer and D5 journal now supply the public mutation and durability
  boundaries, and the Display Settings route is integrated separately;
  resident composition, nested convergence, hardware, and resource
  qualification remain later outcomes.
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
