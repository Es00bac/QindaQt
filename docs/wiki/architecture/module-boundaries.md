# Module boundaries

Module boundaries prevent the desktop from turning into one tightly coupled
shell process. Each module owns its implementation, public surface, focused
tests, and the wiki page describing its contract.

## Source ownership

| Area | Responsibility | Allowed inward dependencies |
| --- | --- | --- |
| `compositor` | Immutable upstream KWin pin, downstream patch inventory and verifier, and checked-in compositor IPC descriptors | Repository tooling and upstream source metadata; never shell implementation |
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
| `src/shell/status_notifier` | Pure StatusNotifier item values, bounded payload validation, exact-owner keyed registry with generation fencing, validated request intents, and deterministic accessible presentation | Qt Core value types only; never D-Bus connections or name ownership, action execution, QML, or platform objects |
| `src/shell_surface` | Backend-neutral panel and notification logical-surface planning, persistent panel live-set reconciliation, Qt output inventory, and private LayerShellQt adapters | Public `profiles` and `shell_layout` values, Qt Gui/Quick, and LayerShellQt only in adapters; never catalogs, applets, settings, or QML policy |
| `src/shell_orchestration` | Exact output matching, pure cross-module inventory assembly, tokenized reveal/hold interaction state, and runtime panel-plan coordination | Public profile/layout/visibility/surface values and Qt Core; never D-Bus, KWin, LayerShellQt, or QML |
| `src/themes` | Theme schema, validation, token resolution, and built-in theme data | Foundation utilities; never shell objects |
| `src/design_tokens` | Immutable QST-1 semantic derivation and a GUI-thread, read-only QML singleton adapter | Public `themes` values plus Qt Core/Gui; Qt QML only in the adapter; never settings, services, shell, applications, or Kirigami |
| `src/controls` | Compiled `QindaQt.Controls 1.0` token-styled primitives and Qinda-specific form/state presentation | Public `QindaQt.Tokens 1.0` plus Qt Quick, Quick Controls 2, and Layouts; never theme selection, settings, services, shell, applications, LayerShellQt, or Kirigami |
| `src/app_shell` | Application-owned lifecycle requests, stable action/menu projection, injected integration state, portal-request mediation, and reusable first-party QML window/focus/accessibility presentation | Public QST-1 and `QindaQt.Controls 1.0` plus Qt Core/Gui/QML/Quick; never app domain models, routes, persistence, services, platform portal/dialog code, process exit, shell, compositor, LayerShellQt, or KWin |
| `src/applets` | Native applet manifest schema, validation, normalization, and catalog discovery | Qt Core only; it does not load or execute applet code |
| `src/applet_host` | Host selection, capability policy, bounded protocol negotiation, and crash/backoff lifecycle state | `applets` public values and Qt Core; sandbox/process adapters remain separate |
| `src/applet_runtime` | Resolve profile instances through validated manifests, placement, host policy, the compiled built-in registry, and least-authority capability grants | Public `profiles`, `applets`, and `applet_host` values plus Qt Core; never QML, services, or third-party process launch |
| `src/settings` | Immutable-v1/active-v2 schemas, validation, migration, layered resolution, optimistic transactions, change sets, and atomic document codec | Qt Core only; service adapters consume this public model |
| `src/services/settings_protocol` | Generic Settings1 constants, typed outcomes, recursive JSON-native codecs, and resource bounds | Qt Core/DBus only; never settings schema/model, shell, or QML |
| `src/services/settings_service` | D-Bus activation/ownership, user-file lifecycle, copy-on-write persistence, revision authority, and changed-key publication | Public `settings` and settings protocol plus Qt Core/DBus; never shell/QML/lock/presenter authority |
| `src/services/settings_client` | Activation, exact-owner/epoch asynchronous snapshots and writes, timeout/uncertainty recovery, and DND-scoped state projection | Settings protocol plus Qt Core/DBus; never service persistence, shell presentation, or settings files |
| `src/services/portal` | Pure Settings1/QST appearance projection, exact-lineage availability source, standard Settings-backend D-Bus adapter, resident process, appearance-only activation package, and fail-closed frontend selection policy | Public settings client, themes, QST-1, and Qt Core/Gui/DBus; policy sources never import D-Bus, and the module never imports Settings persistence/service internals, applications, QML, shell, compositor, or non-Settings portal implementation authority |
| `src/services/display_protocol` | Display1 versioned values, hostile-input limits, semantic validation, canonical byte codec, and QtDBus value serialization | Qt Core and serialization-only Qt DBus; never connection/name/service/XML/client/platform state |
| `src/services/display_identity` | Pure privacy-preserving stable-ID resolution plus schema-v2 registry values and v1 migration | Qt Core only; never EDID acquisition, Settings persistence, runtime UUID authority, or logs of private material |
| `src/services/display_topology` | Pure candidate validation, normalization, logical geometry, mirror projection, canonical fingerprint, diff, and no-op | Public display protocol plus Qt Core; never KWin, Wayland, stored preferences, or mutation |
| `src/services/display_transaction` | Pure one-transaction state machine, journal value/codec, rollback/hotplug/recovery truth, and injected clock/port seams | Public display protocol/topology plus Qt Core; never real clocks/timers, files, D-Bus/Wayland, lock/logind, or QObject providers |
| `src/services/display_service` | Exact-owner D0 inventory decode/projection, Display1 owner/epoch/revision reset model, resident D-Bus object/process, deadline scheduling, and injected transaction-port composition | Public display protocol/identity/topology/transaction plus Qt Core/DBus; never KWin private ABI, Wayland, QML, Settings, filesystem journal, logind, or shell |
| `src/services/display_client` | Exact-owner asynchronous Display1 activation/snapshots, validated atomic publication, serialized operations, timeout/uncertainty fencing, and server-state-projected reversible transaction coordination | Public display protocol plus Qt Core/DBus; never service implementation, compositor writer, Settings, shell, or QML |
| `src/services/display_writer` | Fail-closed Display1 apply mapping, narrow compositor configuration validation, exactly-one-in-flight lineage/owner fencing, and a direct private KDE public-protocol adapter | Public display service/protocol/transaction plus Qt Core and private Qt/Wayland client integration; never KWin private ABI/store, libkscreen production authority, journal persistence, Settings, shell, or physical outputs |
| `src/services/display_journal` | Canonical Display1 journal file load/store/clear, same-directory atomic replacement, restrictive file/root validation, and the deterministic restart-recovery seam | Public display transaction/writer values plus Qt Core and narrow Linux file operations; never environment path discovery, directory selection/creation, compositor/session state, recovery policy, D-Bus, QML, or KWin |
| `src/services/display_runtime` | Packaged Display1 startup order, explicit user-state-root selection, D1 recovery injection, D4/D5 composition, Wayland-peer-authenticated lock safety, and exact-owner logind delay lifetime | Public display service/transaction/writer boundaries, accepted session-lock service, Qt Core/DBus, and process-local environment inputs; never KWin private ABI/store, Settings, QML, journal format/filesystem implementation, or nested-runtime assertions |
| `src/services/display_color_model` | Pure Display Color C0 bounded ICC descriptor/header validation, deterministic catalog values, per-output capability/assignment-intent evaluation, degraded truth, and fingerprinted atomic snapshots | Qt Core only; never Display1 sibling modules, ICC/profile file or host access, transport, persistence, compositor, QML, or display hardware |
| `src/services/power_protocol` | Power1 bounded values, canonical/fixed codecs, hostile validation, result lineage, and deterministic aggregate-battery policy | Qt Core and serialization-only Qt DBus; never a connection, service, upstream daemon, platform object, session, or UI |
| `src/services/power_service` | Resident Power1 ownership, generation-fenced upstream collaborator seams (battery/profile/session), atomic last-known-good snapshot orchestration, epoch/revision authority, exactly-once operation completion, and the activation package | Public power protocol plus Qt Core/DBus; core orchestration never names host daemons, Wayland, sysfs, or client implementation dependencies |
| `src/services/power_service/adapters` | Production UPower, power-profiles-daemon, logind session/action, and injected-root sysfs translation plus explicit production/unavailable composition | Public power-service/protocol boundaries and Qt Core/DBus; each adapter is confined to its own injected transport, with no Wayland, libupower, privilege helper, polkit UI, process/thread, client, or Display dependency |
| `src/services/power_client` | Exact-owner asynchronous Power1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty recovery, and stale-reply rejection | Public power protocol plus Qt Core/DBus; never service implementation, upstream daemons, or QML |
| `src/services/brightness_model` | Pure stable-ID fixture validation, mirror-collapsed display/keyboard composition, and integer raw-range conversion | Public power protocol plus Qt Core; never Display1 headers, connector identity, topology input, transport, persistence, clocks, QML, or mutation |
| `src/shell` | Qt Quick panel/notification presentation, production window factories, narrow built-in-applet facades, shell-owned launcher scanning/persistence/execution composition, interruption/privacy-policy composition, and global-action controllers | `core`, `profiles`, `themes`, `applet_runtime`, `shell_layout`, `shell_orchestration`, `shell_surface`, public launcher and service clients/models/policies, and focused KDE Framework clients behind private adapters; never LayerShellQt or service implementations directly |
| `src/shell/audio_applet` | Pure Audio applet projection values plus a separately linked shell-private `AudioClient` controller and compiled QML renderer | Pure target: public Audio1 protocol plus Qt Core. Runtime target: pure target, public audio client, and Qt QML/Quick; never audio-service internals, WirePlumber, PipeWire, GLib, direct platform transport, files, or persistence |
| `src/shell/bluetooth_applet` | Pure Bluetooth applet projection/request values plus a separately linked shell-private `BluetoothClient` controller and compiled QML renderer | Pure target: public Bluetooth protocol plus Qt Core. Runtime target: pure target, public Bluetooth client, and Qt QML/Quick; never Bluetooth service/model, BlueZ/BluezQt, Agent1/pairing/trust, host radios, addresses in presentation, files, or persistence |
| `src/shell/power_applet` | Pure Power applet projection/request values plus a separately linked shell-private `PowerClient` controller and compiled QML renderer | Pure target: public power protocol and brightness model plus Qt Core. Runtime target: pure target, public power client, and Qt QML/Quick; never power-service internals, host daemons, direct platform transport, files, or persistence |
| `src/shell/global_menu` | Separate focused targets: canonical bounded menu/action values and authenticated active-window provider ownership policy (protocol/policy), the fail-closed export lineage authority, the Qt Widgets menu adapter, and the shell-owned applet facade whose Qt Quick component owns this applet's presentation policy (orientation, overflow, focus, activation surfaces) | Protocol/policy: public protocol values plus Qt Core. Adapter target additionally Qt Gui/Widgets. Applet-presentation target additionally Qt Quick for its own component; never D-Bus transport, KWin objects, or action execution |
| `src/shell/global_menu/registrar` | Bounded standard AppMenu registrar residency, exact caller-unique-name ownership, owner/registration generation fencing, owner-loss retirement, and the bus-daemon credential seam | Public global-menu ownership values plus Qt Core/DBus; name ownership occurs only in its explicit composition root; never focus authority, menu decoding, QML, shell runtime, or KWin |
| `src/shell/global_menu/dbusmenu` | Exact-owner asynchronous standard dbusmenu calls/signals/properties, hostile recursive-layout conversion, remote-revision high water, and uncertain activation no-replay | Public global-menu protocol/exporter values plus Qt Core/DBus; never registrar name ownership, focus authority, Qt Gui/Widgets, QML, shell runtime, or action execution outside the standard Event request |
| `src/shell/global_menu/composition` | Injected focused-window-to-registrar join, proof reauthentication, selector/exporter publication, applet snapshot handoff, and guarded exactly-once activation intent routing | Public global-menu applet/dbusmenu/exporter/ownership/registrar boundaries plus Qt Core/DBus; never owns a bus name, discovers focus, imports shell runtime/QML/KWin, or changes toolkit exporters |
| `src/compositor` | Persistence-neutral transaction bridges plus the release-matched KWin window registry, generation-retaining output inventory, authenticated panel-owner shell actions and active-window identity projection, development-only virtual-output adapter, topology scene adapter, ordinary chrome pointer router, member/transient policy, lifecycle synchronization, and D-Bus plugin | Public `core`/Hybrid/shell-visibility limits, Qt Core/DBus, and explicit KWin 6.6.5 extension points; never registrar-derived identity |
| `src/session` | `qindaqt-wm` option validation, backend command construction, session environment, and KWin process handoff | Qt Core; it discovers plugins but does not import compositor internals |
| `src/session_supervisor` | Essential host/shell child startup, descriptor-only token handoff, parent-death-witnessed compositor-PID provisioning, coupled lifetime, and failure rollback | Public presentation-token protocol, Linux process identity/lifetime syscalls, and Qt Core; never compositor internals, QML, or service implementation libraries |
| `src/services` | Settings, session, metrics, notifications, audio, portals, and platform adapters | Shared interfaces and narrowly selected platform libraries |
| `src/services/audio_protocol` | Audio1 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation | Qt Core/DBus only; never transport state, QML, or platform objects |
| `src/services/audio_client` | Exact-owner asynchronous Audio1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty, and stale-reply rejection | Public Audio1 protocol plus Qt Core/DBus; never service implementation, WirePlumber, or QML |
| `src/services/audio_service` | Audio backend abstraction, operation coordinator, resident D-Bus object/process, and confined libwireplumber adapter | Public Audio1 protocol plus Qt Core/DBus and private WirePlumber/GLib worker; never shell/settings UI or PipeWire configuration |
| `src/services/bluetooth_protocol` | Bluetooth1 typed values, fixed D-Bus structures, aggregate/text limits, and fail-closed validation | Qt Core/DBus only; never transport state, QML, or platform objects |
| `src/services/bluetooth_model` | Adapter backend port, authoritative epoch/serial/lease coordination, operation validation, deterministic B0 platform adapter, and restart lineage | Public Bluetooth1 protocol plus Qt Core/DBus; never QML, shell, BlueZ mutation of pairing/trust, or D-Bus service ownership |
| `src/services/bluetooth_bluez_adapter` | Exact-owner BlueZ ObjectManager transport, bounded Adapter1/Device1 mapping, caller-scoped discovery sessions, and paired-device operation translation behind `AdapterBackend` | Public Bluetooth model port plus Qt Core/DBus; never service residency, QML/shell/settings, pairing/trust/record mutation, rfkill, ambient bus lookup inside the adapter, or platform handles in public headers |
| `src/services/bluetooth_client` | Exact-owner asynchronous Bluetooth1 discovery/snapshots, invalidation coalescing, serialized operations, timeout/uncertainty, and stale-reply rejection | Public Bluetooth1 protocol plus Qt Core/DBus; never service implementation, BluezQt, or QML |
| `src/services/bluetooth_service` | Resident D-Bus object/name ownership, caller-scoped discovery-lease watching, process entry point, and activation/hardening artifacts | Public Bluetooth1 model/protocol plus Qt Core/DBus; never pairing/trust authority, BlueZ supervision, shell/settings UI, or QML |
| `src/services/network_protocol` | Network1 bounded values, identity normalization, fail-closed validation/redaction, and canonical byte codecs | Qt Core only; never transport state, D-Bus, NetworkManager, platform objects, credentials, or QML |
| `src/services/network_model` | Pure lineage high-water, scan-lease reconciliation, intent admission, and atomic projection | Public Network1 protocol plus Qt Core; never clocks/timers, transport, platform state, persistence, credentials, or QML |
| `src/services/network_client` | Exact-owner asynchronous snapshots and operations, bounded timeout/retry, uncertain outcomes, and injected transport composition | Public Network1 protocol/model plus Qt Core; never a concrete transport, D-Bus, NetworkManager, radios, credentials, or QML |
| `src/services/notification_presentation_protocol` | Versioned presentation values, bounded D-Bus decoding, wire limits, restart lineage, 256-bit presenter-token values, and exact one-shot descriptor records | Qt Core/DBus and Linux descriptor syscalls; never notification policy, child lifecycle, host objects, or shell QML |
| `src/services/notification_presentation_client` | Unique-owner binding, asynchronous authentication/snapshots, serialized operations, initiating-revision result validation, bounded error normalization, uncertain-result recovery, timeout/backoff, invalidation coalescing, and stale-reply rejection | Public presentation protocol plus Qt Core/DBus; never host/service implementation or QML |
| `src/services/session_lock_state` | Fail-closed owner/PID-authenticated KWin/KScreenLocker state, asynchronous query/signal fencing, and bounded object-startup retry | Qt Core/DBus and a supervisor-provisioned PID value; never shell, notification, compositor-private, PAM, or QML objects |
| `src/services/notification_presentation_policy` | Thread-confined, session-volatile interruption state, total popup admission, and a separate fail-closed private-presentation decision | Public presentation-protocol values plus Qt Core; never transport, host/service implementation, Qt Quick/QML, persistence, or platform lock observation |
| `src/services/notification_presentation_model` | Privacy-gated baseline/no-replay Active, policy-filtered bounded popup, and in-memory Recent projections; monotonic popup expiry; center state; success-only popup removal; rejection renewal; and bounded busy/error lifetime | Public presentation client plus injected interruption/privacy policies and Qt Core; never Qt Quick/QML, LayerShellQt, host/service implementation, persistence, or platform lock observation |
| `src/services/notifications` | Bounded notification policy/model plus a separate freedesktop QtDBus adapter | Qt Core for the domain; QtDBus only in the protocol adapter; never QML or Plasma runtime |
| `src/services/notification_host` | Resident D-Bus ownership, one-shot notification-expiry scheduling, and optional authenticated presentation adapter | Public notification model/adapter and presentation protocol plus Qt Core/DBus; never popup UI, history persistence, sound, token provisioning, or session supervision |
| `src/services/clipboard_model` | Volatile bounded clipboard history: canonical media classification, privacy/opt-in gating, generation-fenced deterministic admission/eviction/dedup/pinning/clear, bounded metadata search, lineage-exhaustion fencing, and value/descriptor codecs | Qt Core only; never transport, Wayland, host clipboard, D-Bus, persistence, clocks, QObject providers, or QML |
| `src/services/clipboard_protocol` | Clipboard1 bounded snapshot/operation values, canonical descriptor reuse, hostile-wire validation, and fixed D-Bus structures | Public clipboard model plus Qt Core/DBus; never transport state, Wayland objects, payload persistence, service ownership, or QML |
| `src/services/clipboard_client` | Exact-owner asynchronous Clipboard1 discovery, atomic snapshots, invalidation coalescing, serialized intents, lineage fencing, and timeout uncertainty | Public clipboard protocol plus Qt Core/DBus and an injected transport; never host implementation, Wayland, payload storage, or QML |
| `src/services/clipboard_wayland_adapter` | Pinned `ext-data-control-v1` selection/primary observation, MIME preflight, bounded asynchronous reads, compositor-peer identity, and explicit selection publication | Public clipboard model values plus Qt Core and public Wayland client protocols; never history policy, D-Bus, Settings1, lock policy, persistence, or QML |
| `src/services/clipboard_service` | Resident volatile history ownership, Settings1 opt-in and authenticated-lock composition, Clipboard1 object/name ownership, caller-scoped request lineage, and activation artifacts | Public clipboard model/protocol/adapter plus Settings1 and lock-state clients; never persistence, shell/UI, compositor-private APIs, or payload logging |
| `src/services/font_preferences` | Pure deterministic font family discovery from injected facts, validated typography preferences, lossless codecs, pre-application bootstrap derivation, and atomic LKG publication | Qt Core and Qt Gui value types; never host filesystem scanning, fontconfig daemon mutation, D-Bus, KWin, or QML |
| `src/sdk` | Versioned client libraries, schemas, manifests, and generated IPC bindings | Foundation libraries only |
| `src/apps` | First-party applications behaving as normal desktop clients | Public SDK and application-focused libraries |
| `src/apps/text_editor` | Single-document text policy, bounded local UTF-8 persistence, standard Qt action/menu presentation, and QST-1 adaptation | Public themes/QST-1 plus Qt Core/Gui/Widgets; never shell internals, services, or another app's private code |
| `src/apps/file_manager` | Local directory listing/navigation history policy, a bounded local file-launch intent, and QST-1/QindaQt.Controls QML presentation | Public themes/QST-1/`QindaQt.Controls 1.0` plus Qt Core/Gui/Qml/Quick/QuickControls2; never shell internals, services, mount/portal/trash authority, or another app's private code |
| `src/apps/settings_center` | Bounded built-in route descriptors/registry, process-local navigation history, responsive QST/Controls host, and composition of route-owned public models | Public Settings1 clients, route domain models, themes/QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never settings service/persistence implementations, shell/compositor/platform mutation, arbitrary QML route loading, or one shared transport across independently tokened clients |
| `src/apps/settings/appearance` | Strict Appearance values, per-key draft/rebase and save-result truth, pure QST preview projection, and the route's modular QML presentation | Public Settings1 client, themes, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never settings persistence/service implementations, shell/compositor/display/platform mutation, another route's model, or transport access from QML |
| `src/apps/settings/customize` | Route-owned profile/catalog composition, direct editor-session canvas projection, Settings1 selection lifecycle, atomic user-profile persistence, and responsive accessible QML | Public Settings1 client, `shell_customization_editor`, `shell_customization`, `profiles`, applet manifests, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never private repository headers, settings service persistence, shell surfaces, LayerShellQt, compositor/platform mutation, or engine-policy duplication in QML |
| `src/apps/settings/audio` | Public-Audio1 consumer projection for the Settings Audio route: bounded device/stream rows, shared admission truth, closed set-default/volume/mute intents, and the route's modular QML presentation | Public Audio1 client/protocol, themes/QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never the resident audio service, WirePlumber/PipeWire, Qt D-Bus, stream movement, text entry, or transport access from QML |
| `src/apps/settings/bluetooth` | Exact-lineage Bluetooth inventory, route-scoped discovery lease lifetime, admitted adapter/paired-device controls, and responsive accessible QML | Public Bluetooth client/protocol, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; never BlueZ, private Bluetooth service/model headers, Qt D-Bus, pairing/trust/remove authority, another route's model, or transport access from QML |
| `src/apps/settings/power` | Bounded supply/profile/hold/brightness projection, exact-lineage admitted profile and debounced keyboard-brightness intents, convergence fencing, and responsive accessible QML | Public Power client/protocol, pure brightness math, QST-1, QindaQt.Controls, and Qt Core/Gui/QML; only the named composition root may construct the public Qt transport, and the module never imports the resident Power service/adapters, UPower, logind/session actions, sysfs, another route, or transport into QML/model code |
| `src/apps/terminal` | Terminal launch policy (argv/shell resolution, child environment, hostile-view clamping), bounded single-session PTY lifecycle with exit truth and guaranteed process-group teardown, the application-owned child-PTY bridge, QST-1 adaptation, and Qt action/menu presentation | Public themes/QST-1, Qt Core/Gui/Widgets, and `qtermwidget6` privately linked and confined to the rendering adapter per [ADR-0040](../adr/0040-own-terminal-child-pty-and-bridge-through-teletype.md); never shell internals, services, or another app's private code |
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
- StatusNotifier items are owned by their bus unique name, never a well-known
  name, and reach the tray only through bounded validation and an injected
  transport seam; the tray records request intents instead of executing them.
  See [Status notifier tray](../shell/status-tray.md) and
  [ADR-0032](../adr/0032-status-notifier-exact-owner-foundation.md).
- Bluetooth consumers depend on the typed Bluetooth1 client. BlueZ owns
  pairing, trust, keys, device records, and authorization; Bluetooth1 exposes
  inventory, adapter power, bounded caller-scoped discovery leases, and
  paired-device connect/disconnect only, and pairing prompts belong to a
  separate Agent1 outcome. See [Bluetooth service](bluetooth-service.md) and
  [ADR-0037](../adr/0037-keep-pairing-and-trust-authority-in-bluez.md).
- Network consumers depend on the typed Network1 client. The N0 direction
  remains protocol → model → client, with only an injected transport. N1's
  `network_qt_transport` implements that seam without reversing it;
  `network_service` owns fixed-wire D-Bus residency and an injected backend;
  only `network_manager_adapter` links libnm. Its public boundary exports
  secret-free copies and no NM/GObject handle. NetworkManager profile and
  credential authority remains external, with credentials supplied only by an
  external secret agent. See [Network service](network-service.md),
  [ADR-0045](../adr/0045-fence-network1-pure-boundary.md), and
  [ADR-0052](../adr/0052-confine-networkmanager-behind-network1.md).
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
  PowerClient. Manifest/policy `power.read` and `power.control` decisions gate
  observation and mutation separately; the renderer never sees a transport,
  service implementation, upstream daemon, or reusable general-power object.
- The Bluetooth applet receives only its shell-private controller over the
  public `BluetoothClient`. Manifest/policy `bluetooth.read` and
  `bluetooth.control` grants gate observation and mutation independently. Its
  controller owns one bounded caller-scoped discovery lease, releases it on
  popup close/teardown, and exposes no pairing, trust, key, Agent1, address,
  or BlueZ surface. See [Bluetooth applet](../shell/bluetooth-applet.md).
- The Audio applet receives only its shell-private controller over the
  public `AudioClient`. Manifest/policy `audio.read` and `audio.control`
  grants gate observation and mutation independently; the controller clears
  stale truth and pending operations on exact-owner replacement without
  replay, and exposes no PipeWire, WirePlumber, stream-move,
  default-device, or service-internal surface. See
  [Audio applet](../shell/audio-applet.md).
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
