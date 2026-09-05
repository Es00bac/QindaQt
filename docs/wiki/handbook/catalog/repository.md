# Repository, module, test, and tool navigation

> Repository snapshot: `9728612046940b55d69f85c3811eb38a08a0963b`. This catalog records checked-in contracts and evidence at that commit; it does not claim new runtime verification. Canonical linked pages remain authoritative as the project changes.

Use this map to find the implementation, public headers, focused tests, harnesses, and build ownership behind the user-visible documentation. All file counts come from Git at the snapshot; they count tracked files, not targets, test cases, or passing tests. [Module boundaries](../../architecture/module-boundaries.md) is normative for allowed dependencies.

## Responsibility map

The following boundary summaries are indexed from the canonical [module ownership contract](../../architecture/module-boundaries.md). Consult that page for dependency direction, threading, and cross-boundary restrictions. Directory counts below are a separate inventory.

| Owning boundary | Responsibility |
| --- | --- |
| `compositor` | Immutable upstream KWin pin, downstream patch inventory and verifier, and checked-in compositor IPC descriptors |
| `src/core` | Pure window-container domain model, mutations, invariants, and persistence-neutral values |
| `src/hybrid` | Session-wide window ownership, typed topology commands, and atomic candidate/scene publication |
| `src/hybrid_constraints` | Recursive member-size solving and lossless independent-window restore values |
| `src/hybrid_input` | Toolkit-neutral pointer/keyboard docking grab and intent state machine |
| `src/hybrid_chrome` | Event-free shared-container render-plan layout, typed hit testing, and Qt painting |
| `src/decorations` | Loadable KDecoration3 member-window presentation and standard window actions |
| `src/profiles` | Layout-profile schema, validation, migration, built-in data, and the sole strict atomic user-profile file writer |
| `src/shell_layout` | Pure expansion, collision-free logical geometry planning, and per-output work areas |
| `src/shell/launcher` | Separate targets: the pure bounded desktop-entry model (validation, deterministic category/search/ranking, pinned/recent identities, launch-intent presentation values) and the L1 runtime adapters (injected-root installed-application scanning with debounced watch and generation fencing, Settings1 pinned/recent persistence, seam-based bounded execution and D-Bus activation, and the shell-private applet controller plus compiled `QindaQt.Shell.Launcher` module) |
| `src/shell_customization` | Exclusive editor leases, retained immutable snapshots, manifest-aware mutations, preview/history policy, and atomic candidate validation |
| `src/shell_customization_editor` | Presentation-independent customization intents, gesture/revision orchestration, keyboard/accessibility identity, and a narrow profiles-store adapter |
| `src/shell_visibility_protocol` | Shared size, collection, identifier, and scale limits for the compositor-to-shell visibility wire contract |
| `src/shell_visibility` | Pure, batch-atomic window-aware panel visibility and reservation decisions |
| `src/shell_visibility_client` | Owner-bound asynchronous D-Bus snapshot transport, coalescing, timeout/backoff, and safe-fallback publication |
| `src/shell_window_actions_client` | Exact-owner authenticated compositor transport: one-in-flight no-replay window actions plus revisioned active-window identity refresh on the same owner binding |
| `src/shell_surface` | Backend-neutral panel and notification logical-surface planning, persistent panel live-set reconciliation, Qt output inventory, and private LayerShellQt adapters |
| `src/shell_orchestration` | Exact output matching, pure cross-module inventory assembly, tokenized reveal/hold interaction state, and runtime panel-plan coordination |
| `src/themes` | Theme schema, validation, token resolution, and built-in theme data |
| `src/design_tokens` | Immutable QST-1 semantic derivation and a GUI-thread, read-only QML singleton adapter |
| `src/controls` | Compiled `QindaQt.Controls 1.0` token-styled primitives and Qinda-specific form/state presentation |
| `src/app_shell` (core, excluding `menu_export`) | Application-owned lifecycle requests, stable action/menu projection, injected integration state, portal-request mediation, and reusable first-party QML window/focus/accessibility presentation |
| `src/app_shell/menu_export` | Opt-in composition of one AppShell action snapshot, one application window, and one injected session bus into the transport-owned standard dbusmenu server and platform-true AppMenu identity lifecycle, plus the one shared fail-closed `composeFirstPartyMenuExport` entry that File Manager, Terminal, and Text Editor call after their primary window exists |
| `src/applets` | Native applet manifest schema, validation, normalization, and catalog discovery |
| `src/applet_host` | Host selection, capability policy, bounded protocol negotiation, and crash/backoff lifecycle state |
| `src/applet_runtime` | Resolve profile instances through validated manifests, placement, host policy, the compiled built-in registry, and least-authority capability grants |
| `src/settings` | Immutable-v1/active-v2 schemas, validation, migration, layered resolution, optimistic transactions, change sets, and atomic document codec |
| `src/services/settings_protocol` | Generic Settings1 constants, typed outcomes, recursive JSON-native codecs, and resource bounds |
| `src/services/settings_service` | D-Bus activation/ownership, user-file lifecycle, copy-on-write persistence, revision authority, and changed-key publication |
| `src/services/settings_client` | Activation, exact-owner/epoch asynchronous snapshots and writes, timeout/uncertainty recovery, and DND-scoped state projection |
| `src/services/portal` | Pure Settings1/QST appearance projection, exact-lineage availability source, standard Settings-backend D-Bus adapter, resident process, appearance-only activation package, and fail-closed frontend selection policy |
| `src/services/display_protocol` | Display1 versioned values, hostile-input limits, semantic validation, canonical byte codec, and QtDBus value serialization |
| `src/services/display_identity` | Pure privacy-preserving stable-ID resolution plus schema-v2 registry values and v1 migration |
| `src/services/display_topology` | Pure candidate validation, normalization, logical geometry, mirror projection, canonical fingerprint, diff, and no-op |
| `src/services/display_transaction` | Pure one-transaction state machine, journal value/codec, rollback/hotplug/recovery truth, and injected clock/port seams |
| `src/services/display_service` | Exact-owner D0 inventory decode/projection, Display1 owner/epoch/revision reset model, resident D-Bus object/process, deadline scheduling, and injected transaction-port composition |
| `src/services/display_client` | Exact-owner asynchronous Display1 activation/snapshots, validated atomic publication, serialized operations, timeout/uncertainty fencing, and server-state-projected reversible transaction coordination |
| `src/services/display_writer` | Fail-closed Display1 apply mapping, narrow compositor configuration validation, exactly-one-in-flight lineage/owner fencing, and a direct private KDE public-protocol adapter |
| `src/services/display_journal` | Canonical Display1 journal file load/store/clear, same-directory atomic replacement, restrictive file/root validation, and the deterministic restart-recovery seam |
| `src/services/display_runtime` | Packaged Display1 startup order, explicit user-state-root selection, D1 recovery injection, D4/D5 composition, Wayland-peer-authenticated lock safety, and exact-owner logind delay lifetime |
| `src/services/display_color_model` | Pure Display Color C0 bounded ICC descriptor/header validation, deterministic catalog values, per-output capability/assignment-intent evaluation, degraded truth, and fingerprinted atomic snapshots |
| `src/services/display_color_discovery` | Synchronous ICC profile discovery and user import over injected roots only: bounded header/description-tag reads, deterministic C0 import metadata, origin classification, and atomic ADR-0051-style copies with SHA-256 lineage fingerprints |
| `src/services/display_color_assignment` | Strict per-output color assignment document codec and draft engine plus a Settings1-backed store with draft/apply/conflict/no-replay truth over the public settings client; fail-closed on unusable persisted documents |
| `src/services/power_protocol` | Power1 bounded values, canonical/fixed codecs, hostile validation, result lineage, and deterministic aggregate-battery policy |
| `src/services/power_service` | Resident Power1 ownership, generation-fenced upstream collaborator seams (battery/profile/session), atomic last-known-good snapshot orchestration, epoch/revision authority, exactly-once operation completion, and the activation package |
| `src/services/power_service/adapters` | Production UPower, power-profiles-daemon, logind session/action, and injected-root sysfs translation plus explicit production/unavailable composition |
| `src/services/power_client` | Exact-owner asynchronous Power1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty recovery, and stale-reply rejection |
| `src/services/session_actions` | Exact-owner, bounded asynchronous availability and one-shot dispatch for Session1 logout, ScreenSaver lock, and login1 suspend/reboot/power-off |
| `src/services/brightness_model` | Pure stable-ID fixture validation, mirror-collapsed display/keyboard composition, and integer raw-range conversion |
| `src/shell` | Qt Quick panel/notification presentation, production window factories, narrow built-in-applet facades, shell-owned icon/theme-root composition, launcher, Global Menu, Clipboard, Task List, and Status Notifier composition, interruption/privacy-policy composition, and global-action controllers |
| `src/shell/audio_applet` | Pure Audio applet projection values plus a separately linked shell-private `AudioClient` controller and compiled QML renderer |
| `src/shell/bluetooth_applet` | Pure Bluetooth applet projection/request values plus a separately linked shell-private `BluetoothClient` controller and compiled QML renderer |
| `src/shell/power_applet` | Pure Power applet projection/request values plus a separately linked shell-private `PowerClient` controller, injected `SessionActionsClient`, and compiled QML renderer |
| `src/shell/global_menu` | Separate focused targets: canonical bounded menu/action values and authenticated active-window provider ownership policy, fail-closed export lineage, Qt Widgets adaptation, AppMenu/dbusmenu transport, exact-owner identity-to-provider composition, and a compiled recursive applet/popup module |
| `src/shell/global_menu/registrar` | Bounded standard AppMenu registrar residency, exact caller-unique-name ownership, owner/registration generation fencing, owner-loss retirement, and the bus-daemon credential seam |
| `src/shell/global_menu/dbusmenu` | Complete standard dbusmenu v4 server, exact-owner asynchronous client calls/signals/properties, hostile recursive-layout conversion, remote-revision high water, and uncertain activation no-replay |
| `src/shell/global_menu/composition` | Injected focused-window-to-registrar join, proof reauthentication, selector/exporter publication, applet snapshot handoff, and guarded exactly-once activation intent routing |
| `src/shell/clipboard_applet` | Pure Clipboard applet projection values and the hostile-input snapshot admission gate, plus a separately linked runtime target holding the injected client seam, its in-process C0 model adapter, the shell-private controller, and the compiled QML renderer |
| `src/shell/task_list` | Pure injected-facts task-list values, atomic batch validation, deterministic grouping/ordering, scope filtering, presentation projection, and stale-id intent arbitration |
| `src/shell/task_list/producer` | Exact-owner asynchronous authenticated `CompositorShell1.TaskListSnapshot` reader, hostile bounded wire decoding, owner/epoch/revision/payload lineage fencing, atomic T0 publication, owner-change truth clearing, explicit unavailable publication, and the read-only operation-authority port |
| `src/shell/task_list/operations` | Serialized exactly-once Compositor1 `Submit`/`ReleaseContainer`/`DockWindows` adapter with injected owner/generation/container authority, transport-lifetime token lineage, canonical reply-lineage matching, and truthful Unavailable/Uncertain results |
| `src/shell/task_list/applet` | Pure bounded strip projection plus the shell-private task-list applet controller over injected operation and icon-name seams and the compiled `QindaQt.Shell.TaskList` module |
| `src/shell/status_notifier` | Pure StatusNotifier item values, bounded payload validation, exact-owner keyed registry with generation fencing, validated request intents, and deterministic accessible presentation |
| `src/shell/status_notifier/watcher` | `org.kde.StatusNotifierWatcher` service on an injected session-bus connection: registration keyed to caller unique names, owner-loss retirement, and truthful NameOwnedElsewhere degradation |
| `src/shell/status_notifier/item_client` | Asynchronous `org.kde.StatusNotifierItem` property reader with hostile-input decoding, generation-fenced late-reply dropping, and the monitor that feeds the registry through `StatusNotifierEventSink` |
| `src/shell/status_notifier/icon` | Deterministic icon-theme lookup over injected theme roots and bounded ARGB32 pixmap decoding into `QImage` with deterministic fallback |
| `src/shell/status_notifier/applet` | Tray applet slice: a pure projection target (phases, bounded rows, menu preview) plus a runtime target composing registry/monitor/icon renderer behind the injected `StatusNotifierSourceInterface` seam, with the controller and compiled `QindaQt.Shell.StatusNotifier` QML module |
| `src/shell/icons` | Confined XDG icon-theme lookup over injected roots, desktop-entry-to-icon resolution reusing the launcher's public pure parser, the engine-installed `image://qindaqt-icon/` provider with deterministic placeholder, the `IconRuntime::install` composition seam, and the compiled `QindaQt.Shell.Icons` module ([ADR-0071](../../adr/0072-shell-iconography-confined-xdg-icon-themes.md)) |
| `src/compositor` | Persistence-neutral transaction bridges plus the release-matched KWin window registry, generation-retaining output inventory, authenticated panel-owner shell actions, active-window identity, and atomic task-fact projection, development-only virtual-output adapter, topology scene adapter, ordinary chrome pointer router, member/transient policy, lifecycle synchronization, and D-Bus plugin |
| `src/session` | `qindaqt-wm` option validation, backend command construction, session environment, and KWin process handoff |
| `src/session_supervisor` | Essential host/shell child startup, descriptor-only token handoff, shell-PID-authenticated Session1 logout, optional restart-once secret-agent supervision, parent-death-witnessed compositor-PID provisioning, coupled lifetime, and failure rollback |
| `src/services` | Settings, session, metrics, notifications, audio, portals, and platform adapters |
| `src/services/audio_protocol` | Audio1 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation |
| `src/services/audio_client` | Exact-owner asynchronous Audio1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty, and stale-reply rejection |
| `src/services/audio_service` | Audio backend abstraction, operation coordinator, resident D-Bus object/process, and confined libwireplumber adapter |
| `src/services/bluetooth_protocol` | Frozen Bluetooth1 and current Bluetooth2 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation |
| `src/services/bluetooth_model` | Adapter backend port, authoritative epoch/serial/lease/prompt coordination, operation validation, deterministic B0 platform adapter, and restart lineage |
| `src/services/bluetooth_bluez_adapter` | Exact-owner BlueZ ObjectManager transport, bounded Adapter1/Device1 mapping, caller-scoped discovery, BlueZ-owned device mutations, and one bounded Agent1 prompt |
| `src/services/bluetooth_client` | Exact-owner asynchronous Bluetooth2 discovery/snapshots, invalidation coalescing, separate ordinary/prompt operation lanes, timeout/uncertainty, and stale-reply rejection |
| `src/services/bluetooth_service` | Resident frozen-Bluetooth1/current-Bluetooth2 object/name ownership, caller-scoped discovery-lease watching, pairing/reply wire methods, process entry point, and activation/hardening artifacts |
| `src/services/network_protocol` | Network1 bounded values, identity normalization, fail-closed validation/redaction, and canonical byte codecs |
| `src/services/network_model` | Pure lineage high-water, scan-lease reconciliation, intent admission, and atomic projection |
| `src/services/network_client` | Exact-owner asynchronous snapshots and operations, bounded timeout/retry, uncertain outcomes, and injected transport composition |
| `src/services/network_secret_agent` | Standard NetworkManager SecretAgent residency, exact-owner request admission, bounded interactive prompt, ephemeral reply dispatch, and presence-name publication |
| `src/services/notification_presentation_protocol` | Versioned presentation values, bounded D-Bus decoding, wire limits, restart lineage, 256-bit presenter-token values, and exact one-shot descriptor records |
| `src/services/notification_presentation_client` | Unique-owner binding, asynchronous authentication/snapshots, serialized operations, initiating-revision result validation, bounded error normalization, uncertain-result recovery, timeout/backoff, invalidation coalescing, and stale-reply rejection |
| `src/services/session_lock_state` | Fail-closed owner/PID-authenticated KWin/KScreenLocker state, asynchronous query/signal fencing, and bounded object-startup retry |
| `src/services/notification_presentation_policy` | Thread-confined, session-volatile interruption state, total popup admission, and a separate fail-closed private-presentation decision |
| `src/services/notification_presentation_model` | Privacy-gated baseline/no-replay Active, policy-filtered bounded popup, and in-memory Recent projections; monotonic popup expiry; center state; success-only popup removal; rejection renewal; and bounded busy/error lifetime |
| `src/services/notifications` | Bounded notification policy/model plus a separate freedesktop QtDBus adapter |
| `src/services/notification_host` | Resident D-Bus ownership, one-shot notification-expiry scheduling, and optional authenticated presentation adapter |
| `src/services/clipboard_model` | Volatile bounded clipboard history: canonical media classification, privacy/opt-in gating, generation-fenced deterministic admission/eviction/dedup/pinning/clear, bounded metadata search, lineage-exhaustion fencing, and value/descriptor codecs |
| `src/services/clipboard_protocol` | Clipboard1 bounded snapshot/operation values, canonical descriptor reuse, hostile-wire validation, and fixed D-Bus structures |
| `src/services/clipboard_client` | Exact-owner asynchronous Clipboard1 discovery, atomic snapshots, invalidation coalescing, serialized intents, lineage fencing, and timeout uncertainty |
| `src/services/clipboard_wayland_adapter` | Pinned `ext-data-control-v1` selection/primary observation, MIME preflight, bounded asynchronous reads, compositor-peer identity, and explicit selection publication |
| `src/services/clipboard_service` | Resident volatile history ownership, Settings1 opt-in and authenticated-lock composition, Clipboard1 object/name ownership, caller-scoped request lineage, and activation artifacts |
| `src/services/font_preferences` | Pure deterministic font family discovery from injected facts, validated typography preferences, exact-typed lossless codecs, pre-application bootstrap derivation (pure apply/gate helpers), atomic LKG publication, and the F1 Settings1 confirmed-snapshot bridge |
| `src/services/font_discovery` | Sole fontconfig-backed producer of F0 `FontFact` values from an internally built `FcConfig` over injected directories and an injected configuration file, with bounded counts/strings, deterministic ordering, and fail-closed unavailable truth; hosts the F1 production composition root (`FontSessionBootstrap`) that reads confirmed Settings1 `fonts.*` preferences pre-`QGuiApplication` and applies them when the live catalog resolves the family |
| `src/sdk` | Versioned client libraries, schemas, manifests, and generated IPC bindings |
| `src/apps` | First-party applications behaving as normal desktop clients |
| `src/apps/text_editor` | Bounded multi-document text policy, independent optimistic local UTF-8 persistence, bounded find/replace, paths-only restore state, AppShell action/menu presentation, and QST-1 adaptation |
| `src/apps/file_manager` | Local directory listing/navigation, bounded file launch, identity-checked asynchronous local mutation, home Trash/recovery policy, AppShell actions/global-menu opt-in, and QST-1/Controls presentation |
| `src/apps/settings_center` | Bounded built-in route descriptors/registry, process-local navigation history, responsive QST/Controls host, and composition of route-owned public models |
| `src/apps/settings/appearance` | Strict Appearance values, per-key draft/rebase and save-result truth, pure QST preview projection, and the route's modular QML presentation |
| `src/apps/settings/customize` | Route-owned profile/catalog composition, direct editor-session canvas projection, Settings1 selection lifecycle, atomic user-profile persistence, and responsive accessible QML |
| `src/apps/settings/audio` | Public-Audio1 consumer projection for the Settings Audio route: bounded device/stream rows, shared admission truth, closed set-default/volume/mute intents, and the route's modular QML presentation |
| `src/apps/settings/bluetooth` | Exact-lineage Bluetooth inventory, route-scoped discovery lease lifetime, admitted device/pairing controls, one inline prompt, and responsive accessible QML |
| `src/apps/settings/power` | Bounded supply/profile/hold/brightness projection, exact-lineage admitted profile and debounced keyboard-brightness intents, injected session-action presentation, convergence fencing, and responsive accessible QML |
| `src/apps/settings/clipboard` | Settings1 history-preference draft/conflict truth, content-free Clipboard1 state/count projection, and exact-lineage confirmed all-history clearing |
| `src/apps/settings/color` | Per-display ICC assignment projection over the Display1 inventory, C1 discovery/import catalog, and C1 Settings1 assignment draft/apply with conflict/no-replay truth, exact owner/epoch/revision admission, and responsive accessible QML |
| `src/apps/terminal` | Terminal launch policy (argv/shell resolution, child environment, hostile-view clamping), bounded single-session PTY lifecycle with exit truth and guaranteed process-group teardown, the application-owned child-PTY bridge, QST-1 adaptation, and Qt action/menu presentation |
| `tools` and `tests` | Isolated development harnesses, fixtures, integration scenarios, and verification |

## Production source modules

| Boundary | Tracked files | Surface | Build registration |
| --- | --- | --- | --- |
| `src/app_shell` | 15 | 6 headers in include directories; 1 QML files | `src/app_shell/CMakeLists.txt`, `src/app_shell/menu_export/CMakeLists.txt` |
| `src/applet_host` | 15 | 7 headers in include directories; 0 QML files | `src/applet_host/CMakeLists.txt` |
| `src/applet_runtime` | 5 | 2 headers in include directories; 0 QML files | `src/applet_runtime/CMakeLists.txt` |
| `src/applets` | 11 | 5 headers in include directories; 0 QML files | `src/applets/CMakeLists.txt` |
| `src/apps/file_manager` | 40 | 0 headers in include directories; 6 QML files | `src/apps/file_manager/CMakeLists.txt` |
| `src/apps/settings` | 115 | 16 headers in include directories; 48 QML files | `src/apps/settings/appearance/CMakeLists.txt`, `src/apps/settings/audio/CMakeLists.txt`, `src/apps/settings/bluetooth/CMakeLists.txt`, `src/apps/settings/clipboard/CMakeLists.txt`, `src/apps/settings/color/CMakeLists.txt`, `src/apps/settings/customize/CMakeLists.txt`, `src/apps/settings/display/CMakeLists.txt`, `src/apps/settings/network/CMakeLists.txt`, `src/apps/settings/power/CMakeLists.txt` |
| `src/apps/settings_center` | 15 | 0 headers in include directories; 6 QML files | `src/apps/settings_center/CMakeLists.txt` |
| `src/apps/terminal` | 51 | 0 headers in include directories; 0 QML files | `src/apps/terminal/CMakeLists.txt` |
| `src/apps/text_editor` | 43 | 0 headers in include directories; 0 QML files | `src/apps/text_editor/CMakeLists.txt` |
| `src/compositor` | 149 | 10 headers in include directories; 0 QML files | `src/compositor/CMakeLists.txt` |
| `src/controls` | 15 | 0 headers in include directories; 14 QML files | `src/controls/CMakeLists.txt` |
| `src/core` | 8 | 0 headers in include directories; 0 QML files | `src/core/CMakeLists.txt` |
| `src/decorations` | 6 | 0 headers in include directories; 0 QML files | `src/decorations/CMakeLists.txt` |
| `src/design_tokens` | 11 | 4 headers in include directories; 0 QML files | `src/design_tokens/CMakeLists.txt` |
| `src/hybrid` | 11 | 4 headers in include directories; 0 QML files | `src/hybrid/CMakeLists.txt` |
| `src/hybrid_chrome` | 11 | 5 headers in include directories; 0 QML files | `src/hybrid_chrome/CMakeLists.txt` |
| `src/hybrid_constraints` | 8 | 4 headers in include directories; 0 QML files | `src/hybrid_constraints/CMakeLists.txt` |
| `src/hybrid_input` | 5 | 3 headers in include directories; 0 QML files | `src/hybrid_input/CMakeLists.txt` |
| `src/profiles` | 19 | 6 headers in include directories; 0 QML files | `src/profiles/CMakeLists.txt` |
| `src/services/audio_client` | 7 | 3 headers in include directories; 0 QML files | `src/services/audio_client/CMakeLists.txt` |
| `src/services/audio_protocol` | 7 | 4 headers in include directories; 0 QML files | `src/services/audio_protocol/CMakeLists.txt` |
| `src/services/audio_service` | 19 | 4 headers in include directories; 0 QML files | `src/services/audio_service/CMakeLists.txt` |
| `src/services/bluetooth_bluez_adapter` | 15 | 2 headers in include directories; 0 QML files | `src/services/bluetooth_bluez_adapter/CMakeLists.txt` |
| `src/services/bluetooth_client` | 9 | 3 headers in include directories; 0 QML files | `src/services/bluetooth_client/CMakeLists.txt` |
| `src/services/bluetooth_model` | 9 | 3 headers in include directories; 0 QML files | `src/services/bluetooth_model/CMakeLists.txt` |
| `src/services/bluetooth_protocol` | 7 | 4 headers in include directories; 0 QML files | `src/services/bluetooth_protocol/CMakeLists.txt` |
| `src/services/bluetooth_service` | 12 | 1 headers in include directories; 0 QML files | `src/services/bluetooth_service/CMakeLists.txt` |
| `src/services/brightness_model` | 9 | 5 headers in include directories; 0 QML files | `src/services/brightness_model/CMakeLists.txt` |
| `src/services/clipboard_client` | 6 | 3 headers in include directories; 0 QML files | `src/services/clipboard_client/CMakeLists.txt` |
| `src/services/clipboard_model` | 16 | 5 headers in include directories; 0 QML files | `src/services/clipboard_model/CMakeLists.txt` |
| `src/services/clipboard_protocol` | 6 | 3 headers in include directories; 0 QML files | `src/services/clipboard_protocol/CMakeLists.txt` |
| `src/services/clipboard_service` | 13 | 2 headers in include directories; 0 QML files | `src/services/clipboard_service/CMakeLists.txt` |
| `src/services/clipboard_wayland_adapter` | 7 | 2 headers in include directories; 0 QML files | `src/services/clipboard_wayland_adapter/CMakeLists.txt` |
| `src/services/display_client` | 9 | 4 headers in include directories; 0 QML files | `src/services/display_client/CMakeLists.txt` |
| `src/services/display_color_assignment` | 5 | 2 headers in include directories; 0 QML files | `src/services/display_color_assignment/CMakeLists.txt` |
| `src/services/display_color_discovery` | 10 | 1 headers in include directories; 0 QML files | `src/services/display_color_discovery/CMakeLists.txt` |
| `src/services/display_color_model` | 7 | 4 headers in include directories; 0 QML files | `src/services/display_color_model/CMakeLists.txt` |
| `src/services/display_identity` | 7 | 4 headers in include directories; 0 QML files | `src/services/display_identity/CMakeLists.txt` |
| `src/services/display_journal` | 4 | 1 headers in include directories; 0 QML files | `src/services/display_journal/CMakeLists.txt` |
| `src/services/display_protocol` | 11 | 5 headers in include directories; 0 QML files | `src/services/display_protocol/CMakeLists.txt` |
| `src/services/display_runtime` | 8 | 3 headers in include directories; 0 QML files | `src/services/display_runtime/CMakeLists.txt` |
| `src/services/display_service` | 17 | 4 headers in include directories; 0 QML files | `src/services/display_service/CMakeLists.txt` |
| `src/services/display_topology` | 7 | 2 headers in include directories; 0 QML files | `src/services/display_topology/CMakeLists.txt` |
| `src/services/display_transaction` | 10 | 4 headers in include directories; 0 QML files | `src/services/display_transaction/CMakeLists.txt` |
| `src/services/display_writer` | 11 | 3 headers in include directories; 0 QML files | `src/services/display_writer/CMakeLists.txt` |
| `src/services/font_discovery` | 5 | 2 headers in include directories; 0 QML files | `src/services/font_discovery/CMakeLists.txt` |
| `src/services/font_preferences` | 20 | 10 headers in include directories; 0 QML files | `src/services/font_preferences/CMakeLists.txt` |
| `src/services/network_client` | 5 | 2 headers in include directories; 0 QML files | `src/services/network_client/CMakeLists.txt` |
| `src/services/network_manager_adapter` | 14 | 3 headers in include directories; 0 QML files | `src/services/network_manager_adapter/CMakeLists.txt` |
| `src/services/network_model` | 10 | 5 headers in include directories; 0 QML files | `src/services/network_model/CMakeLists.txt` |
| `src/services/network_protocol` | 13 | 7 headers in include directories; 0 QML files | `src/services/network_protocol/CMakeLists.txt` |
| `src/services/network_qt_transport` | 3 | 1 headers in include directories; 0 QML files | `src/services/network_qt_transport/CMakeLists.txt` |
| `src/services/network_secret_agent` | 19 | 6 headers in include directories; 1 QML files | `src/services/network_secret_agent/CMakeLists.txt` |
| `src/services/network_service` | 8 | 3 headers in include directories; 0 QML files | `src/services/network_service/CMakeLists.txt` |
| `src/services/notification_host` | 13 | 4 headers in include directories; 0 QML files | `src/services/notification_host/CMakeLists.txt` |
| `src/services/notification_presentation_client` | 9 | 3 headers in include directories; 0 QML files | `src/services/notification_presentation_client/CMakeLists.txt` |
| `src/services/notification_presentation_model` | 7 | 2 headers in include directories; 0 QML files | `src/services/notification_presentation_model/CMakeLists.txt` |
| `src/services/notification_presentation_policy` | 5 | 2 headers in include directories; 0 QML files | `src/services/notification_presentation_policy/CMakeLists.txt` |
| `src/services/notification_presentation_protocol` | 8 | 4 headers in include directories; 0 QML files | `src/services/notification_presentation_protocol/CMakeLists.txt` |
| `src/services/notifications` | 21 | 6 headers in include directories; 0 QML files | `src/services/notifications/CMakeLists.txt` |
| `src/services/portal` | 18 | 5 headers in include directories; 0 QML files | `src/services/portal/CMakeLists.txt` |
| `src/services/power_client` | 8 | 3 headers in include directories; 0 QML files | `src/services/power_client/CMakeLists.txt` |
| `src/services/power_protocol` | 13 | 6 headers in include directories; 0 QML files | `src/services/power_protocol/CMakeLists.txt` |
| `src/services/power_service` | 37 | 11 headers in include directories; 0 QML files | `src/services/power_service/CMakeLists.txt` |
| `src/services/session_actions` | 4 | 1 headers in include directories; 0 QML files | `src/services/session_actions/CMakeLists.txt` |
| `src/services/session_lock_state` | 8 | 4 headers in include directories; 0 QML files | `src/services/session_lock_state/CMakeLists.txt` |
| `src/services/settings_client` | 10 | 4 headers in include directories; 0 QML files | `src/services/settings_client/CMakeLists.txt` |
| `src/services/settings_protocol` | 12 | 5 headers in include directories; 0 QML files | `src/services/settings_protocol/CMakeLists.txt` |
| `src/services/settings_service` | 11 | 2 headers in include directories; 0 QML files | `src/services/settings_service/CMakeLists.txt` |
| `src/session` | 13 | 0 headers in include directories; 0 QML files | `src/session/CMakeLists.txt` |
| `src/session_supervisor` | 13 | 5 headers in include directories; 0 QML files | `src/session_supervisor/CMakeLists.txt` |
| `src/settings` | 17 | 6 headers in include directories; 0 QML files | `src/settings/CMakeLists.txt` |
| `src/shell` | 336 | 90 headers in include directories; 41 QML files | `src/shell/CMakeLists.txt`, `src/shell/audio_applet/CMakeLists.txt`, `src/shell/bluetooth_applet/CMakeLists.txt`, `src/shell/clipboard_applet/CMakeLists.txt`, `src/shell/global_menu/CMakeLists.txt`, `src/shell/global_menu/applet/CMakeLists.txt`, `src/shell/global_menu/composition/CMakeLists.txt`, `src/shell/global_menu/dbusmenu/CMakeLists.txt`, `src/shell/global_menu/exporter/CMakeLists.txt`, `src/shell/global_menu/ownership/CMakeLists.txt`, `src/shell/global_menu/protocol/CMakeLists.txt`, `src/shell/global_menu/qt_widgets_adapter/CMakeLists.txt`, `src/shell/global_menu/registrar/CMakeLists.txt`, `src/shell/icons/CMakeLists.txt`, `src/shell/launcher/CMakeLists.txt`, `src/shell/power_applet/CMakeLists.txt`, `src/shell/status_notifier/CMakeLists.txt`, `src/shell/status_notifier/applet/CMakeLists.txt`, `src/shell/status_notifier/icon/CMakeLists.txt`, `src/shell/status_notifier/item_client/CMakeLists.txt`, `src/shell/status_notifier/watcher/CMakeLists.txt`, `src/shell/task_list/CMakeLists.txt`, `src/shell/task_list/applet/CMakeLists.txt`, `src/shell/task_list/operations/CMakeLists.txt`, `src/shell/task_list/producer/CMakeLists.txt` |
| `src/shell_customization` | 26 | 4 headers in include directories; 0 QML files | `src/shell_customization/CMakeLists.txt` |
| `src/shell_customization_editor` | 20 | 9 headers in include directories; 0 QML files | `src/shell_customization_editor/CMakeLists.txt` |
| `src/shell_layout` | 4 | 2 headers in include directories; 0 QML files | `src/shell_layout/CMakeLists.txt` |
| `src/shell_orchestration` | 9 | 4 headers in include directories; 0 QML files | `src/shell_orchestration/CMakeLists.txt` |
| `src/shell_surface` | 18 | 10 headers in include directories; 0 QML files | `src/shell_surface/CMakeLists.txt` |
| `src/shell_visibility` | 10 | 4 headers in include directories; 0 QML files | `src/shell_visibility/CMakeLists.txt` |
| `src/shell_visibility_client` | 6 | 3 headers in include directories; 0 QML files | `src/shell_visibility_client/CMakeLists.txt` |
| `src/shell_visibility_protocol` | 2 | 1 headers in include directories; 0 QML files | `src/shell_visibility_protocol/CMakeLists.txt` |
| `src/shell_window_actions_client` | 6 | 3 headers in include directories; 0 QML files | `src/shell_window_actions_client/CMakeLists.txt` |
| `src/themes` | 7 | 3 headers in include directories; 0 QML files | `src/themes/CMakeLists.txt` |

## Test families

| Boundary | Tracked files | Surface | Build registration |
| --- | --- | --- | --- |
| `tests/app_shell` | 13 | 0 headers in include directories; 2 QML files | `tests/app_shell/CMakeLists.txt`, `tests/app_shell/installed_consumer/CMakeLists.txt` |
| `tests/applet_host` | 4 | 0 headers in include directories; 0 QML files | `tests/applet_host/CMakeLists.txt` |
| `tests/applet_runtime` | 2 | 0 headers in include directories; 0 QML files | `tests/applet_runtime/CMakeLists.txt` |
| `tests/applets` | 3 | 0 headers in include directories; 0 QML files | `tests/applets/CMakeLists.txt` |
| `tests/apps/file_manager` | 17 | 0 headers in include directories; 0 QML files | `tests/apps/file_manager/CMakeLists.txt` |
| `tests/apps/settings` | 83 | 0 headers in include directories; 1 QML files | `tests/apps/settings/appearance/CMakeLists.txt`, `tests/apps/settings/audio/CMakeLists.txt`, `tests/apps/settings/bluetooth/CMakeLists.txt`, `tests/apps/settings/clipboard/CMakeLists.txt`, `tests/apps/settings/color/CMakeLists.txt`, `tests/apps/settings/customize/CMakeLists.txt`, `tests/apps/settings/display/CMakeLists.txt`, `tests/apps/settings/network/CMakeLists.txt`, `tests/apps/settings/power/CMakeLists.txt` |
| `tests/apps/settings_center` | 11 | 0 headers in include directories; 1 QML files | `tests/apps/settings_center/CMakeLists.txt` |
| `tests/apps/terminal` | 22 | 0 headers in include directories; 0 QML files | `tests/apps/terminal/CMakeLists.txt` |
| `tests/apps/text_editor` | 20 | 0 headers in include directories; 0 QML files | `tests/apps/text_editor/CMakeLists.txt` |
| `tests/compositor` | 64 | 0 headers in include directories; 0 QML files | `tests/compositor/CMakeLists.txt` |
| `tests/controls` | 52 | 0 headers in include directories; 3 QML files | `tests/controls/CMakeLists.txt` |
| `tests/core` | 3 | 0 headers in include directories; 0 QML files | `tests/core/CMakeLists.txt` |
| `tests/decorations` | 2 | 0 headers in include directories; 0 QML files | `tests/decorations/CMakeLists.txt` |
| `tests/design_tokens` | 9 | 0 headers in include directories; 1 QML files | `tests/design_tokens/CMakeLists.txt`, `tests/design_tokens/installed_consumer/CMakeLists.txt` |
| `tests/hybrid` | 5 | 0 headers in include directories; 0 QML files | `tests/hybrid/CMakeLists.txt` |
| `tests/hybrid_chrome` | 5 | 0 headers in include directories; 0 QML files | `tests/hybrid_chrome/CMakeLists.txt` |
| `tests/hybrid_constraints` | 3 | 0 headers in include directories; 0 QML files | `tests/hybrid_constraints/CMakeLists.txt` |
| `tests/hybrid_input` | 5 | 0 headers in include directories; 0 QML files | `tests/hybrid_input/CMakeLists.txt` |
| `tests/profiles` | 5 | 0 headers in include directories; 0 QML files | `tests/profiles/CMakeLists.txt` |
| `tests/scenarios` | 15 | 0 headers in include directories; 0 QML files | Parent build registration |
| `tests/services/audio_client` | 5 | 0 headers in include directories; 0 QML files | `tests/services/audio_client/CMakeLists.txt` |
| `tests/services/audio_protocol` | 2 | 0 headers in include directories; 0 QML files | `tests/services/audio_protocol/CMakeLists.txt` |
| `tests/services/audio_service` | 5 | 0 headers in include directories; 0 QML files | `tests/services/audio_service/CMakeLists.txt` |
| `tests/services/bluetooth_bluez_adapter` | 14 | 0 headers in include directories; 0 QML files | `tests/services/bluetooth_bluez_adapter/CMakeLists.txt` |
| `tests/services/bluetooth_client` | 5 | 0 headers in include directories; 0 QML files | `tests/services/bluetooth_client/CMakeLists.txt` |
| `tests/services/bluetooth_model` | 4 | 0 headers in include directories; 0 QML files | `tests/services/bluetooth_model/CMakeLists.txt` |
| `tests/services/bluetooth_protocol` | 2 | 0 headers in include directories; 0 QML files | `tests/services/bluetooth_protocol/CMakeLists.txt` |
| `tests/services/bluetooth_service` | 8 | 0 headers in include directories; 0 QML files | `tests/services/bluetooth_service/CMakeLists.txt`, `tests/services/bluetooth_service/installed_consumer/CMakeLists.txt` |
| `tests/services/brightness_model` | 4 | 0 headers in include directories; 0 QML files | `tests/services/brightness_model/CMakeLists.txt` |
| `tests/services/clipboard_client` | 3 | 0 headers in include directories; 0 QML files | `tests/services/clipboard_client/CMakeLists.txt` |
| `tests/services/clipboard_model` | 6 | 0 headers in include directories; 0 QML files | `tests/services/clipboard_model/CMakeLists.txt` |
| `tests/services/clipboard_protocol` | 2 | 0 headers in include directories; 0 QML files | `tests/services/clipboard_protocol/CMakeLists.txt` |
| `tests/services/clipboard_service` | 11 | 0 headers in include directories; 0 QML files | `tests/services/clipboard_service/CMakeLists.txt`, `tests/services/clipboard_service/installed_consumer/CMakeLists.txt` |
| `tests/services/clipboard_wayland_adapter` | 5 | 0 headers in include directories; 0 QML files | `tests/services/clipboard_wayland_adapter/CMakeLists.txt` |
| `tests/services/display_client` | 10 | 0 headers in include directories; 0 QML files | `tests/services/display_client/CMakeLists.txt` |
| `tests/services/display_color_assignment` | 8 | 0 headers in include directories; 0 QML files | `tests/services/display_color_assignment/CMakeLists.txt`, `tests/services/display_color_assignment/installed_consumer/CMakeLists.txt` |
| `tests/services/display_color_discovery` | 9 | 0 headers in include directories; 0 QML files | `tests/services/display_color_discovery/CMakeLists.txt`, `tests/services/display_color_discovery/installed_consumer/CMakeLists.txt` |
| `tests/services/display_color_model` | 10 | 0 headers in include directories; 0 QML files | `tests/services/display_color_model/CMakeLists.txt`, `tests/services/display_color_model/installed_consumer/CMakeLists.txt` |
| `tests/services/display_identity` | 4 | 0 headers in include directories; 0 QML files | `tests/services/display_identity/CMakeLists.txt` |
| `tests/services/display_journal` | 6 | 0 headers in include directories; 0 QML files | `tests/services/display_journal/CMakeLists.txt`, `tests/services/display_journal/installed_consumer/CMakeLists.txt` |
| `tests/services/display_protocol` | 4 | 0 headers in include directories; 0 QML files | `tests/services/display_protocol/CMakeLists.txt` |
| `tests/services/display_runtime` | 12 | 0 headers in include directories; 0 QML files | `tests/services/display_runtime/CMakeLists.txt`, `tests/services/display_runtime/installed_consumer/CMakeLists.txt` |
| `tests/services/display_service` | 10 | 0 headers in include directories; 0 QML files | `tests/services/display_service/CMakeLists.txt` |
| `tests/services/display_topology` | 4 | 0 headers in include directories; 0 QML files | `tests/services/display_topology/CMakeLists.txt` |
| `tests/services/display_transaction` | 7 | 0 headers in include directories; 0 QML files | `tests/services/display_transaction/CMakeLists.txt` |
| `tests/services/display_writer` | 10 | 0 headers in include directories; 0 QML files | `tests/services/display_writer/CMakeLists.txt`, `tests/services/display_writer/installed_consumer/CMakeLists.txt` |
| `tests/services/font_discovery` | 16 | 0 headers in include directories; 0 QML files | `tests/services/font_discovery/CMakeLists.txt`, `tests/services/font_discovery/installed_consumer/CMakeLists.txt` |
| `tests/services/font_preferences` | 12 | 0 headers in include directories; 0 QML files | `tests/services/font_preferences/CMakeLists.txt`, `tests/services/font_preferences/installed_consumer/CMakeLists.txt` |
| `tests/services/network_client` | 9 | 0 headers in include directories; 0 QML files | `tests/services/network_client/CMakeLists.txt`, `tests/services/network_client/installed_consumer/CMakeLists.txt` |
| `tests/services/network_manager_adapter` | 10 | 0 headers in include directories; 0 QML files | `tests/services/network_manager_adapter/CMakeLists.txt`, `tests/services/network_manager_adapter/installed_consumer/CMakeLists.txt` |
| `tests/services/network_model` | 5 | 0 headers in include directories; 0 QML files | `tests/services/network_model/CMakeLists.txt` |
| `tests/services/network_protocol` | 6 | 0 headers in include directories; 0 QML files | `tests/services/network_protocol/CMakeLists.txt` |
| `tests/services/network_qt_transport` | 3 | 0 headers in include directories; 0 QML files | `tests/services/network_qt_transport/CMakeLists.txt` |
| `tests/services/network_secret_agent` | 7 | 0 headers in include directories; 0 QML files | `tests/services/network_secret_agent/CMakeLists.txt` |
| `tests/services/network_service` | 4 | 0 headers in include directories; 0 QML files | `tests/services/network_service/CMakeLists.txt` |
| `tests/services/notification_host` | 6 | 0 headers in include directories; 0 QML files | `tests/services/notification_host/CMakeLists.txt` |
| `tests/services/notification_presentation_client` | 5 | 0 headers in include directories; 0 QML files | `tests/services/notification_presentation_client/CMakeLists.txt` |
| `tests/services/notification_presentation_model` | 4 | 0 headers in include directories; 0 QML files | `tests/services/notification_presentation_model/CMakeLists.txt` |
| `tests/services/notification_presentation_policy` | 3 | 0 headers in include directories; 0 QML files | `tests/services/notification_presentation_policy/CMakeLists.txt` |
| `tests/services/notification_presentation_protocol` | 3 | 0 headers in include directories; 0 QML files | `tests/services/notification_presentation_protocol/CMakeLists.txt` |
| `tests/services/notifications` | 5 | 0 headers in include directories; 0 QML files | `tests/services/notifications/CMakeLists.txt` |
| `tests/services/portal` | 12 | 0 headers in include directories; 0 QML files | `tests/services/portal/CMakeLists.txt` |
| `tests/services/power_client` | 8 | 0 headers in include directories; 0 QML files | `tests/services/power_client/CMakeLists.txt`, `tests/services/power_client/installed_consumer/CMakeLists.txt` |
| `tests/services/power_protocol` | 5 | 0 headers in include directories; 0 QML files | `tests/services/power_protocol/CMakeLists.txt` |
| `tests/services/power_service` | 20 | 0 headers in include directories; 0 QML files | `tests/services/power_service/CMakeLists.txt` |
| `tests/services/session_actions` | 4 | 0 headers in include directories; 0 QML files | `tests/services/session_actions/CMakeLists.txt` |
| `tests/services/session_lock_state` | 5 | 0 headers in include directories; 0 QML files | `tests/services/session_lock_state/CMakeLists.txt` |
| `tests/services/settings_client` | 6 | 0 headers in include directories; 0 QML files | `tests/services/settings_client/CMakeLists.txt` |
| `tests/services/settings_protocol` | 3 | 0 headers in include directories; 0 QML files | `tests/services/settings_protocol/CMakeLists.txt` |
| `tests/services/settings_service` | 4 | 0 headers in include directories; 0 QML files | `tests/services/settings_service/CMakeLists.txt` |
| `tests/session` | 161 | 0 headers in include directories; 0 QML files | `tests/session/CMakeLists.txt` |
| `tests/session_supervisor` | 5 | 0 headers in include directories; 0 QML files | `tests/session_supervisor/CMakeLists.txt` |
| `tests/settings` | 5 | 0 headers in include directories; 0 QML files | `tests/settings/CMakeLists.txt` |
| `tests/shell` | 212 | 0 headers in include directories; 34 QML files | `tests/shell/CMakeLists.txt`, `tests/shell/audio_applet/CMakeLists.txt`, `tests/shell/bluetooth_applet/CMakeLists.txt`, `tests/shell/clipboard_applet/CMakeLists.txt`, `tests/shell/clipboard_applet/installed_consumer/CMakeLists.txt`, `tests/shell/global_menu/CMakeLists.txt`, `tests/shell/global_menu/applet/CMakeLists.txt`, `tests/shell/global_menu/boundary/CMakeLists.txt`, `tests/shell/global_menu/composition/CMakeLists.txt`, `tests/shell/global_menu/dbusmenu/CMakeLists.txt`, `tests/shell/global_menu/exporter/CMakeLists.txt`, `tests/shell/global_menu/ownership/CMakeLists.txt`, `tests/shell/global_menu/protocol/CMakeLists.txt`, `tests/shell/global_menu/qml/CMakeLists.txt`, `tests/shell/global_menu/qt_widgets_adapter/CMakeLists.txt`, `tests/shell/global_menu/registrar/CMakeLists.txt`, `tests/shell/global_menu/runtime_composition/CMakeLists.txt`, `tests/shell/global_menu/transport_composition/CMakeLists.txt`, `tests/shell/icons/CMakeLists.txt`, `tests/shell/launcher/CMakeLists.txt`, `tests/shell/power_applet/CMakeLists.txt`, `tests/shell/status_notifier/CMakeLists.txt`, `tests/shell/status_notifier/applet/CMakeLists.txt`, `tests/shell/status_notifier/applet/installed_consumer/CMakeLists.txt`, `tests/shell/task_list/CMakeLists.txt`, `tests/shell/task_list/installed_task_list_consumer/CMakeLists.txt` |
| `tests/shell_customization` | 7 | 0 headers in include directories; 0 QML files | `tests/shell_customization/CMakeLists.txt` |
| `tests/shell_customization_editor` | 8 | 0 headers in include directories; 0 QML files | `tests/shell_customization_editor/CMakeLists.txt` |
| `tests/shell_layout` | 3 | 0 headers in include directories; 0 QML files | `tests/shell_layout/CMakeLists.txt` |
| `tests/shell_orchestration` | 6 | 0 headers in include directories; 0 QML files | `tests/shell_orchestration/CMakeLists.txt` |
| `tests/shell_surface` | 6 | 0 headers in include directories; 0 QML files | `tests/shell_surface/CMakeLists.txt` |
| `tests/shell_visibility` | 8 | 0 headers in include directories; 0 QML files | `tests/shell_visibility/CMakeLists.txt` |
| `tests/shell_visibility_client` | 3 | 0 headers in include directories; 0 QML files | `tests/shell_visibility_client/CMakeLists.txt` |
| `tests/shell_visibility_producers` | 4 | 0 headers in include directories; 0 QML files | `tests/shell_visibility_producers/CMakeLists.txt` |
| `tests/shell_window_actions_client` | 3 | 0 headers in include directories; 0 QML files | `tests/shell_window_actions_client/CMakeLists.txt` |
| `tests/themes` | 2 | 0 headers in include directories; 0 QML files | `tests/themes/CMakeLists.txt` |
| `tests/tools` | 2 | 0 headers in include directories; 0 QML files | `tests/tools/CMakeLists.txt` |

## Tooling inventory

Every checked-in file below `tools/` is listed, including command wrappers and their Python implementation modules. Executable entry points are scripts such as `tools/qindaqt-dev-session`, `tools/validate-docs`, `tools/check-source-shape`, `tools/team-board`, and `tools/build-wiki-epub`; implementation modules are navigation aids, not necessarily standalone commands.

- `tools/build-wiki-epub` — Build the complete offline wiki EPUB.
- `tools/check-source-shape` — Stable entry point for QindaQt source-shape policy checks.
- `tools/docs_validation.py` — Validate local Markdown links and MkDocs navigation using only Python stdlib.
- `tools/qindaqt-dev-session` — Stable entry point for the QindaQt nested-session development harness.
- `tools/qindaqt_dev/__init__.py` — QindaQt's dependency-free development-session harness.
- `tools/qindaqt_dev/backends.py` — Translate a validated scenario into a backend-specific launch plan.
- `tools/qindaqt_dev/cli.py` — Command-line interface for isolated QindaQt development sessions.
- `tools/qindaqt_dev/isolation.py` — Create and supervise a disposable desktop-session environment.
- `tools/qindaqt_dev/scenarios.py` — Load and validate declarative virtual-display scenarios.
- `tools/source-shape.json` — Source shape thresholds and approved exception configuration.
- `tools/source_shape/__init__.py` — Repository source-shape policy package.
- `tools/source_shape/checker.py` — Apply size and cohesion proxies to hand-written source files.
- `tools/source_shape/cli.py` — CLI adapter for the source-shape checker.
- `tools/source_shape/config.py` — Parse the allowlisted, repository-owned source-shape policy.
- `tools/source_shape/language.py` — Find oversized function-shaped blocks without compiler dependencies.
- `tools/team-board/board.mjs` — Supporting board tooling; inspect its entry point and callers before invoking.
- `tools/team-board/board.test.mjs` — Supporting board.test tooling; inspect its entry point and callers before invoking.
- `tools/team-board/markdown.dom.test.mjs` — Supporting markdown.dom.test tooling; inspect its entry point and callers before invoking.
- `tools/team-board/markdown.mjs` — Supporting markdown tooling; inspect its entry point and callers before invoking.
- `tools/team-board/public/index.html` — Supporting index tooling; inspect its entry point and callers before invoking.
- `tools/team-board/public/markdown.js` — Supporting markdown tooling; inspect its entry point and callers before invoking.
- `tools/team-board/server.mjs` — Supporting server tooling; inspect its entry point and callers before invoking.
- `tools/validate-docs` — Stable entry point for dependency-free wiki validation.

## Remaining repository areas

- `.github/` — Repository automation and continuous integration definitions. 1 tracked files.
- `cmake/` — Build dependency discovery and package configuration. 1 tracked files.
- `compositor/` — Pinned upstream compositor metadata, patch inventory, and protocol descriptors. 7 tracked files.
- `data/` — Packaged declarative assets; see the asset and settings catalogs. 31 tracked files.
- `docs/` — Canonical wiki, task list, handoff, and delivery documentation. 147 tracked files.
- `ops/` — Delivery ledger, queues, durable worker messages, and operational coordination; activity is not product qualification. 585 tracked files.

## Root entry points

- `.gitignore`
- `AGENTS.md`
- `CMakeLists.txt`
- `CMakePresets.json`
- `LICENSE.md`
- `README.md`
- `mkdocs.yml`

## Finding individual behavior

Run `rg --files src tests` for exact current file names; use `rg "BehaviorName" src tests` to follow a public type or scenario. Module-local `CMakeLists.txt` owns target registration, while `tests/CMakeLists.txt` and scenario manifests establish test composition. The [testing harness](../../development/testing-harness.md) defines which unit, package, nested-session, resolution, and physical gates are necessary. File presence alone establishes none of those results.

