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
| `src/profiles` | Layout-profile schema, validation, migration, and built-in profile data | `core` only when shared value types are unavoidable |
| `src/shell_layout` | Pure expansion, collision-free logical geometry planning, and per-output work areas | Public `profiles` values and Qt Core; never shell surfaces, compositor objects, or physical-pixel conversion |
| `src/shell_customization` | Exclusive editor leases, retained immutable snapshots, manifest-aware mutations, preview/history policy, and atomic candidate validation | Public `profiles`, `applets`, and `shell_layout` values plus Qt Core; never applet execution, shell surfaces, persistence, or settings UI |
| `src/shell_visibility` | Pure, batch-atomic window-aware panel visibility and reservation decisions | Public `profiles` values and Qt Core; never KWin objects, timers, QML, or layer-shell side effects |
| `src/shell_surface` | Backend-neutral panel-surface planning and reconciliation, Qt output inventory, and the private LayerShellQt platform adapter | Public `profiles` and `shell_layout` values, Qt Gui/Quick, and LayerShellQt only in the adapter; never catalogs, applets, settings, or QML policy |
| `src/themes` | Theme schema, validation, token resolution, and built-in theme data | Foundation utilities; never shell objects |
| `src/applets` | Native applet manifest schema, validation, normalization, and catalog discovery | Qt Core only; it does not load or execute applet code |
| `src/applet_host` | Host selection, capability policy, bounded protocol negotiation, and crash/backoff lifecycle state | `applets` public values and Qt Core; sandbox/process adapters remain separate |
| `src/settings` | Schema-v1 settings values, layered resolution, optimistic transactions, change sets, and atomic persistence | Qt Core only; future service adapters consume this public model |
| `src/shell` | Qt Quick presentation, production panel-window factory, and shell controllers consuming public values | `core`, `profiles`, `themes`, `shell_layout`, `shell_surface`, and public service clients; never LayerShellQt directly |
| `src/compositor` | Persistence-neutral transaction bridges plus the release-matched KWin registry, topology scene adapter, ordinary chrome pointer router, member/transient policy, lifecycle synchronization, and D-Bus plugin | Public `core`/Hybrid contracts, Qt Core/DBus, and explicit KWin 6.6.5 extension points |
| `src/session` | `qindaqt-wm` option validation, backend command construction, session environment, and KWin process handoff | Qt Core; it discovers plugins but does not import compositor internals |
| `src/services` | Settings, session, metrics, notifications, portals, and platform adapters | Shared interfaces and narrowly selected platform libraries |
| `src/services/notifications` | Bounded notification policy/model plus a separate freedesktop QtDBus adapter | Qt Core for the domain; QtDBus only in the protocol adapter; never QML or Plasma runtime |
| `src/services/notification_host` | Resident D-Bus ownership and one-shot notification-expiry scheduling | Public notification model/adapter plus Qt Core/DBus; never popup UI, history persistence, sound, or session supervision |
| `src/sdk` | Versioned client libraries, schemas, manifests, and generated IPC bindings | Foundation libraries only |
| `src/apps` | First-party applications behaving as normal desktop clients | Public SDK and application-focused libraries |
| `tools` and `tests` | Isolated development harnesses, fixtures, integration scenarios, and verification | Public APIs; test-only hooks in test builds |

Not every planned directory exists yet. Add one only when its responsibility is
implemented; do not use placeholder modules to bypass a boundary.

## Dependency direction

- `core`, profile, and theme models never import shell, compositor, service, or
  application presentation code.
- The compositor publishes state and accepts validated atomic commands. The
  shell does not link to KWin private objects.
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
  implementation-library link.
- Applets and applications use the SDK and public IPC. They do not include shell
  private headers or assume a specific panel implementation.
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
