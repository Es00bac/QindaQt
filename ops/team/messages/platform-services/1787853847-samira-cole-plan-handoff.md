# Platform services: parallel delivery plan and manager handoff

- **Timestamp:** 2026-08-27T12:04:07-06:00
- **Worker:** Samira Cole, platform-services architecture lane
- **Product base inspected:** `dc29c88911f0ed6d381211027f16f46bbf92a07c`
- **Product mutation:** none; the product checkout remained clean
- **Claim:** [1787853568-samira-cole-claim.md](1787853568-samira-cole-claim.md)
- **Open consumer question:**
  [1787853665-samira-cole-consumer-question.md](1787853665-samira-cole-consumer-question.md)
- **Required manager contract read:**
  [cross-lane board contract](../desktop-experience-coordination/1787853412-manager-cross-lane-board-contract.md)
- **Related native-app handoff read:**
  [Juno Park design handoff](../native-application-design/1787853515-juno-park-design-handoff.md)

## Decision

Yes, platform work can proceed in parallel with shell/customization and native
application work, but it should be a portfolio of focused outcomes rather than
one `Platform1` API or one always-resident `qindaqt-platform` process.

QindaQt should leave authoritative state with the supported upstream services:
WirePlumber/PipeWire for media graphs, UPower for batteries, optional
power-profiles-daemon for profiles, logind for session power/brightness,
NetworkManager for connections, BlueZ for Bluetooth, colord for profile
catalog/assignment, fontconfig for font discovery/matching, KWin/libkscreen for
live outputs, and xdg-desktop-portal for sandbox requests. QindaQt adds only
bounded typed adapters, desktop policy, preview/rollback, privacy mediation,
and consumer-facing state that those projects do not provide together.

The immediate scheduling constraint is not architecture; it is path ownership.
The active Settings1 candidate `08c7156c578eaac21116498ed563828be4c1a625`
changes `src/CMakeLists.txt`, `tests/CMakeLists.txt`, all of
`src/apps/settings_center/**`, `src/settings/**`, `src/services/settings_*`,
notification shell paths, MkDocs navigation, TASK_LIST/HANDOFF, and the main
architecture/testing pages. Provider design and leaf-module work are safe now,
but an integration-ready code candidate should start from the accepted
Settings1 merge so it can use the real client and shared registries once,
without churn. That merge is close enough that forcing overlapping CMake/docs
edits now would save little and create avoidable review work.

## Evidence at the inspected base

- Normative architecture already says system UI uses *focused* platform
  services adapting PipeWire/WirePlumber, NetworkManager, BlueZ, UPower,
  logind, colord, and related services
  (`docs/wiki/architecture/overview.md:35-36`).
- Module boundaries already require shell-to-client rather than
  shell-to-implementation dependencies, forbid platform adapters from calling
  QML, and require version/error/timeout/restart semantics
  (`docs/wiki/architecture/module-boundaries.md`).
- The roadmap lists the complete platform-services milestone as Planned and
  makes failure, accessibility, persistence, tests, nested display evidence,
  and owning docs part of completion
  (`docs/wiki/development/implementation-roadmap.md:89-96`).
- The required virtual display matrix already covers fractional scaling,
  portrait, mixed DPI, mirroring, lid closure, negative positions, hotplug,
  reorder, rotation, scale, and primary transfer
  (`docs/wiki/development/testing-harness.md:541-566`).
- Qt 6.11 is required by the project. The inspected host has Qt 6.11.1,
  PipeWire 1.6.6, WirePlumber 0.5.14, UPower 1.91.2, systemd 260.2,
  NetworkManager 1.56.1, NetworkManagerQt/BluezQt 6.27.0, BlueZ 5.86,
  libkscreen/KScreen 6.6.5, colord 1.4.8, fontconfig 2.18.1,
  xdg-desktop-portal 1.22.1, and wayland-protocols 1.49.
  `power-profiles-daemon` and `brightnessctl` are not installed.
- Exact KScreen 6.6.5 headers expose get/set operations and per-output scale,
  rotation, priority, HDR, WCG, ICC profile, brightness, DDC-CI, and automatic
  brightness capabilities. The staging `ext-data-control-v1.xml` is installed.

Installed Plasma applications (`powerdevil`, `plasma-nm`, `bluedevil`,
`plasma-pa`, Plasma workspace) are not proposed dependencies. They mix Plasma
UI/configuration/policy with the same underlying services and would create a
second desktop authority. The focused libraries (KF6Screen,
KF6NetworkManagerQt, KF6BluezQt) and standard daemons are appropriate reuse.

## Dependency graph

```text
                     Settings1 (preference/desired-state authority)
                    /      |          |              \
              Display1   Font1   Portal Settings   Clipboard1 enable
                 |          |                         |
 KWin +       live/preview  fontconfig         authenticated lock state
 libkscreen      |          |                         |
       |         +-----> QindaQt app bootstrap        |
       +---> output brightness <--- Brightness model -+
       |                 ^              |
       +---> ICC apply <- Color1         +-- logind internal panel
                            ^             +-- UPower keyboard backlight
                            |
                          colord

 PipeWire + WirePlumber ---> Audio1 <--- late Bluetooth-audio correlation
 BlueZ --------------------> Bluetooth1
 NetworkManager -----------> Network1
 UPower + optional PPD + logind ---> Power1 ---> later lid policy ---> Display1

 xdg-desktop-portal frontend ---> existing KDE backend for broad portals
                              `-> focused QindaQt Settings backend only

 wayland ext-data-control + lock state + Settings1 ---> Clipboard1

 Every accepted client ---> QindaQt.Controls/AppShell ---> Settings page
                                              `-------> narrow shell applet
```

There is deliberately no horizontal dependency between audio, power,
network, and Bluetooth base outcomes. Display is the only prerequisite for
color, output brightness, and lid/display policy. Settings1 is a prerequisite
only where QindaQt owns preference or desired-state persistence, not for
upstream-owned device state.

## Common contract without a god service

Use these semantics in each domain, but do not create a general runtime
controller or a generic `a{sv}` Platform API:

- Each cross-process QindaQt surface has its own name and process
  (`Audio1`, `Power1`, `Network1`, `Bluetooth1`, `Display1`, `Color1`,
  `Font1`, `Clipboard1`). Portal backend interfaces remain freedesktop
  interfaces, not `Portal1`.
- Snapshots are fixed, typed, bounded structs with protocol version,
  service-epoch, and monotonic revision. Lists and strings have explicit
  limits. Generic JSON or unbounded `a{sv}` is not the domain model.
- A change signal is an invalidation. The owner-bound asynchronous client
  coalesces and refetches; it never reconstructs truth from a possibly missed
  signal stream. Upstream/QindaQt owner replacement creates a new epoch and
  invalidates all handles.
- Qt D-Bus calls are asynchronous. Provider public QObjects and publication
  state are owned by the Qt main thread. WirePlumber, colord, and fontconfig
  work is confined to dedicated workers/GMainContexts; only immutable bounded
  values cross queued connections, never raw GObjects or fontconfig handles.
- A mutation result carries its initiating epoch/revision and one typed status.
  Timeout or owner loss is `Uncertain`; the client resnapshots and does not
  replay. Suspend/reboot/poweroff, pairing acceptance, network secret
  submission, display confirm, and clipboard paste are never automatically
  retried.
- Missing upstream daemons or unsupported capabilities publish
  `Unavailable`/`Degraded`; controls disable with an explanation. They do not
  show fabricated defaults. User-safe diagnostics are bounded and do not
  include device secrets, SSIDs marked hidden, Bluetooth keys, clipboard
  payloads, or raw ICC contents.
- Upstream services retain their native persistence and authorization.
  Settings1 stores only QindaQt preferences/desired display or font choices;
  it does not mirror NetworkManager profiles, BlueZ trust keys, WirePlumber
  graph state, UPower readings, or colord's profile database.
- Upstream polkit and session policy remain authoritative. A QindaQt proxy must
  not gain special rules that broaden caller rights. Secrets are never returned
  in a snapshot or logged. Capability-sensitive private presentation (notably
  clipboard history) uses supervisor-provisioned authentication rather than a
  public same-user session-bus method.
- Consumers link only `<domain>_client`/model libraries. QML sees a narrow view
  model/facade, never QDBus, KF6Screen, BluezQt, NetworkManagerQt, GObject, or a
  provider implementation QObject.

Do not add a common code library until at least two accepted providers prove a
truly identical immutable value. The state vocabulary is being coordinated in
the linked consumer question; premature base classes would create the same god
boundary in library form.

## Independent vertical outcomes

### A. Audio1 — audible volume/device control

- **User outcome:** quick-settings and Settings Center show real outputs,
  inputs, application streams, default devices, levels, and mute state; users
  can change volume/default device and move a stream.
- **Reuse/custom:** reuse the running WirePlumber policy manager and PipeWire
  graph through libwireplumber's object manager. Custom code only maps graph
  objects to bounded desktop values and operations. Never run a competing
  policy manager and never transport audio samples.
- **Boundary/API:** D-Bus-activated user process `qindaqt-audio-service`,
  `org.qindaqt.Audio1`. `GetSnapshot`, `SetDefault`, `SetVolume`, `SetMute`,
  and `MoveStream`; use WirePlumber object serial plus service epoch, not
  volatile object IDs, as client handles.
- **Ownership/threading/failure:** Qt main thread owns D-Bus; one confined GLib
  worker owns `WpCore`/object manager. WirePlumber loss resets epoch; in-flight
  mutations become uncertain. Clamp normalized volume and reject stale handles.
- **Paths:** `src/services/audio_protocol/**`, `audio_client/**`,
  `audio_service/**`; `tests/services/audio_{protocol,client,service}/**`;
  later `src/apps/settings_center/AudioPage.qml`, `data/applets/audio.json`,
  and one shell-owned `AudioApplet.qml`/facade.
- **Dependencies/packaging:** Qt Core/DBus, GLib/GObject, libwireplumber 0.5;
  PipeWire + WirePlumber runtime. Install executable, D-Bus activation file,
  and hardened systemd user unit. Do not link `plasma-pa`.
- **Accessibility/consumer:** named device/stream rows, keyboard level steps,
  mute state announced independent of icon/color, peak meter not required for
  control, clear reconnect/busy/error state; compact applet links to Audio page.
- **Evidence:** unit fake backend; hostile protocol bounds; private session
  D-Bus activation/owner replacement; isolated PipeWire runtime with null
  sinks/sources and WirePlumber restart; offscreen QML. Hardware-later:
  USB/HDMI/Bluetooth/jack hotplug, multichannel, microphones, suspend.
- **Docs:** `architecture/audio-service.md`, `reference/audio1-v1.md`, ADR for
  libwireplumber worker/process boundary.
- **Independence:** safe alongside every base provider; Bluetooth-audio
  correlation is a later integration, not a blocker.

### B. Power1 — trustworthy battery, profile, and session actions

- **User outcome:** battery/UPS state and estimates, charging state, supported
  power profiles, inhibitors, and explicit suspend/reboot/power-off actions.
- **Reuse/custom:** UPower device APIs, optional power-profiles-daemon, and
  logind manager/session methods and inhibitors. Custom code composes values;
  it does not estimate battery time independently or replace logind policy.
- **Boundary/API:** D-Bus-activated `qindaqt-power-service`,
  `org.qindaqt.Power1`: snapshot, `SetProfile`, and explicit session actions.
  Keep collaborators `UpowerState`, `PowerProfiles`, and `LogindActions`
  separate. Lid policy is excluded until Display1 exists.
- **Ownership/threading/failure:** Qt main-thread async D-Bus only. PPD absence
  (true on this host) is a supported `Unavailable` capability. Never replay an
  irreversible action; surface polkit challenge/denial/cancel distinctly.
  Delay-inhibitor FDs are owned RAII and closed on cancel/process exit.
- **Paths:** `power_protocol/**`, `power_client/**`, `power_service/**`, focused
  tests; later `PowerPage.qml`, `data/applets/power.json`, and shell applet.
- **Dependencies/packaging:** Qt Core/DBus; runtime UPower + systemd-logind;
  optional PPD with no hard package dependency. No PowerDevil.
- **Accessibility/consumer:** percentage plus charging/time text, no icon-only
  state; destructive actions require keyboard-safe confirmation and announce
  errors; hide unsupported profiles rather than synthesizing Balanced.
- **Evidence:** fake private system bus for UPower/PPD/logind, owner changes,
  multiple battery/UPS aggregation, PPD absent, inhibitor FD lifecycle,
  polkit-style errors; offscreen QML. Hardware-later: laptop battery/UPS,
  suspend/resume, thermal/profile drivers. Nested lid work belongs to the later
  Display1 composition.
- **Docs:** `architecture/power-service.md`, `reference/power1-v1.md`, ADR for
  split upstream authority and no-retry session actions.
- **Independence:** base state/actions can start with Audio1 and do not touch
  shell or display paths.

### C. Network1 — real connectivity without a new network stack

- **User outcome:** connectivity, active connection, Wi-Fi scan, known-profile
  connect/disconnect, airplane/networking state, and later VPN/captive-portal
  status.
- **Reuse/custom:** KF6 NetworkManagerQt over NetworkManager. NetworkManager
  remains owner of devices, connection profiles, activation, persistence,
  routing, and secrets.
- **Boundary/API:** `qindaqt-network-service`, `org.qindaqt.Network1` with
  bounded snapshot, scan lease, activate existing connection, deactivate, and
  radio enable. Split a later `NetworkSecretAgent` outcome for new secured
  Wi-Fi/profile editing; do not put secrets in Network1.
- **Ownership/threading/failure:** Qt main thread. Scan requests are
  rate-limited/leased and cancel when the client or NM owner disappears.
  Connection activation returns an operation ID and is never guessed complete
  from a single property signal. NM owner replacement invalidates all paths.
- **Paths:** `network_protocol/**`, `network_client/**`, `network_service/**`,
  focused tests; later `NetworkPage.qml`, network applet manifest/QML.
- **Dependencies/packaging:** Qt Core/DBus, KF6NetworkManagerQt; NetworkManager
  runtime. Do not use `plasma-nm`. A later secret-agent slice reuses an
  existing Secret Service; it must not invent a credentials database.
- **Accessibility/consumer:** named SSIDs with strength text, security and
  connectivity spelled out, keyboard scan/select/connect, hidden SSID privacy,
  captive-portal link only from authoritative connectivity state.
- **Evidence:** fake NM object tree on a private system bus for lifecycle,
  malformed/oversized properties, scan leases, activation states, owner loss;
  optional isolated network namespace with its own NM only in dedicated CI,
  never host configuration. Hardware-later: Wi-Fi chipsets, Ethernet, VPN,
  metered links, captive portal.
- **Docs:** `architecture/network-service.md`, `reference/network1-v1.md`, ADR
  separating state/control from SecretAgent credentials.
- **Independence:** base status/known-profile outcome is independent of all
  other providers; SecretAgent UI waits for AppShell secure prompt components.

### D. Bluetooth1 — paired-device control first, pairing second

- **User outcome:** adapter power, known device inventory, discovery,
  connect/disconnect of already-paired devices, battery/role status where BlueZ
  provides it; pairing UI follows as a separate reviewable outcome.
- **Reuse/custom:** KF6 BluezQt over BlueZ. BlueZ owns pairing, trust, keys,
  device records, profiles, and authorization.
- **Boundary/API:** `qindaqt-bluetooth-service`,
  `org.qindaqt.Bluetooth1`. Base API has snapshot, discovery lease,
  power/connect/disconnect. Later pairing adds a narrowly scoped BlueZ Agent1
  implementation with explicit request objects and deadlines; it is not folded
  into the inventory controller.
- **Ownership/threading/failure:** Qt main thread; no raw BluezQt QObject leaks.
  Discovery reference-counts client leases and stops on owner/client loss.
  Pairing requests fail closed on prompt loss/timeout and are never accepted by
  default. PipeWire, not Bluetooth1, owns Bluetooth audio nodes.
- **Paths:** `bluetooth_protocol/**`, `bluetooth_client/**`,
  `bluetooth_service/**`; later separate `bluetooth_agent/**`; focused tests;
  page/applet unique paths.
- **Dependencies/packaging:** Qt Core/DBus, KF6BluezQt, BlueZ runtime. No
  Bluedevil. Agent service registration ships only with the later pairing
  outcome.
- **Accessibility/consumer:** device name plus type/connection/battery text,
  keyboard discovery and connect, explicit PIN/passkey/confirmation labels,
  timed prompt announcements.
- **Evidence:** fake BlueZ on private system bus, discovery leases, pairing
  request loss/cancel, owner replacement, malformed devices; optional `vhci`
  only in dedicated privileged CI. Hardware-later: keyboards/mice/controllers,
  BLE, audio profiles, adapter unplug, suspend.
- **Docs:** `architecture/bluetooth-service.md`, `reference/bluetooth1-v1.md`,
  ADR splitting Agent1 and forbidding duplicated trust storage.
- **Independence:** inventory/paired control can run with Network1; pairing UI
  can lag. Audio association is a later consumer join.

### E. Display1 — reversible output configuration

- **User outcome:** identify outputs; arrange, enable, rotate, scale, mirror,
  choose primary/mode/refresh/HDR; preview with a visible countdown; automatic
  exact revert if confirmation is lost.
- **Reuse/custom:** KF6Screen/libkscreen 6.6.5 and its backend launcher over the
  release-matched KWin. Do not bind the Settings app directly to private KWin
  objects or create a second raw output-management implementation.
- **Boundary/API:** resident `qindaqt-display-service`,
  `org.qindaqt.Display1`: `GetSnapshot`, pure `Validate`, `Preview(candidate,
  deadline)` returning a lease/token, `Confirm`, and `Cancel`. Persistent IDs
  derive from connector plus bounded EDID identity, never transient KScreen
  integer IDs. One preview lease at a time; service restart/hotplug/revision
  mismatch reverts or rebases conservatively.
- **Ownership/threading/failure:** Qt main thread owns KScreen operations and
  D-Bus. Settings1 owns the last confirmed desired configuration; live KWin
  state is authoritative for what is actually applied. `Confirm` succeeds only
  after the matching Settings1 transaction commits and a live snapshot matches;
  failure reverts. QindaQt session must not run a competing Plasma KScreen
  persistence policy.
- **Paths:** `display_protocol/**`, `display_client/**`, `display_service/**`,
  focused tests; later `DisplaysPage.qml`. No shell applet is required initially.
- **Dependencies/packaging:** Qt Core/Gui/DBus, KF6Screen pinned compatible with
  KWin; libkscreen backend launcher runtime. A user service must survive a
  Settings Center crash long enough to revert.
- **Accessibility/consumer:** non-color output labels and spatial descriptions,
  keyboard arrangement/rotation/scale, identify action, assertive countdown and
  revert announcement; never require pointer dragging.
- **Evidence:** pure topology validation, KScreen Fake backend, private D-Bus
  crash/timeout/owner tests, then nested KWin for the complete required display
  matrix and real shell work-area reconciliation. Hardware-later: Intel/AMD/
  NVIDIA, real EDID/DDC, MST/docks, lid/hotplug, VRR/HDR.
- **Docs:** `architecture/display-service.md`, `reference/display1-v1.md`, ADR
  for KScreen reuse and Settings1-confirmed preview authority.
- **Independence/order:** core can follow base providers, but brightness,
  color, and lid policy must wait for its accepted contract.

### F. Brightness — compose output and power authorities, no extra daemon

- **User outcome:** one understandable brightness surface for each physical
  display and keyboard backlight.
- **Reuse/custom:** use KScreen `BrightnessControl`/`DdcCi` through Display1 for
  outputs; use logind `Session.SetBrightness` only as the internal-panel
  fallback when the device is unambiguous; use UPower KbdBacklight for keyboard
  lights. Never write `/sys/class/backlight` directly and do not shell out to
  `brightnessctl`.
- **Boundary/API:** no `Brightness1` process. A focused
  `src/services/brightness_model/**` composes `DisplayClient` and `PowerClient`
  values while mutations remain with their owning service. It deduplicates the
  internal panel so one physical output never gets two sliders.
- **Ownership/threading/failure:** GUI/client thread immutable model; each
  underlying client retains its lifecycle. Capability loss removes/disables
  only that device. Coalesce rapid key/slider changes, but never report a
  requested value until an authoritative refresh confirms it.
- **Paths:** `brightness_model/**`, tests, later `BrightnessPage.qml` and narrow
  shell facade; no service activation/package.
- **Dependencies/packaging:** public Display/Power clients only. Direct
  sysfs/logind/UPower logic stays private in those providers.
- **Accessibility/evidence:** named output plus percentage, keyboard steps,
  value announcements. Fake KScreen/logind/UPower and nested KWin first;
  hardware-later internal backlights, OLED/DDC monitors, ambient sensors,
  keyboard devices.
- **Docs:** `architecture/brightness-routing.md`; ADR only if fallback priority
  or automatic-brightness policy becomes durable.

### G. Color1 — profile catalog and compositor application

- **User outcome:** inspect/import ICC profiles, assign a profile to an output,
  select supported HDR/WCG behavior, and see when the compositor cannot apply
  it.
- **Reuse/custom:** colord remains catalog/association authority. Color1
  resolves a selected profile to a validated path and asks Display1 to apply
  its ICC/HDR-capable output state. Do not duplicate profile assignments in
  Settings1.
- **Boundary/API:** lazy `qindaqt-color-service`, `org.qindaqt.Color1` for
  bounded profile/device snapshots, import-by-FD, assign/default/remove user
  relation. Display apply is a typed client dependency, not access to Display1
  internals.
- **Ownership/threading/failure:** colord GLib objects and ICC validation live
  on one confined worker; Qt main thread publishes values. Files arrive by FD
  from a portal selection, with size/type/parse bounds. Colord or Display1 loss
  yields degraded catalog/apply states separately.
- **Paths:** `color_protocol/**`, `color_client/**`, `color_service/**`, tests;
  later `ColorPage.qml`.
- **Dependencies/packaging:** Qt Core/DBus, libcolord/GLib, DisplayClient,
  colord runtime. Sensors/calibration tooling are not v1 dependencies.
- **Accessibility/evidence:** profile names plus origin/quality/default text,
  non-color warnings, keyboard assignment. Fake colord + synthetic bounded ICC
  + fake Display1, private bus, nested apply; hardware-later colorimeters,
  wide-gamut/HDR panels and GPU color pipeline.
- **Docs:** `architecture/color-service.md`, `reference/color1-v1.md`, ADR for
  colord authority and Display1 application split.
- **Order:** after Display1; otherwise independent of other services.

### H. Font1 — fontconfig catalog plus per-process QindaQt application

- **User outcome:** searchable font family/style previews; choose UI/monospace,
  size, antialiasing, hinting, and subpixel order; QindaQt apps consistently
  apply the confirmed choice.
- **Reuse/custom:** fontconfig owns discovery, matching, and raster settings;
  Settings1 owns the QindaQt preference keys. Custom code provides a bounded
  catalog and atomically derives one QindaQt-owned fontconfig fragment. It does
  not create a second font database.
- **Boundary/API:** lazy `qindaqt-font-service`, `org.qindaqt.Font1` for catalog
  and match snapshots. A separate small `src/sdk/app_bootstrap/**` reads the
  Settings client and applies `QGuiApplication` font before first-party QML is
  constructed. Full arbitrary third-party toolkit propagation is a separate
  release decision, not implied by v1.
- **Ownership/threading/failure:** fontconfig config/catalog is confined to one
  worker; immutable results return to Qt. Settings1 commit is authoritative;
  derived fragment replacement is atomic and only its exact owned path may be
  replaced. Missing family resolves through fontconfig fallback with an
  explicit diagnostic, never a hard-coded invisible substitute.
- **Paths:** `font_protocol/**`, `font_client/**`, `font_service/**`, focused
  tests; `src/sdk/app_bootstrap/**`; later `FontsPage.qml`.
- **Dependencies/packaging:** Qt Core/Gui/DBus, fontconfig; activation file.
  Own one documented fragment under the user fontconfig directory and run
  cache refresh only when required by font installation, not preference change.
- **Accessibility/evidence:** preview strings remain readable at selected scale,
  full family/style announced, locale/RTL/sample coverage. Isolated XDG config
  and font dirs, synthetic fonts, cache corruption/fallback, Settings1 owner
  loss, offscreen QindaQt app startup; hardware is not required, but release
  visual baselines cover FreeType/rendering variants.
- **Docs:** `architecture/font-service.md`, `reference/font1-v1.md`, ADR for
  Settings1/fontconfig derived-state and first-party propagation scope.
- **Order:** catalog can develop independently; apply waits for Settings1 and
  coordinates with QST/AppBootstrap.

### I. Portals — reuse the frontend/backends, implement only QindaQt settings

- **User outcome:** sandboxed apps receive working file chooser, open URI,
  notifications, inhibit, screencast/remote-desktop, and desktop appearance
  settings appropriate to the QindaQt session.
- **Reuse/custom:** keep `xdg-desktop-portal` as frontend and initially qualify
  `xdg-desktop-portal-kde` for the broad UI/compositor portals already installed.
  Add only a focused QindaQt `org.freedesktop.impl.portal.Settings` backend so
  color scheme, contrast, and related exported values come from Settings1/QST.
  Do not fork the frontend or create QindaQt FileChooser/ScreenCast backends
  until executable qualification proves the KDE backend insufficient.
- **Boundary/API:** process `qindaqt-portal-settings-backend`, standard backend
  D-Bus name/interface and `.portal` metadata; no QindaQt Portal1. Respect
  xdg-desktop-portal's documented backend selection layering so different
  interfaces can come from QindaQt and KDE.
- **Ownership/threading/failure:** Qt main-thread D-Bus, owner-bound Settings1
  client; missing Settings1 returns documented defaults/unavailable values,
  not stale user state. Portal request handles, cancellation, parent windows,
  and user consent remain with frontend/backend contracts.
- **Paths:** `src/services/portal_settings_backend/**`, unique
  `data/portals/qindaqt-settings.portal` and QindaQt portal configuration,
  focused tests. Existing KDE backend files are not modified.
- **Dependencies/packaging:** Qt Core/DBus, xdg-desktop-portal frontend;
  xdg-desktop-portal-kde initially. Package QindaQt backend separately so it
  can be disabled without breaking the frontend.
- **Accessibility/evidence:** reused chooser/consent dialogs must pass keyboard,
  screen-reader, modality, parent-window, and cancellation tests in the QindaQt
  theme/session. Private session bus + isolated portal frontend + Settings1
  fake; nested Wayland and Flatpak test app for real requests. Screencast/
  remote-desktop qualification additionally needs nested KWin/PipeWire;
  physical multi-monitor/input remains later.
- **Docs:** `architecture/portals.md`, ADR for mixed QindaQt/KDE backend
  selection and replacement criteria.
- **Order:** Settings backend waits for Settings1/QST; existing KDE portal
  qualification can run alongside Display1.

### J. Clipboard1 — bounded, lock-safe history

- **User outcome:** normal clipboard remains Wayland-native; an optional compact
  history can list, reselect, paste, delete, and clear recent items without
  exposing them while locked.
- **Reuse/custom:** Wayland core data transfer for ordinary selection and the
  standardized staging `ext-data-control-v1` protocol for trusted history
  observation/ownership. A focused custom host is justified because no selected
  upstream daemon supplies QindaQt's authenticated lock/privacy and applet
  contract. Pin the protocol XML and generate the Qt client bindings.
- **Boundary/API:** resident `qindaqt-clipboard-host`, private authenticated
  `org.qindaqt.Clipboard1`. V1 history is session-memory only, bounded by item
  count, total bytes, per-item bytes, MIME count/name, and preview length.
  Snapshots expose metadata/short safe previews; full data moves by bounded FD
  only after an explicit user action. `services.clipboardHistory` enables it.
- **Ownership/threading/failure:** Wayland/Qt main thread owns offers and FDs.
  Unknown/locking/locked/session-lock transport loss immediately purges and
  denies every history read/action. Disable or process exit purges. Reject
  password-manager/private MIME hints and unsupported/oversized transfers;
  close all FDs on timeout/client loss. No disk persistence in v1.
- **Paths:** `clipboard_protocol/**`, `clipboard_client/**`,
  `clipboard_host/**`, focused tests; later `ClipboardPage.qml`, manifest, and
  shell facade. Supervisor/session-lock provisioning changes are a later
  coordinated path, not owned by the clipboard worker.
- **Dependencies/packaging:** Qt Core/Gui/DBus/WaylandClient,
  wayland-protocols code generation, KWin protocol support, SettingsClient,
  authenticated SessionLockState. Package host/service only when those two
  prerequisites exist.
- **Accessibility/evidence:** non-content type/source/time labels, keyboard
  search/delete/clear/paste, sensitive-content warning and clear confirmation.
  Protocol tests for fragmented FD reads, MIME/size/count bounds, cancellations
  and producer loss; private D-Bus authentication; nested KWin copy/paste among
  Qt/GTK/XWayland fixtures; lock-transition purge. Hardware is not required,
  but real mixed-toolkit clipboard qualification remains a release gate.
- **Docs:** `architecture/clipboard-service.md`, `reference/clipboard1-v1.md`,
  ADR for ext-data-control privilege, volatile history, authentication, and
  purge policy.
- **Order:** last of the initial services: it depends on integrated Settings1,
  authenticated lock-state provisioning outside the shell, and verified KWin
  protocol support.

## Native-app and shell integration

The active Settings1 candidate introduces `qindaqt-settings`, but its current
`main.cpp`, `Main.qml`, and CMake module accept only the `notifications` route.
The first post-integration native-app outcome should therefore be a serial,
single-owner AppShell/route-registry refactor. Juno's proposed
`src/design_tokens/**`, `src/controls/**`, and `src/appshell/**` are collision-
free and can begin before that refactor; domain pages cannot.

After the route shell is accepted, `AudioPage.qml`, `PowerPage.qml`,
`NetworkPage.qml`, `BluetoothPage.qml`, `DisplaysPage.qml`, `ColorPage.qml`,
and `FontsPage.qml` are independent page paths if each has its own view model
and test directory. One app owner retains `main.cpp`, `Main.qml`, the route
registry, and Settings Center CMake. Shell owners similarly retain shared QML/
CMake/manifest registries; service workers only supply a stable public client
and may own the uniquely named page/applet after explicit assignment.

Every consumer needs `Accessible.name`, role, relationships/descriptions,
keyboard equivalence, non-color state, focus retention, localized strings,
busy/error announcements, and honest unavailable/degraded presentation. Display
preview, pairing, secret prompts, destructive power actions, and clear-history
confirmation require assertive countdown/prompt announcements and safe focus
return. The linked open question asks native/shell lanes to ratify the common
state and route behavior instead of duplicating it.

## Exact path-ownership rule for implementation assignments

A provider implementer receives only its three domain directories
(`*_protocol`, `*_client`, `*_service`), matching focused test directories,
and its unique wiki/reference page. It does **not** own:

- root, `src/`, or `tests/` CMake registries;
- `mkdocs.yml`, ADR index/number allocation, TASK_LIST, HANDOFF, roadmap,
  architecture overview, or module-boundaries table;
- `src/settings/**`, `src/services/settings_*`, or settings schema/data;
- Settings Center `main.cpp`/`Main.qml`/CMake/route registry;
- shell CMake, shared QML module registry, supervisor, lock-state, or applet
  catalog registries.

Those are manager/integrator coordination points. The manager reserves the
next ADR number (current candidate uses ADR-0012), makes minimal additive
registry edits after rebasing each accepted candidate, and updates shared docs
in the same integration. A worker can use a domain-local standalone test driver
during development if required, but no orphan module should be called complete
until the integrated full-tree registry builds and tests it.

## Recommended scheduling with four total concurrency slots

1. **Current convergence:** keep Ada's Settings1 repair/re-review moving. This
   plan and Juno's native design handoff are complete design lanes; do not open
   overlapping product edits before Settings1 acceptance.
2. **First implementation pair after the Settings1 merge:** Audio1 and the
   Power1 base outcome in two isolated worktrees. They have disjoint leaf paths,
   give immediate visible applet/page value, and exercise both GLib-worker and
   Qt-native provider patterns. In parallel, native S1 design tokens can use a
   third worker if a slot is available.
3. **Second pair:** Network1 base and Bluetooth1 inventory/paired control.
   Their secure SecretAgent and pairing-Agent prompt slices wait for AppShell
   prompt components; their base outcomes do not.
4. **Native integration point:** one owner lands AppShell/routes after the
   Settings1 app is stable. Then service pages can fan out in parallel without
   touching the route registry.
5. **Display spine:** implement/review Display1 and run KScreen Fake plus the
   first nested matrix. Then brightness and Color1 run concurrently; Power lid
   policy joins only after preview/hotplug semantics are stable.
6. **Settings-derived services:** Font1/app bootstrap and Portal Settings
   backend can run concurrently once Settings1/QST values are stable.
7. **Privacy-sensitive tail:** Clipboard1 follows lock-state provisioning and
   ext-data-control qualification. Network SecretAgent and Bluetooth pairing
   Agent follow the shared secure-prompt design.
8. **Integration/qualification:** cross-provider Bluetooth audio, lid/display,
   portal screencast, complete nested matrix, then physical hardware and
   performance/memory/package budgets. Virtual/private evidence must never be
   reported as hardware proof.

This keeps at most one risky dependency spine (Display1) active while the
independent providers produce user-visible results. Review remains exact-commit
and cross-worker per AGENTS.md; shared integration should be small relative to
the provider work.

## Primary upstream references

- Qt asynchronous D-Bus and connection ownership:
  [QDBusPendingCall](https://doc.qt.io/qt-6/qdbuspendingcall.html),
  [QDBusConnection](https://doc.qt.io/qt-6/qdbusconnection.html)
- Qt Wayland protocol code generation and QML accessibility:
  [client source generation](https://doc.qt.io/qt-6/qt-generate-wayland-protocol-client-sources.html),
  [Accessible attached type](https://doc.qt.io/qt-6/qml-qtquick-accessible.html)
- WirePlumber architecture/object tracking:
  [design overview](https://pipewire.pages.freedesktop.org/wireplumber/design/understanding_wireplumber.html),
  [object manager API](https://pipewire.pages.freedesktop.org/wireplumber/library/c_api/obj_manager_api.html)
- UPower and power profiles:
  [UPower daemon API](https://upower.freedesktop.org/docs/UPower/),
  [device API](https://upower.freedesktop.org/docs/Device.html),
  [keyboard backlight](https://upower.freedesktop.org/docs/KbdBacklight.html),
  [power-profiles-daemon API](https://upower.pages.freedesktop.org/power-profiles-daemon/gdbus-org.freedesktop.UPower.PowerProfiles.html)
- logind power, brightness, and inhibitors:
  [login1 API](https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.login1.html),
  [inhibitor locks](https://systemd.io/INHIBITOR_LOCKS/),
  [desktop-environment integration](https://systemd.io/WRITING_DESKTOP_ENVIRONMENTS/)
- Linux backlight ABI (used to justify going through logind, not direct writes):
  [kernel backlight ABI](https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-class-backlight)
- BlueZ and BluezQt:
  [Device API](https://bluez.readthedocs.io/en/latest/device-api/),
  [Agent API](https://bluez.readthedocs.io/en/latest/agent-api/),
  [BluezQt Manager](https://api.kde.org/bluezqt-manager.html)
- NetworkManager:
  [D-Bus specification](https://networkmanager.dev/docs/api/latest/spec.html),
  [Settings API](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.Settings.html),
  [SecretAgent API](https://networkmanager.dev/docs/api/latest/dbus-secret-agent.html),
  [NetworkManagerQt](https://api.kde.org/networkmanager-qt-index.html)
- KScreen/libkscreen exact compositor-compatible source:
  [libkscreen v6.6.5](https://invent.kde.org/plasma/libkscreen/-/tree/v6.6.5)
- colord and fontconfig:
  [colord profile API](https://www.freedesktop.org/software/colord/gtk-doc/colord-cd-profile.html),
  [fontconfig developer API](https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-devel/),
  [fontconfig user/configuration reference](https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-user.html)
- xdg-desktop-portal:
  [desktop developer layering](https://flatpak.github.io/xdg-desktop-portal/docs/for-desktop-developers.html),
  [writing a backend](https://flatpak.github.io/xdg-desktop-portal/docs/writing-a-new-backend.html),
  [Settings backend API](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.Settings.html)
- Clipboard protocols:
  [Wayland core data transfer](https://wayland.freedesktop.org/docs/html/apa.html),
  [ext-data-control-v1 staging XML](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/staging/ext-data-control/ext-data-control-v1.xml)

All links above returned HTTP 200 during this read-only research pass. No live
desktop, session/system bus, PipeWire graph, network, Bluetooth adapter, power
action, display, input device, clipboard, or user configuration was queried or
changed. Package/header/source inspection and documentation reads are design
evidence only; no product build, test, or runtime success is claimed.

## Manager next action

Accept this as the platform-services lane plan, route the linked open consumer
question to the next native/shell owners, reserve shared ADR/registry ownership,
and—after the Settings1 exact candidate is accepted and integrated—assign the
Audio1 and Power1 base outcomes from that exact new main commit in separate
worktrees. Their acceptance contracts should quote the corresponding sections
above rather than broadening into adjacent secure-prompt, display, or shell UI
work.
