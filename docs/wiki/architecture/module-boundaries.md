# Module boundaries

Module boundaries prevent the desktop from turning into one tightly coupled
shell process. Each module owns its implementation, public surface, focused
tests, and the wiki page describing its contract.

## Source ownership

| Area | Responsibility | Allowed inward dependencies |
| --- | --- | --- |
| `compositor` | Immutable upstream KWin pin, downstream patch inventory and verifier, and checked-in compositor IPC descriptors | Repository tooling and upstream source metadata; never shell implementation |
| `src/workspaces_apps` | XDG application lookup and asynchronous desktop-entry launch | Qt Core, KF6 Service and KIOGui; no persistence, matching, compositor or UI policy |
| `src/workspaces_ui` | Compact native Qt Widgets saved-workspace dialogs backed by public Workspaces values and a borrowed synchronous platform port | Public Workspaces/Core values plus Qt Widgets; never KWin objects, application-launch implementation, persistence policy, or matching/adoption logic |
| `src/compositor/kwin/kwinworkspaceuiport.*` | GUI-thread KWin implementation of the workspace UI port: preselected live snapshots, eligible window inventory, desktop-entry delegation, and one coordinator-backed restore | KWin live registry/runtime plus public Workspaces UI and desktop-app APIs; borrows all collaborators, never reaches into session private state or bypasses topology coordination |
| `src/compositor/kwin/kwinworkspacecontroller.*` | GUI-thread saved-workspace library lifetime, stable shortcut and native palette | Borrows the KWin workspace port; owns dialogs and action, destroyed before the port/runtime; no topology or persistence policy |
| `src/workspaces` | Saved workspace values, slot assignment, container instantiation and atomic workspace storage | Public Core plus Qt Core; never live compositor handles, app launching, shell or QML |
| `src/core` | Pure window-container domain model, mutations, invariants, and persistence-neutral values | Qt Core and the C++ standard library |
| `src/hybrid` | Session-wide window ownership, typed topology commands, and atomic candidate/scene publication | `core` and Qt Core; never KWin objects or input events |
| `src/hybrid_constraints` | Recursive member-size solving and lossless independent-window restore values | `core` and Qt Core; never compositor objects or presentation |
| `src/hybrid_input` | Toolkit-neutral pointer/keyboard docking grab and intent state machine | Qt Core/Gui value types and an injected target resolver |
| `src/hybrid_chrome` | Event-free shared-container render-plan layout, typed hit testing, and Qt painting | Qt Core/Gui/Widgets; never topology mutation or KWin policy |
| `src/decorations` | Loadable KDecoration3 member-window presentation and standard window actions | KDecoration3 and Qt Gui; it does not infer container topology |
| `src/profiles` | Layout-profile schema, validation, migration, built-in data, and the sole strict atomic user-profile file writer | `core` only when shared value types are unavoidable |
| `src/shell_layout` | Pure expansion, collision-free logical geometry planning, and per-output work areas | Public `profiles` values and Qt Core; never shell surfaces, compositor objects, or physical-pixel conversion |
| `src/shell/launcher` | Separate targets: the pure bounded desktop-entry model (validation, deterministic category/search/ranking, pinned/recent identities, launch-intent presentation values) and the L1 runtime adapters (injected-root installed-application scanning with debounced watch and generation fencing, Settings1 pinned/recent persistence, seam-based bounded execution and D-Bus activation, and the shell-private applet controller plus compiled `QindaQt.Shell.Launcher` module) | Pure target: Qt Core only. Runtime target: pure target, public Settings1 client, and Qt Core/DBus/Qml/Quick with filesystem, process, and bus reach confined to the scanner, spawner, and activator adapter files; production composition belongs to `src/shell/runtime`; never settings service internals or third-party process authority |
| `src/shell_customization` | Exclusive editor leases, retained immutable snapshots, manifest-aware mutations, preview/history policy, and atomic candidate validation | Public `profiles`, `applets`, and `shell_layout` values plus Qt Core; never applet execution, shell surfaces, persistence, or settings UI |
| `src/shell_customization_editor` | Presentation-independent customization intents, gesture/revision orchestration, keyboard/accessibility identity, and a narrow profiles-store adapter | Public `shell_customization`, `profiles`, `applets`, and Qt Core; never filesystem policy, QML, shell surfaces, D-Bus, or Settings schema authority |
| `src/shell_visibility_protocol` | Shared size, collection, identifier, and scale limits for the compositor-to-shell visibility wire contract | Qt Core value types only; producer and consumer must never duplicate these limits |
| `src/shell_visibility` | Pure, batch-atomic window-aware panel visibility and reservation decisions | Public `profiles` values and Qt Core; never KWin objects, timers, QML, or layer-shell side effects |
| `src/shell_visibility_client` | Owner-bound asynchronous D-Bus snapshot transport, coalescing, timeout/backoff, and safe-fallback publication | Public `shell_visibility` values plus Qt Core/DBus; never panel geometry, QML, or KWin objects |
| `src/shell_window_actions_client` | Exact-owner authenticated compositor transport: one-in-flight no-replay window actions plus revisioned active-window identity refresh on the same owner binding | Public compositor action/identity values plus Qt Core/DBus; never KWin objects, task-list/launcher/global-menu policy, QML, or automatic mutation retries |
| `src/shell_surface` | Backend-neutral panel and notification logical-surface planning, persistent panel live-set reconciliation, Qt output inventory, and private LayerShellQt adapters | Public `profiles` and `shell_layout` values, Qt Gui/Quick, and LayerShellQt only in adapters; never catalogs, applets, settings, or QML policy |
| `src/shell_orchestration` | Exact output matching, pure cross-module inventory assembly, tokenized reveal/hold interaction state, and runtime panel-plan coordination | Public profile/layout/visibility/surface values and Qt Core; never D-Bus, KWin, LayerShellQt, or QML |
| `src/themes` | Theme schema, validation, token resolution, and built-in theme data | Foundation utilities; never shell objects |
| `src/design_tokens` | Immutable QST-1 semantic derivation and a GUI-thread, read-only QML singleton adapter | Public `themes` values plus Qt Core/Gui; Qt QML only in the adapter; never settings, services, shell, applications, or Kirigami |
| `src/app_appearance` | Pure theme/scheme compatibility resolution and a GUI-thread projection of confirmed Settings1 appearance into one retained validated theme | Public `themes`, design-token facade, and settings client boundaries plus Qt Core/Gui; never settings persistence/service internals, application widgets, QML engines, shell, or platform theme mutation |
| `src/controls` | Compiled `QindaQt.Controls 1.0` token-styled primitives and Qinda-specific form/state presentation | Public `QindaQt.Tokens 1.0` plus Qt Quick, Quick Controls 2, and Layouts; never theme selection, settings, services, shell, applications, LayerShellQt, or Kirigami |
| `src/app_shell` (core, excluding `menu_export`) | Application-owned lifecycle requests, stable action/menu projection, injected integration state, portal-request mediation, and reusable first-party QML window/focus/accessibility presentation | Public QST-1 and `QindaQt.Controls 1.0` plus Qt Core/Gui/QML/Quick; never app domain models, routes, persistence, services, platform portal/dialog code, process exit, shell, compositor, LayerShellQt, or KWin |
| `src/app_shell/menu_export` | Opt-in composition of one AppShell action snapshot, one application window, and one injected session bus into the transport-owned standard dbusmenu server and platform-true AppMenu identity lifecycle, plus the one shared fail-closed `composeFirstPartyMenuExport` entry that File Manager, Terminal, and Text Editor call after their primary window exists | Public AppShell and global-menu protocol/dbusmenu/registrar boundaries plus Qt Core/DBus/Gui; the Qt 6.11 private KDE appmenu hook is confined to one Wayland identity adapter. Never issue canonical owner/epoch/revision lineage, import the shell exporter/selector, declare a dbusmenu interface, reach app domain models or shell runtime/KWin, look up an ambient bus, infer menu hosting, or execute outside `ApplicationCoordinator` |
| `src/applets` | Native applet manifest schema, validation, normalization, and catalog discovery | Qt Core only; it does not load or execute applet code |
| `src/applet_host` | Host selection, capability policy, bounded protocol negotiation, and crash/backoff lifecycle state | `applets` public values and Qt Core; sandbox/process adapters remain separate |
| `src/applet_runtime` | Resolve profile instances through validated manifests, placement, host policy, the compiled built-in registry, and least-authority capability grants | Public `profiles`, `applets`, and `applet_host` values plus Qt Core; never QML, services, or third-party process launch |
| `src/settings` | Immutable-v1/active-v2 schemas, validation, migration, layered resolution, optimistic transactions, change sets, and atomic document codec | Qt Core only; service adapters consume this public model |
| `src/services/settings_protocol` | Generic Settings1 constants, typed outcomes, recursive JSON-native codecs, and resource bounds | Qt Core/DBus only; never settings schema/model, shell, or QML |
| `src/services/settings_service` | D-Bus activation/ownership, user-file lifecycle, copy-on-write persistence, revision authority, and changed-key publication | Public `settings` and settings protocol plus Qt Core/DBus; never shell/QML/lock/presenter authority |
| `src/services/settings_client` | Activation, exact-owner/epoch asynchronous snapshots and writes, timeout/uncertainty recovery, and DND-scoped state projection | Settings protocol plus Qt Core/DBus; never service persistence, shell presentation, or settings files |
| `src/services/portal` | Pure Settings1/QST appearance projection, exact-lineage availability source, standard Settings-backend D-Bus adapter, resident process, appearance-only activation package, and a fail-closed frontend selection policy that routes every portal family explicitly without implementing any non-Settings family | Public settings client, themes, QST-1, and Qt Core/Gui/DBus; policy sources never import D-Bus, and the module never imports Settings persistence/service internals, applications, QML, shell, compositor, or non-Settings portal implementation authority |
| `src/services/display_protocol` | Display1 versioned values, hostile-input limits, semantic validation, canonical byte codec, and QtDBus value serialization | Qt Core and serialization-only Qt DBus; never connection/name/service/XML/client/platform state |
| `src/services/display_identity` | Pure privacy-preserving stable-ID resolution plus schema-v2 registry values and v1 migration | Qt Core only; never EDID acquisition, Settings persistence, runtime UUID authority, or logs of private material |
| `src/services/display_topology` | Pure candidate validation, normalization, logical geometry, mirror projection, canonical fingerprint, diff, and no-op | Public display protocol plus Qt Core; never KWin, Wayland, stored preferences, or mutation |
| `src/services/display_transaction` | Pure one-transaction state machine, journal value/codec, rollback/hotplug/recovery truth, and injected clock/port seams | Public display protocol/topology plus Qt Core; never real clocks/timers, files, D-Bus/Wayland, lock/logind, or QObject providers |
| `src/services/display_service` | Exact-owner D0 inventory decode/projection, Display1 owner/epoch/revision reset model, resident D-Bus object/process, deadline scheduling, and injected transaction-port composition | Public display protocol/identity/topology/transaction plus Qt Core/DBus; never KWin private ABI, Wayland, QML, Settings, filesystem journal, logind, or shell |
| `src/services/display_client` | Exact-owner asynchronous Display1 activation/snapshots, validated atomic publication, serialized operations, timeout/uncertainty fencing, and server-state-projected reversible transaction coordination | Public display protocol plus Qt Core/DBus; never service implementation, compositor writer, Settings, shell, or QML |
| `src/services/display_writer` | Fail-closed Display1 apply mapping, narrow topology and ICC-profile compositor configurations, exactly-one-in-flight lineage/owner fencing, and a direct private KDE public-protocol adapter | Public display service/protocol/transaction plus Qt Core and private Qt/Wayland client integration; never KWin private ABI/store, libkscreen production authority, journal persistence, Settings, shell, or physical outputs |
| `src/services/display_journal` | Canonical Display1 journal file load/store/clear, same-directory atomic replacement, restrictive file/root validation, and the deterministic restart-recovery seam | Public display transaction/writer values plus Qt Core and narrow Linux file operations; never environment path discovery, directory selection/creation, compositor/session state, recovery policy, D-Bus, QML, or KWin |
| `src/services/display_runtime` | Packaged Display1 startup order, explicit user-state-root selection, D1 recovery injection, D4/D5 composition, Wayland-peer-authenticated lock safety, and exact-owner logind delay lifetime | Public display service/transaction/writer boundaries, accepted session-lock service, Qt Core/DBus, and process-local environment inputs; never KWin private ABI/store, Settings, QML, journal format/filesystem implementation, or nested-runtime assertions |
| `src/services/display_color_model` | Pure Display Color C0 bounded ICC descriptor/header validation, deterministic catalog values, per-output capability/assignment-intent evaluation, degraded truth, and fingerprinted atomic snapshots | Qt Core only; never Display1 sibling modules, ICC/profile file or host access, transport, persistence, compositor, QML, or display hardware |
| `src/services/display_color_discovery` | Synchronous ICC profile discovery and user import over injected roots only: bounded header/description-tag reads, deterministic C0 import metadata, origin classification, and atomic ADR-0051-style copies with SHA-256 lineage fingerprints | Public `display_color_model` values plus Qt Core; never root/path discovery, profile body interpretation, Display1 siblings, transport, persistence, settings, QML, or hardware |
| `src/services/display_color_assignment` | Strict per-output color assignment document codec and draft engine plus a Settings1-backed store with draft/apply/conflict/no-replay truth over the public settings client; fail-closed on unusable persisted documents | Public `display_color_model` values and the public settings client plus Qt Core; never settings service/persistence implementations, a second journal authority, Discovery internals, transport, QML, shell, or hardware |
| `src/apps/settings/color` | Color route presentation, public Display1 inventory, ICC discovery/import, Settings1 assignment persistence, and serialized restore/application through a separately owned public display-writer port | Public display client/color discovery/color assignment/display writer boundaries plus Qt Core/QML; never Display1 service internals, KWin private ABI/store, libkscreen, colord, or direct compositor objects in QML |
| `src/services/power_protocol` | Power1 bounded values, canonical/fixed codecs, hostile validation, result lineage, and deterministic aggregate-battery policy | Qt Core and serialization-only Qt DBus; never a connection, service, upstream daemon, platform object, session, or UI |
| `src/services/power_service` | Resident Power1 ownership, generation-fenced upstream collaborator seams (battery/profile/session), atomic last-known-good snapshot orchestration, epoch/revision authority, exactly-once operation completion, and the activation package | Public power protocol plus Qt Core/DBus; core orchestration never names host daemons, Wayland, sysfs, or client implementation dependencies |
| `src/services/power_service/adapters` | Production UPower, power-profiles-daemon, logind session/action, and injected-root sysfs translation plus explicit production/unavailable composition | Public power-service/protocol boundaries and Qt Core/DBus; each adapter is confined to its own injected transport, with no Wayland, libupower, privilege helper, polkit UI, process/thread, client, or Display dependency |
| `src/services/power_client` | Exact-owner asynchronous Power1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty recovery, and stale-reply rejection | Public power protocol plus Qt Core/DBus; never service implementation, upstream daemons, or QML |
| `src/services/session_actions` | Exact-owner, bounded asynchronous availability and one-shot dispatch for Session1 logout, ScreenSaver lock, and login1 suspend/reboot/power-off | Injected Qt session/system bus connections only; owns every Session1, ScreenSaver, and login1 symbol used by presentation, never Power1, service internals, polling, optimistic state, QML, or automatic replay |
| `src/services/brightness_model` | Pure stable-ID fixture validation, mirror-collapsed display/keyboard composition, and integer raw-range conversion | Public power protocol plus Qt Core; never Display1 headers, connector identity, topology input, transport, persistence, clocks, QML, or mutation |
| `src/shell` | Qt Quick wallpaper/panel/notification presentation, production window factories, narrow built-in-applet facades, shell-owned icon/theme-root composition, launcher, Global Menu, Clipboard, Task List, and Status Notifier composition, interruption/privacy-policy composition, and global-action controllers | `core`, `profiles`, `themes`, `applet_runtime`, `shell_layout`, `shell_orchestration`, `shell_surface`, `shell/icons`, public launcher/global-menu/task-list and service clients/models/policies, and focused KDE Framework clients behind private adapters; Global Menu and Task List borrow the one shell-owned exact-owner window-actions client and never create a second compositor binding; Clipboard borrows the scoped Settings1 client and owns only a public Clipboard1 client/transport; Status Notifier owns the S1 watcher service and monitor adapter on the injected session bus behind the applet seam; no service implementations directly; LayerShellQt is limited to the private wallpaper controller exception in [ADR-0078](../adr/0078-own-wallpaper-surfaces-in-the-shell.md), while panel/notification surfaces continue through `shell_surface` |
| `src/shell/audio_applet` | Pure Audio applet projection values plus a separately linked shell-private `AudioClient` controller and compiled QML renderer | Pure target: public Audio1 protocol plus Qt Core. Runtime target: pure target, public audio client, and Qt QML/Quick; never audio-service internals, WirePlumber, PipeWire, GLib, direct platform transport, files, or persistence |
| `src/shell/bluetooth_applet` | Pure Bluetooth applet projection/request values plus a separately linked shell-private `BluetoothClient` controller and compiled QML renderer | Pure target: public Bluetooth protocol plus Qt Core. Runtime target: pure target, public Bluetooth client, and Qt QML/Quick; may confirm/cancel a public pairing prompt, but never imports Bluetooth service/model, BlueZ/BluezQt, host radios, addresses in presentation, files, or persistence |
| `src/shell/power_applet` | Pure Power applet projection/request values plus a separately linked shell-private `PowerClient` controller, injected `SessionActionsClient`, and compiled QML renderer | Pure target: public power protocol and brightness model plus Qt Core. Runtime target: pure target, public power/session-actions clients, and Qt QML/Quick; never power-service internals, host daemons, direct platform transport, files, or persistence |
| `src/shell/global_menu` | Separate focused targets: canonical bounded menu/action values and authenticated active-window provider ownership policy, fail-closed export lineage, Qt Widgets adaptation, AppMenu/dbusmenu transport, exact-owner identity-to-provider composition, and a compiled recursive applet/popup module | Protocol/policy use Qt Core; the Widgets adapter additionally uses Qt Gui/Widgets; transport uses Qt Core/DBus; applet presentation uses Qt QML/Quick. The module never imports KWin/private compositor objects or a second compositor client, and activation crosses the guarded export lineage exactly once |
| `src/shell/global_menu/registrar` | Bounded standard AppMenu registrar residency, exact caller-unique-name ownership, owner/registration generation fencing, owner-loss retirement, and the bus-daemon credential seam | Public global-menu ownership values plus Qt Core/DBus; name ownership occurs only in its explicit composition root; never focus authority, menu decoding, QML, shell runtime, or KWin |
| `src/shell/global_menu/dbusmenu` | Complete standard dbusmenu v4 server, exact-owner asynchronous client calls/signals/properties, hostile recursive-layout conversion, remote-revision high water, and uncertain activation no-replay | Public global-menu protocol/exporter values plus Qt Core/DBus; the server validates lineage-free canonical content, owns stable numeric wire IDs/revisions and the complete filtered/grouped method surface, but never issues authenticated lineage. Never registrar name ownership, focus authority, Qt Gui/Widgets, QML, shell runtime, or action execution outside the standard Event request |
| `src/shell/global_menu/composition` | Injected focused-window-to-registrar join, proof reauthentication, selector/exporter publication, applet snapshot handoff, and guarded exactly-once activation intent routing | Public global-menu applet/dbusmenu/exporter/ownership/registrar boundaries plus Qt Core/DBus; never owns a bus name, discovers focus, imports shell runtime/QML/KWin, or changes toolkit exporters |
| `src/shell/clipboard_applet` | Pure Clipboard applet projection values and the hostile-input snapshot admission gate, plus a separately linked runtime target holding the injected client seam, its in-process C0 model adapter, the shell-private controller, and the compiled QML renderer | Pure target: public clipboard model values plus Qt Core. Runtime target: pure target, public clipboard model, and Qt QML/Quick; never clipboard-service/transport internals, Wayland/X11 selection access, host clipboard engines, D-Bus, files, or persistence |
| `src/shell/task_list` | Pure injected-facts task-list values, atomic batch validation, deterministic grouping/ordering, scope filtering, presentation projection, and stale-id intent arbitration | Qt Core only; never transport, compositor objects, window mutation, or presentation side effects ([ADR-0044](../adr/0044-inject-task-list-facts-into-the-shell.md)) |
| `src/shell/task_list/producer` | Exact-owner asynchronous authenticated `CompositorShell1.TaskListSnapshot` reader, hostile bounded wire decoding, owner/epoch/revision/payload lineage fencing, atomic T0 publication, owner-change truth clearing, explicit unavailable publication, and the read-only operation-authority port | Public task-list and compositor task-fact values plus Qt Core/DBus in the injected-connection transport only; never combines independent Compositor1 inventories, fabricates scope/container truth, imports KWin, shell runtime, or QML |
| `src/shell/task_list/operations` | Serialized exactly-once Compositor1 `Submit`/`ReleaseContainer`/`DockWindows` adapter with injected owner/generation/container authority, transport-lifetime token lineage, canonical reply-lineage matching, and truthful Unavailable/Uncertain results | Public task-list operation-authority port plus Qt Core/DBus; never window-level mutations (the separate authenticated window-actions client owns those), KWin, shell runtime, or QML |
| `src/shell/task_list/applet` | Pure bounded strip projection plus the shell-private task-list applet controller over injected operation and icon-name seams and the compiled `QindaQt.Shell.TaskList` module | Public task-list source/presentation, producer authority, operations, and shell-icon QML boundaries plus Qt Core/Qml/Quick; never a bus connection, KWin, the shell runtime, the window-actions client, files, or persistence |
| `src/shell/status_notifier` | Pure StatusNotifier item values, bounded payload validation, exact-owner keyed registry with generation fencing, validated request intents, and deterministic accessible presentation | Qt Core value types only; never D-Bus connections or name ownership, action execution, QML, or platform objects |
| `src/shell/status_notifier/watcher` | `org.kde.StatusNotifierWatcher` service on an injected session-bus connection: registration keyed to caller unique names, owner-loss retirement, and truthful NameOwnedElsewhere degradation | Foundation `status_notifier` values plus Qt Core/DBus; never the registry, item property decoding, QML, or the host bus by construction |
| `src/shell/status_notifier/item_client` | Asynchronous `org.kde.StatusNotifierItem` property reader with hostile-input decoding, generation-fenced late-reply dropping, and the monitor that feeds the registry through `StatusNotifierEventSink` | Foundation `status_notifier` values plus Qt Core/DBus; never registry internals beyond the sink, dbusmenu rendering, QML, or intent policy (the registry evaluates and revalidates) |
| `src/shell/status_notifier/icon` | Deterministic icon-theme lookup over injected theme roots and bounded ARGB32 pixmap decoding into `QImage` with deterministic fallback | Foundation `status_notifier` values plus Qt Core/Gui; never the network, filesystem writes, D-Bus, or QML |
| `src/shell/status_notifier/applet` | Tray applet slice: a pure projection target (phases, bounded rows, menu preview) plus a runtime target composing registry/monitor/icon renderer behind the injected `StatusNotifierSourceInterface` seam, with the controller and compiled `QindaQt.Shell.StatusNotifier` QML module | Runtime may link the public S1 foundation/item-client/icon targets and Qt Core/Gui/Qml/Quick/QuickControls2; it must not open D-Bus connections or call QDBus APIs beyond the injected `QDBusConnection` type (the wire stays in the S1 monitor); never panel QML or the shell runtime |
| `src/shell/icons` | Confined XDG icon-theme lookup over injected roots, desktop-entry-to-icon resolution reusing the launcher's public pure parser, the engine-installed `image://qindaqt-icon/` provider with deterministic placeholder, the `IconRuntime::install` composition seam, and the compiled `QindaQt.Shell.Icons` module ([ADR-0071](../adr/0072-shell-iconography-confined-xdg-icon-themes.md)) | Public `shell/launcher` parser plus Qt Core/Gui/Qml/Quick/Svg; never D-Bus, KWin, LayerShellQt, the status-notifier modules, shell runtime/QML, the network, or filesystem writes; all filesystem reach stays beneath caller-injected roots |
| `src/compositor` | Persistence-neutral transaction bridges plus the release-matched KWin window registry, generation-retaining output inventory, authenticated panel-owner shell actions, active-window identity, and atomic task-fact projection, development-only virtual-output adapter, topology scene adapter, ordinary chrome pointer router, member/transient policy, lifecycle synchronization, and D-Bus plugin | Public `core`/Hybrid/shell-visibility limits, Qt Core/DBus, and explicit release-matched KWin extension points; never registrar-derived identity or shell presentation policy |
| `src/session` | `qindaqt-wm` option validation, backend command construction, session environment, and KWin process handoff | Qt Core; it discovers plugins but does not import compositor internals |
| `src/session_supervisor` | Essential host/shell child startup, descriptor-only token handoff, shell-PID-authenticated Session1 logout, paced bounded shell recovery, optional restart-once secret-agent supervision, one-shot optional Welcome supervision after shell startup, parent-death-witnessed compositor-PID provisioning, and failure rollback | Public presentation-token protocol, Linux process identity/lifetime syscalls, and Qt Core/DBus; never Welcome preferences or UI, compositor internals, QML, login1, ScreenSaver, or service implementation libraries |
| `src/services` | Settings, session, metrics, notifications, audio, portals, and platform adapters | Shared interfaces and narrowly selected platform libraries |
| `src/services/audio_protocol` | Audio1 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation | Qt Core/DBus only; never transport state, QML, or platform objects |
| `src/services/audio_client` | Exact-owner asynchronous Audio1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty, and stale-reply rejection | Public Audio1 protocol plus Qt Core/DBus; never service implementation, WirePlumber, or QML |
| `src/services/audio_service` | Audio backend abstraction, operation coordinator, resident D-Bus object/process, and confined libwireplumber adapter | Public Audio1 protocol plus Qt Core/DBus and private WirePlumber/GLib worker; never shell/settings UI or PipeWire configuration |
| `src/services/bluetooth_protocol` | Frozen Bluetooth1 and current Bluetooth2 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation | Qt Core/DBus only; never transport state, QML, or platform objects |
| `src/services/bluetooth_model` | Adapter backend port, authoritative epoch/serial/lease/prompt coordination, operation validation, deterministic B0 platform adapter, and restart lineage | Public Bluetooth protocol plus Qt Core/DBus; never QML, shell, local pairing/trust persistence, or D-Bus service ownership |
| `src/services/bluetooth_bluez_adapter` | Exact-owner BlueZ ObjectManager transport, bounded Adapter1/Device1 mapping, caller-scoped discovery, BlueZ-owned device mutations, and one bounded Agent1 prompt | Public Bluetooth model port plus Qt Core/DBus; never service residency, QML/shell/settings, local pairing/trust/record persistence, rfkill, ambient bus lookup inside the adapter, or platform handles in public headers |
| `src/services/bluetooth_client` | Exact-owner asynchronous Bluetooth2 discovery/snapshots, invalidation coalescing, separate ordinary/prompt operation lanes, timeout/uncertainty, and stale-reply rejection | Public Bluetooth protocol plus Qt Core/DBus; never service implementation, BluezQt, or QML |
| `src/services/bluetooth_service` | Resident frozen-Bluetooth1/current-Bluetooth2 object/name ownership, caller-scoped discovery-lease watching, pairing/reply wire methods, process entry point, and activation/hardening artifacts | Public Bluetooth model/protocol plus Qt Core/DBus; forwards authority to BlueZ through its injected backend and never stores pairing/trust, supervises BlueZ, or imports shell/settings UI/QML |
| `src/services/network_protocol` | Network1 bounded values, identity normalization, fail-closed validation/redaction, and canonical byte codecs | Qt Core only; never transport state, D-Bus, NetworkManager, platform objects, credentials, or QML |
| `src/services/network_model` | Pure lineage high-water, scan-lease reconciliation, intent admission, and atomic projection | Public Network1 protocol plus Qt Core; never clocks/timers, transport, platform state, persistence, credentials, or QML |
| `src/services/network_client` | Exact-owner asynchronous snapshots and operations, bounded timeout/retry, uncertain outcomes, and injected transport composition | Public Network1 protocol/model plus Qt Core; never a concrete transport, D-Bus, NetworkManager, radios, credentials, or QML |
| `src/services/network_secret_agent` | Standard NetworkManager SecretAgent residency, exact-owner request admission, bounded interactive prompt, ephemeral reply dispatch, and presence-name publication | Qt Core/DBus/Quick plus QindaQt Controls and Tokens; never Network1, Settings, libnm, persistence, agent-owned storage, or a QindaQt credential interface |
| `src/services/notification_presentation_protocol` | Versioned presentation values, bounded D-Bus decoding, wire limits, restart lineage, 256-bit presenter-token values, and exact one-shot descriptor records | Qt Core/DBus and Linux descriptor syscalls; never notification policy, child lifecycle, host objects, or shell QML |
| `src/services/notification_presentation_client` | Unique-owner binding, asynchronous authentication/snapshots, serialized operations, initiating-revision result validation, bounded error normalization, uncertain-result recovery, timeout/backoff, invalidation coalescing, and stale-reply rejection | Public presentation protocol plus Qt Core/DBus; never host/service implementation or QML |
| `src/services/session_lock_state` | Fail-closed owner/PID-authenticated KWin/KScreenLocker state, asynchronous query/signal fencing, and bounded object-startup retry | Qt Core/DBus and a supervisor-provisioned PID value; never shell, notification, compositor-private, PAM, or QML objects |
| `src/session/powerdevil_lid` | Fail-closed PowerDevil lid-close and power-button preference adapter: writes only the `SuspendAndShutdown` `LidAction`, `InhibitLidActionWhenExternalMonitorPresent`, and `PowerButtonAction` entries of the AC, Battery, and LowBattery profiles and requests one daemon reload (ADR-0132) | Qt Core/DBus, KF6 Config, and the injected session bus; never shell, QML, login1, session-actions, ScreenSaver, or compositor objects |
| `src/services/notification_presentation_policy` | Thread-confined, session-volatile interruption state, total popup admission, and a separate fail-closed private-presentation decision | Public presentation-protocol values plus Qt Core; never transport, host/service implementation, Qt Quick/QML, persistence, or platform lock observation |
| `src/services/notification_presentation_model` | Privacy-gated baseline/no-replay Active, policy-filtered bounded popup, and in-memory Recent projections; monotonic popup expiry; center state; success-only popup removal; rejection renewal; and bounded busy/error lifetime | Public presentation client plus injected interruption/privacy policies and Qt Core; never Qt Quick/QML, LayerShellQt, host/service implementation, persistence, or platform lock observation |
| `src/services/notifications` | Bounded notification policy/model plus a separate freedesktop QtDBus adapter | Qt Core for the domain; QtDBus only in the protocol adapter; never QML or Plasma runtime |
| `src/services/notification_host` | Resident D-Bus ownership, one-shot notification-expiry scheduling, and optional authenticated presentation adapter | Public notification model/adapter and presentation protocol plus Qt Core/DBus; never popup UI, history persistence, sound, token provisioning, or session supervision |
| `src/services/clipboard_model` | Volatile bounded clipboard history: canonical media classification, privacy/opt-in gating, generation-fenced deterministic admission/eviction/dedup/pinning/clear, bounded metadata search, lineage-exhaustion fencing, and value/descriptor codecs | Qt Core only; never transport, Wayland, host clipboard, D-Bus, persistence, clocks, QObject providers, or QML |
| `src/services/clipboard_protocol` | Clipboard1 bounded snapshot/operation values, canonical descriptor reuse, hostile-wire validation, and fixed D-Bus structures | Public clipboard model plus Qt Core/DBus; never transport state, Wayland objects, payload persistence, service ownership, or QML |
| `src/services/clipboard_client` | Exact-owner asynchronous Clipboard1 discovery, atomic snapshots, invalidation coalescing, serialized intents, lineage fencing, and timeout uncertainty | Public clipboard protocol plus Qt Core/DBus and an injected transport; never host implementation, Wayland, payload storage, or QML |
| `src/services/clipboard_wayland_adapter` | Pinned `ext-data-control-v1` selection/primary observation, MIME preflight, bounded asynchronous reads, compositor-peer identity, and explicit selection publication | Public clipboard model values plus Qt Core and public Wayland client protocols; never history policy, D-Bus, Settings1, lock policy, persistence, or QML |
| `src/services/clipboard_service` | Resident volatile history ownership, Settings1 opt-in and authenticated-lock composition, Clipboard1 object/name ownership, caller-scoped request lineage, and activation artifacts | Public clipboard model/protocol/adapter plus Settings1 and lock-state clients; never persistence, shell/UI, compositor-private APIs, or payload logging |
| `src/services/font_preferences` | Pure deterministic font family discovery from injected facts, validated typography preferences, exact-typed lossless codecs, pre-application bootstrap derivation (pure apply/gate helpers), atomic LKG publication, and the F1 Settings1 confirmed-snapshot bridge | Qt Core/Gui and the public Settings1 client; fully transport-free (no Qt D-Bus); never host filesystem scanning, fontconfig, KWin, or QML |
| `src/services/font_discovery` | Sole fontconfig-backed producer of F0 `FontFact` values from an internally built `FcConfig` over injected directories and an injected configuration file, with bounded counts/strings, deterministic ordering, and fail-closed unavailable truth; hosts the F1 production composition root (`FontSessionBootstrap`) that reads confirmed Settings1 `fonts.*` preferences pre-`QGuiApplication` and applies them when the live catalog resolves the family | Public `font_preferences` values, Qt Core, and privately linked fontconfig ([ADR-0067](../adr/0067-confine-fontconfig-behind-font-discovery.md)); Qt Gui/D-Bus and the public Settings1 protocol confined to the session bootstrap composition source; never QML, Qt Widgets, host config mutation, or shell/applications |
| `src/sdk` | Versioned client libraries, schemas, manifests, and generated IPC bindings | Foundation libraries only |
| `src/apps` | First-party applications behaving as normal desktop clients | Public SDK and application-focused libraries |
| `src/apps/text_editor` | Bounded multi-window, single-document text policy, independent optimistic local UTF-8 persistence, bounded find/replace, paths-only restore state, and AppShell action/menu presentation on stock Qt Widgets | Public AppShell, Settings1 client, Qt Core/Gui/Widgets/PrintSupport, and private KF6 SyntaxHighlighting in DocumentEditor (ADR-0095); appearance flows from the Qt platform theme per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md) with no token/palette projection; never settings service/persistence implementation, shell/compositor internals, or another app's private code |
| `src/apps/file_manager` | Local directory listing/navigation, bounded file launch, identity-checked asynchronous local mutation, home Trash/recovery policy, AppShell actions/global-menu opt-in, and stock `QtQuick.Controls` presentation | Public `QindaQt.AppShell 1.0` lifecycle/action seams and AppShell menu-export plus Qt Core/Gui/Qml/Quick/QuickControls2 and local POSIX filesystem calls; presentation uses stock controls themed by the Qt platform theme per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md); never QST/Controls imports in app QML, shell internals, services, mount/per-volume-trash/portal authority, or another app's private code |
| `src/apps/settings_center` | Bounded built-in route descriptors/registry, process-local navigation history, responsive QST/Controls host, and composition of route-owned public models | Public Settings1 clients, route domain models, themes/QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never settings service/persistence implementations, shell/compositor/platform mutation, arbitrary QML route loading, or one shared transport across independently tokened clients |
| `src/apps/settings/appearance` | Strict Appearance values, per-key draft/rebase and save-result truth, pure QST preview projection, and the route's modular QML presentation | Public Settings1 client, themes, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never settings persistence/service implementations, shell/compositor/display/platform mutation, another route's model, or transport access from QML |
| `src/apps/settings/customize` | Route-owned profile/catalog composition, direct editor-session canvas projection, Settings1 selection lifecycle, atomic user-profile persistence, and responsive accessible QML | Public Settings1 client, `shell_customization_editor`, `shell_customization`, `profiles`, applet manifests, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never private repository headers, settings service persistence, shell surfaces, LayerShellQt, compositor/platform mutation, or engine-policy duplication in QML |
| `src/apps/settings/audio` | Public-Audio1 consumer projection for the Settings Audio route: bounded device/stream rows, shared admission truth, closed set-default/volume/mute intents, and the route's modular QML presentation | Public Audio1 client/protocol, themes/QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never the resident audio service, WirePlumber/PipeWire, Qt D-Bus, stream movement, text entry, or transport access from QML |
| `src/apps/settings/bluetooth` | Exact-lineage Bluetooth inventory, route-scoped discovery lease lifetime, admitted device/pairing controls, one inline prompt, and responsive accessible QML | Public Bluetooth client/protocol, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never BlueZ, private Bluetooth service/model headers, Qt D-Bus, local pairing/trust/remove authority, another route's model, or transport access from QML |
| `src/apps/settings/power` | Bounded supply/profile/hold/brightness projection, exact-lineage admitted profile and debounced keyboard-brightness intents, injected session-action presentation, a separate KScreenLocker Autolock/Timeout/LockOnResume/LockGrace preference adapter, plus an injected PowerDevil lid/power-button policy port behind the same fail-closed style, convergence fencing, and responsive accessible QML | Public Power and session-actions clients, pure brightness math, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; only the named composition root constructs transports/clients and the separate screen-lock and PowerDevil-lid adapter ports, while QML/model code never imports resident services/adapters, UPower, login1, ScreenSaver, sysfs, another route, or transport |
| `src/apps/settings/clipboard` | Settings1 history-preference draft/conflict truth, content-free Clipboard1 state/count projection, and exact-lineage confirmed all-history clearing | Public Settings1 and Clipboard1 clients/protocol, QST-1, QindaQt.Controls, and Qt Core/DBus/Gui/QML in the explicit composition root; never clipboard payload/content reads, private clipboard service/Wayland APIs, per-entry mutation authority, another route's model, or transport access from QML |
| `src/apps/settings/color` | Per-display ICC assignment projection over the Display1 inventory, C1 discovery/import catalog, and C1 Settings1 assignment draft/apply with conflict/no-replay truth, exact owner/epoch/revision admission, and responsive accessible QML | Public Display1 client, public Display Color C1 discovery/assignment/model, the public Settings1 client, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; only the named composition root may construct the public Qt transports, and the module never imports compositor/colord authority, Display service internals, another route, or transport into QML/model code |
| `src/apps/terminal` | Terminal launch policy (argv/shell resolution, child environment, hostile-view clamping), bounded single-session PTY lifecycle with exit truth and guaranteed process-group teardown, the application-owned child-PTY bridge, and Qt action/menu presentation on stock Qt Widgets | Qt Core/Gui/Widgets and `qtermwidget6` privately linked and confined to the rendering adapter per [ADR-0040](../adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md); window appearance flows from the Qt platform theme per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md), while the ANSI protocol palette (ADR-0112) remains terminal content derived from the active palette; never shell internals, services, or another app's private code |
| `src/apps/calendar` | Local calendar collection/event policy on KF6 CalendarCore, occurrence expansion, reminder scheduling/delivery, ICS import/export, Settings1 preferences, AppShell action/menu presentation, and stock `QtQuick.Controls` presentation | Public `QindaQt.AppShell 1.0` lifecycle/action seams plus Qt Core/Gui/Qml/Quick/QuickControls2 and private KF6::CalendarCore; presentation uses stock controls themed by the Qt platform theme per [ADR-0116](../adr/0116-build-bundled-applications-on-stock-qt6.md); never QST/Controls imports in app QML, network sync, shell internals, services, or another app's private code |
| `src/apps/welcome` | First-run and manually reopenable desktop tutorial, application-local show-next-launch preference, portable optional hero discovery, and a fixed set of first-party try-it launch actions | Public application-appearance, Settings1 client, QST-1, `QindaQt.Controls 1.0`, and Qt Core/DBus/Gui/QML/Quick; never Settings1 writes, shell/compositor internals, arbitrary process execution, or another app's private code |
| `tools` and `tests` | Isolated development harnesses, fixtures, integration scenarios, and verification | Public APIs; test-only hooks in test builds |

Not every planned directory exists yet. Add one only when its responsibility is
implemented; do not use placeholder modules to bypass a boundary.

## Dependency direction

- `core`, profile, and theme models never import shell, compositor, service, or
  application presentation code.
- QST-1 consumes themes and explicit caller inputs. Theme/catalog selection and
  Settings1 projection remain outside the token module; QML can observe only
  complete generations. See
  [ADR-0013](../adr/0013-own-qst1-semantic-tokens.md).
- The portal appearance policy consumes a complete public Settings1 snapshot
  and public QST-1 derivation; it never reads settings files or imports the
  service repository. Only its private adapter imports Qt D-Bus, and only the
  standard Settings backend interface is registered. Its installed frontend
  selector explicitly orders external fallback providers and keeps unlisted
  families closed; it does not grant this module their implementation
  authority. See
  [XDG Settings portal appearance backend](portal-service.md).
- First-party presentation imports [QindaQt.Controls 1.0](../shell/controls.md)
  explicitly. Controls consume complete QST roles without inspecting theme
  identity or adding fallback palette/timing authority; domain state and
  availability remain caller inputs.
- The KWin-hosted `qindaqt` WindowSwitcher package consumes KWin's public
  TabBox model and the compositor plugin's process-local, read-only semantic
  palette map. Before that map is available it falls back to Kirigami's host
  palette. It imports no shell code, settings transport, theme identity, or
  palette literals. This boundary is recorded in
  [ADR-0089](../adr/0089-present-task-switching-through-kwins-native-model.md)
  and [ADR-0092](../adr/0092-project-confirmed-palette-into-compositor-ui.md).
- Launcher presentation consumes the pure launcher model's values and resolves
  every activation through the catalog's single intent builder. The L1
  adapters in the same module own scanning, seam-based execution, and
  Settings1 pinned/recent persistence behind their own boundaries; none of
  that platform reach moves into the pure model. See
  [Launcher](../shell/launcher.md),
  [ADR-0042](../adr/0042-launcher-model-without-execution.md), and
  [ADR-0062](../adr/0062-bound-launcher-execution-behind-injected-seams.md).
- First-party QML applications may compose
  [QindaQt.AppShell 1.0](../apps/application-shell.md) around app-owned content.
  The application injects action truth, lifecycle decisions, integration state,
  portal results, desktop identity, and QST publication; AppShell never discovers
  or executes those policies. See
  [ADR-0027](../adr/0027-extract-a-narrow-first-party-application-shell.md).
- The compositor publishes state and accepts validated atomic commands. The
  shell does not link to KWin private objects.
- Notification surface routing consumes only the public, owner-bound
  `Compositor1.Outputs` semantic order. The runtime joins its exact
  `outputGeneration` and output-ID set to the already accepted shell-visibility
  snapshot and current Qt inventory before resolving a `QScreen`; it never
  substitutes Qt's platform-local primary-screen guess or compositor-private
  output objects.
- `src/hybrid` owns the process-local session topology; the KWin adapter may
  orchestrate its public coordinator but may not duplicate tree mutation or
  expose KWin pointers through it. The older Compositor1 bridge remains a
  separate per-container compatibility surface. Compositor1 may mirror actual
  Hybrid revisions and value snapshots for read-only observation without
  becoming the interaction transport.
- The shell depends on service clients, not service implementations. Platform
  adapters never call QML objects. In particular, the freedesktop notification
  server is producer-facing; notification presentation requires a versioned
  private resident-host adapter and public shell client rather than an
  implementation-library link. The shell model consumes that public client,
  and QML consumes the public model projection.
- Audio consumers depend on the typed Audio1 client. WirePlumber and GObject
  handles remain on the audio service's dedicated GLib worker; only bounded
  values cross to Qt. A future Settings route may consume the full model, while
  shell QML receives only a default-output facade and settings-opening action.
  See [Audio service](audio-service.md) and
  [ADR-0014](../adr/0014-confine-wireplumber-to-glib-worker.md).
- StatusNotifier exported menu transport privately consumes the public
  `GlobalMenuDbusMenu` client and its bounded canonical tree. Registry owner
  generations plus local menu revisions fence all menu invocations; the
  applet receives copied presentation values and narrow intents, never wire
  handles. The shared client marks retained layouts non-current during reads
  and AboutToShow preparation. See
  [ADR-0114](../adr/0114-status-notifier-actionable-menus.md).
- StatusNotifier items are owned by their bus unique name, never a well-known
  name, and reach the tray only through bounded validation and an injected
  transport seam; the registry records request intents instead of executing
  them, and the item-client monitor dispatches only registry-validated,
  revalidated intents over the injected bus connection. The watcher service,
  item client, and icon renderer live in dedicated submodules that depend on
  the pure foundation, never the reverse.
  See [Status notifier tray](../shell/status-tray.md) and
  [ADR-0032](../adr/0032-status-notifier-exact-owner-foundation.md).
- Current Bluetooth consumers depend on the typed Bluetooth2 client; the
  frozen Bluetooth1 object remains available to existing v1 clients. BlueZ owns
  pairing, trust, keys, device records, and authorization. Bluetooth2 exposes
  inventory, adapter power, bounded caller-scoped discovery leases, device
  operations, and a bounded exact-ID Agent1 prompt/reply projection. It forwards
  every pairing/trust/removal decision to BlueZ and stores none of it. See [Bluetooth service](bluetooth-service.md) and
  [ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md).
- Network consumers depend on the typed Network1 client. The N0 direction
  remains protocol → model → client, with only an injected transport. N1's
  `network_qt_transport` implements that seam without reversing it;
  `network_service` owns fixed-wire D-Bus residency and an injected backend;
  only `network_manager_adapter` links libnm. Its public boundary exports
  secret-free copies and no NM/GObject handle. NetworkManager profile and
  credential authority remains outside Network1. The separately deployed
  first-party process implements the standard SecretAgent contract without
  linking Network1 or exporting secrets on a QindaQt interface. See [Network
  service](network-service.md), [Network secret agent](network-secret-agent.md),
  [ADR-0045](../adr/0045-fence-network1-pure-boundary.md), and
  [ADR-0069](../adr/0069-confine-network-credential-entry.md).
  The adapter may also submit one bounded visible-Wi-Fi profile through
  `AddAndActivateConnection`: Open, `wpa-psk`, or `sae` only, with every secret
  absent and secured PSKs marked agent-owned. Network1 exposes only the opaque
  access-point selector; NetworkManager retains profile persistence authority.
- Display consumers will depend on a typed Display1 client, not these service
  implementation modules. D1's dependency direction is protocol → topology →
  transaction. Identity depends only on Qt Core and is independent of protocol,
  topology, and transaction. The resident service composes those public D1
  boundaries and consumes only D0's public Compositor1 inventory through an
  exact-owner QtDBus adapter; it never links the compositor or its KWin ABI.
  The separate Display runtime composes D2 with D4, D5, authenticated lock
  state, and logind; those dependencies do not leak back into D1/D2.
  KWin remains live/restore authority, Settings owns later registry/policy
  persistence, and shell geometry never waits for Display1. See
  [Display service](display-service.md),
  [ADR-0016](../adr/0016-display1-transaction-authority.md), and
  [ADR-0053](../adr/0053-compose-display1-from-authenticated-runtime-authorities.md).
- Notification interruption policy is injected into the presentation model by
  shell composition. It filters only the popup projection; it cannot mutate the
  host, private wire, Active/Recent retention, or persistent settings. The
  shell's DND-scoped Settings1 bridge drives the policy from confirmed
  snapshots without reversing this dependency; it fails quiet before baseline
  and retains the last confirmed value across service loss.
  See
  [ADR-0010](../adr/0010-inject-shell-notification-interruption-policy.md).
- Session lock observation is a separate public service-client boundary. It
  authenticates the common unique owner of the QindaQt compositor and both
  KScreenLocker names against the supervisor-provisioned KWin PID, then drives
  an injected privacy decision. Privacy denial outranks interruption policy,
  clears every notification projection, and never grants applet or shortcut
  authority. See
  [ADR-0011](../adr/0011-gate-notifications-on-authenticated-lock-state.md).
- The clipboard history model is a pure volatile boundary that a future
  Clipboard1 host composes with Wayland `ext-data-control-v1` transport and
  authenticated lock state; the model itself never touches transport or
  persistence, payload bytes leave it only through an explicit promote, and
  privacy denial purges content behind a generation fence. See
  [Clipboard service](clipboard-service.md) and
  [ADR-0031](../adr/0031-volatile-bounded-clipboard-history.md).
- Built-in applet QML receives a purpose-specific shell facade, never a general
  shell controller or service model. The notification-center entry can request
  a center toggle and observe open plus read-only Do Not Disturb state, but it
  cannot change interruption policy and receives no notification records,
  operations, or service authority. Its manifest therefore requests no
  capabilities.
- The Power applet receives only its shell-private controller over the public
  PowerClient plus the purpose-built `SessionActionsClient`. Manifest/policy
  `power.read` and `power.control` decisions gate Power1 observation and
  mutation separately. Session buttons consume only typed client facts and
  methods; the renderer never sees a bus, service implementation, upstream
  daemon, or reusable general-power object.
- The Bluetooth applet receives only its shell-private controller over the
  public `BluetoothClient`. Manifest/policy `bluetooth.read` and
  `bluetooth.control` grants gate observation and mutation independently. Its
  controller owns one bounded caller-scoped discovery lease, releases it on
  popup close/teardown, and exposes only public prompt truth plus
  confirmation/cancellation; it exposes no pair initiation, trust, key, PIN,
  address, or BlueZ surface. See [Bluetooth applet](../shell/bluetooth-applet.md).
- The Audio applet receives only its shell-private controller over the
  public `AudioClient`. Manifest/policy `audio.read` and `audio.control`
  grants gate observation and mutation independently; the controller clears
  stale truth and pending operations on exact-owner replacement without
  replay, and exposes no PipeWire, WirePlumber, stream-move,
  default-device, or service-internal surface. See
  [Audio applet](../shell/audio-applet.md).
- The Global Menu applet receives only `GlobalMenuAppletAccess`. Its
  composition borrows the shell's existing `ShellWindowActionsClient`, owns
  AppMenu registrar residency, and joins projected PID/window/menu facts to an
  exact dbusmenu owner. Manifest/policy `global-menu.read` gates observation;
  `windows.activate` remains denied. The QML module sees neither bus nor
  compositor transport. See [Global application menu](../shell/global-menu.md).
- The Clipboard applet receives only its shell-private controller over the
  injected `ClipboardClientInterface` seam. The shell composition owns the
  public Clipboard1 client/transport, borrows its scoped Settings1 client, and
  admits history only with exact user-override consent. Manifest/policy `clipboard.read`
  and `clipboard.write` decisions gate observation and mutation separately;
  the renderer never sees the C0 model, a transport, or a reusable
  general-clipboard object. See [Clipboard applet](../shell/clipboard-applet.md).
- The Status Notifier applet receives only its shell-private controller over
  the injected `StatusNotifierSourceInterface` seam. The shell composition
  owns the S1 watcher service and the S2 monitor adapter (registry, item
  monitor, icon renderer) on the shell's session bus and routes the
  degradation acknowledgement through the seam. Manifest/policy
  `status-items.read` and `status-items.activate` decisions gate observation
  and intent dispatch separately; the renderer never sees the registry, a
  connection, or a bus endpoint. See
  [Status notifier tray](../shell/status-tray.md).
- Shell-wide presentation shortcuts are shell-owned actions registered through
  a private KF6 GlobalAccel adapter. KGlobalAccel/KWin owns conflict resolution
  and user remapping; neither profile data nor applet QML may register or
  reclaim those bindings. This focused dependency is accepted in
  [ADR-0009](../adr/0009-use-kglobalaccel-for-shell-shortcuts.md).
- Applets and applications use the SDK and public IPC. They do not include shell
  private headers or assume a specific panel implementation.
- The Terminal application is the only consumer of `qtermwidget6`; the library
  is mandatory for the terminal target, privately linked and invisible to
  every other module, and its integration contract — including the
  application-owned child-PTY bridge — is pinned in
  [ADR-0040](../adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md)
  (superseding ADR-0030).
- Terminal search admission/counting and link detection/open policy remain
  qtermwidget-free application components. Only the confined adapter maps an
  admitted query to the pinned renderer search surface or extracts its bounded
  live-screen tail; only the application-owned opener may confirm and dispatch
  one absolute-program argv request. Neither extension changes PTY ownership.
- Font discovery and typography preferences follow the F0 pure boundary plus
  the F1 composition seam: only `src/services/font_discovery` links
  fontconfig, tests inject their configuration and directories so host font
  state is never consulted, and confirmed Settings1 `fonts.*` values reach
  first-party applications only through the coordinator bridge and the guarded
  pre-`QGuiApplication` session bootstrap composition. See
  [Font preferences](font-preferences.md),
  [ADR-0047](../adr/0047-pure-font-catalog-and-preference-boundary.md), and
  [ADR-0067](../adr/0067-confine-fontconfig-behind-font-discovery.md).
- Tests use public APIs first. Input-injection providers/devices, fake-device
  creation, output forcing, and similar backdoor authority must be absent from
  normal production sessions and clearly named as test interfaces. A versioned
  public method may remain present for contract tests only when normal sessions
  advertise it disabled and reject it before parsing or changing state.

Cross-process contracts carry explicit version, error, timeout, and restart
semantics. Persisted formats carry a schema version and migration tests. A new
dependency crossing these directions requires an ADR.

The concrete session/compositor boundary and current runtime qualifications are
documented in [Compositor and session integration](compositor-session.md). The
experimental D-Bus payload is documented separately in the
[Compositor1 reference](../reference/compositor-control-v1.md).

## Decomposition rules

Keep data model, mutation policy, serialization, IPC adaptation, and visual
presentation separate. A controller may orchestrate collaborators but may not
also become their storage, renderer, and platform adapter. Split components
when they gain a second reason to change; line-count limits in root
`AGENTS.md` are a final warning, not the definition of modularity.

Future-agent comment conventions and interface documentation requirements are
in [Coding practices](../development/coding-practices.md). Changes to these
boundaries follow the [documentation policy](../contributing/documentation-policy.md).

The application `Controls::applicationIcon` boundary and QML `Icon` are
GUI-thread asset lookup/presentation, with an embedded catalog fallback. They
are separate from the shell icon runtime. `MaterialSurface` uses local layers,
not backdrop capture. AppAppearance projects explicitly subscribed, confirmed
font/accessibility preferences through public QST inputs. See
[ADR-0109](../adr/0109-use-pearl-and-smoked-plum-app-materials.md).

The [Qt platform theme](qt-platform-theme.md) owns only standard palette, font,
icon and scheme projection for ordinary Qt consumers. Its pure projection uses
public ThemeSpec/QST; only the QPA plugin links matching `Qt6::GuiPrivate` and
observes the existing public Settings1 client. It writes no configuration,
implements no QStyle and adds no KDE appearance dependency. Platform services
remain delegated to Qt's generic base theme, except that a D-Bus menubar is
requested only while the AppMenu registrar has an owner
([ADR-0130](../adr/0130-window-attached-menus-without-a-global-menu.md)). See
[ADR-0115](../adr/0115-share-appearance-through-qt-platform-theme.md).
