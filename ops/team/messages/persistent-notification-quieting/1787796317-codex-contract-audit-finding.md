# Finding: Settings1 contract audit and recommended decisions

- Worker: Codex contract auditor
- Timestamp: 2026-08-27T02:05:17Z
- Audited tree: detached `496e5135ee4f40359f8b871eec130f0b8b02a241`
- Scope: read-only source/document inspection; no source edit, build, test, stage, or commit was performed

## Blocking contradictions first

1. **Schema version is not decided.** The owning page promises a future schema
   revision for DND (`docs/wiki/architecture/settings-service.md:68-73`), while
   the current implementation only accepts the compile-time exact v1
   (`src/settings/CMakeLists.txt:31-35`,
   `src/settings/src/settings_schema.cpp:67-75`) and persistent documents must
   exactly equal the active version (`src/settings/src/settings_document.cpp:20-39`).
   There is no migrator. Adding a key to `schema-v1.json` would preserve sparse
   old documents but silently change the meaning of a file named/versioned v1.
   **Recommendation:** keep v1 immutable, add schema v2, and implement an
   explicit v1-to-v2 migrator. Copy every normalized v1 override; omit the new
   DND override so it resolves to the v2 system default `false`. Reject corrupt
   v1, unsupported versions, or invalid migrated candidates without writing.

2. **The present UI authority contradicts the new route.** ADR-0010 says the
   notification center owns the writable control
   (`docs/wiki/adr/0010-inject-shell-notification-interruption-policy.md:44-51`),
   and QML writes the presentation controller directly
   (`src/shell/qml/NotificationCenter.qml:52-75`; controller writer at
   `src/services/notification_presentation_model/include/qindaqt/services/notification_presentation_model/notification_presentation_controller.h:41-43,74-76`).
   That bypass would let policy diverge from persistence when a Settings1 commit
   fails. **Recommendation:** a new ADR supersedes that ownership clause. Make
   presentation DND read-only to QML; only a shell Settings1 bridge applies an
   accepted setting to the injected interruption policy. Remove the center
   toggle or turn it into a read-only status/link. The writable switch belongs
   to a standalone settings-center route consuming only the public Settings1
   client.

3. **Current supervision cannot provide independent settings restart.** The
   supervisor owns exactly notification host plus shell and any child exit
   tears down its sibling and session
   (`src/session_supervisor/include/qindaqt/session_supervisor/session_process_supervisor.h:27-29`,
   `src/session_supervisor/src/session_process_supervisor.cpp:129-149`).
   **Recommendation:** do not add Settings1 as a third coupled essential child.
   Install a D-Bus activation descriptor and have clients asynchronously call
   `StartServiceByName`, then resolve/bind the unique owner. If activation is
   rejected, a new selective bounded restart policy is required; simply adding
   a third child is incompatible with the acceptance criterion.

4. **Atomic file save is not atomic service commit.** `LayeredSettings::commit`
   mutates live memory/revision (`src/settings/src/layered_settings.cpp:72-120,166-219`),
   while `SettingsFileStore::save` only protects the file replacement with
   `QSaveFile` (`src/settings/src/settings_document.cpp:120-151`).
   **Recommendation:** add a repository/coordinator that clones the current
   model, commits into the candidate, serializes and commits the candidate user
   document, then swaps/publishes the candidate. On validation or save failure,
   the live model, revision, file, and signal stream remain unchanged. If the
   file commit succeeds but the reply is lost, the client treats the operation
   as uncertain and rereads authority; it must never report a confirmed failure.

5. **“Owner-authenticated” and failure fallback need explicit meanings.** The
   reusable client pattern binds signals and calls to an exact unique owner
   (`src/services/notification_presentation_client/include/qindaqt/services/notification_presentation_client/presentation_transport.h:11-13`,
   `src/services/notification_presentation_client/src/qt_notification_presentation_transport.cpp:222-258`),
   but it does not prove executable/PID identity. **Recommendation:** Settings1
   v1 accepts the session bus name owner as the service authority and permits
   ordinary same-session clients to mutate **user overrides only**. State this
   same-UID threat scope; do not imply PID/token authentication. If trusted-only
   writers are wanted, token/PID/bus-policy provisioning is a separate missing
   contract. For shell behavior, set DND `true` before the first authoritative
   baseline (fail quiet), then retain the last accepted value across timeout,
   owner loss, or transport loss while disabling writes. Rebind and replace it
   only after a validated new-owner snapshot. Lock privacy remains separately
   fail closed and always outranks DND
   (`docs/wiki/adr/0011-gate-notifications-on-authenticated-lock-state.md:78-92`).

## Reusable foundation and patterns

- `SettingsSchema` already gives exact typed/default normalization and full-layer
  validation (`src/settings/include/qindaqt/settings/settings_schema.h:43-63`).
- `LayeredSettings` already gives four-layer resolution, optimistic base
  revisions, semantic no-op detection, and raw/effective change sets
  (`src/settings/include/qindaqt/settings/layered_settings.h:18-31`;
  `src/settings/src/layered_settings.cpp:166-219`).
- `SettingsDocumentCodec` and `SettingsFileStore` already reject wrong layers,
  validate before opening the destination, and atomically replace one file
  (`src/settings/include/qindaqt/settings/settings_document.h:25-46`;
  `src/settings/src/settings_document.cpp:45-76,120-151`). The service, not these
  low-level APIs, must create the parent directory, enforce path/permissions,
  assert the expected document layer, and coordinate model/file atomicity.
- The notification presentation client supplies the owner-lineage, request
  token, timeout, bounded retry, stale-reply, invalidation-coalescing, and
  uncertain-operation recovery pattern
  (`src/services/notification_presentation_client/include/qindaqt/services/notification_presentation_client/notification_presentation_client.h:17-33,65-122`;
  `src/services/notification_presentation_client/src/notification_presentation_client.cpp:166-220,223-337`).
- The lock transport supplies the missing explicit bus-daemon-loss pattern;
  owner signals are not enough
  (`src/services/session_lock_state/include/qindaqt/services/session_lock_state/session_lock_transport.h:44-49`;
  `src/services/session_lock_state/src/qt_session_lock_transport.cpp:65-87,327-340`).
- The notification host supplies explicit object/name acquisition and rollback
  (`src/services/notifications/src/freedesktop_notification_server.cpp:118-162`)
  plus typed startup conflict reporting
  (`src/services/notification_host/src/resident_notification_host.cpp:187-254`).
- The existing DND policy remains the correct persistence-neutral target; it is
  thread-confined, defaults off internally, and only filters popups
  (`src/services/notification_presentation_policy/include/qindaqt/services/notification_presentation_policy/notification_interruption_policy.h:12-37`).
  The shell bridge may choose the temporary fail-quiet value before Settings1 is
  authoritative without teaching this module persistence.

## Recommended v1 Settings1 wire/service contract

- Well-known name/interface `org.qindaqt.Settings1`, object
  `/org/qindaqt/Settings1`; shared protocol constants and bounded codecs live in
  a Qt Core/DBus-only protocol module.
- Every snapshot contains exact wire schema, active settings-schema version,
  fresh service epoch UUID, nonwrapping process-local revision, requested
  effective values, and source layers. Clients compare revision only within
  `(unique owner, epoch)`.
- `SettingsChanged(epoch, revision, changedKeys)` is a bounded invalidation hint,
  not a value broadcast. A client subscribes to the exact unique owner before
  its first read and rereads a complete scoped snapshot. Calls target that
  unique owner, never the replaceable well-known name.
- Use one bounded `CommitUserTransaction(epoch, baseRevision, operations)` call.
  Decode exact keys/types/count/aggregate bytes before constructing the existing
  `SettingsTransaction`; reject duplicate operations rather than letting map
  insertion silently choose one. V1 exposes only user overrides. A no-op does
  not save, increment revision, or signal. Conflict is a typed result carrying
  current lineage/revision; validation, read-only layer, persistence failure,
  and revision exhaustion are distinct typed errors.
- The current schema has no text/list/object size or recursion bounds and an
  object accepts any `QVariantMap`
  (`src/settings/src/settings_value_normalizer.cpp:80-113`). A generic D-Bus
  adapter therefore needs protocol aggregate/count/depth/UTF-8 limits and exact
  JSON-native variant decoding even though DND itself is Boolean.
- Add revision-exhaustion handling before exposing the model cross-process;
  the current unconditional increment can wrap
  (`src/settings/src/layered_settings.cpp:216-218`).
- Startup reads schema, profile defaults, and user data completely into a
  candidate. It must win the well-known name before any migration write, so a
  losing concurrent host cannot rewrite shared state. After ownership, commit a
  pending migration atomically; failure rolls back object/name and exits. User
  storage should be one service-owned file below the XDG config home, with a
  service-owned parent directory and no client-visible path.
- Install a D-Bus activation descriptor. Client startup/owner loss requests
  activation asynchronously with bounded backoff, installs the owner watcher
  and local `Disconnected` observation first, then binds/fetches. Bus-daemon loss
  is terminal for that transport instance and visibly degraded; service process
  replacement is recoverable.

## Likely file/module boundaries

- `data/settings/schema-v2.json` and version-matched profile defaults; retain v1
  for migration fixtures.
- `src/settings`: add a v1-to-v2 migrator and copy-on-write persistent repository;
  keep D-Bus/QtQml out.
- `src/services/settings_protocol`: wire constants, bounded snapshot/operation/
  result codecs and typed errors; Qt Core/DBus only.
- `src/services/settings_service`: repository composition, private D-Bus object,
  owner/registration lifecycle, activation-ready executable; links public
  Settings + protocol, never QML/shell.
- `src/services/settings_client`: transport seam, exact-owner Qt transport,
  activation, timeout/backoff/coalescing, commit uncertainty/conflict recovery;
  links protocol, never service/settings persistence/QML.
- `src/apps/settings_center`: installed ordinary GUI client with a Notifications
  route and DND switch. It consumes the settings client only; it must not import
  notification presentation or shell-private types.
- `src/shell/runtime/notificationquietingsettingsbridge.*`: consumes the scoped
  settings client and applies accepted Boolean state to the injected interruption
  policy. It never reads JSON or calls settings-center objects. Keep lock privacy
  wiring unchanged.
- Additive registrations in `src/CMakeLists.txt`, `tests/CMakeLists.txt`, install
  rules for executable/protocol headers/schema/activation descriptor/settings
  app desktop entry, and focused tests under `tests/settings`,
  `tests/services/settings_{protocol,service,client}`, and settings-center/shell
  bridge test directories.
- Add ADR-0012 plus Settings1 reference; update settings, notifications, overview,
  module boundaries, notification presentation, roadmap/testing pages, ADR index,
  `mkdocs.yml`, task list, and handoff. The same-change/ADR rule is explicit at
  `docs/wiki/contributing/documentation-policy.md:8-18,47-60`.

## Acceptance test matrix

1. **Schema/migration/repository:** v2 default false and dedicated key (never
   repurpose `services.notifications`, currently at
   `data/settings/schema-v1.json:234-238`); valid v1 sparse/user/profile data
   migrates losslessly; missing key uses false; corrupt JSON, invalid type,
   unknown key, wrong layer, stale/unsupported version, failed migration, failed
   save, and revision exhaustion preserve prior file/model/revision; migration is
   idempotent; save/reopen round trip; no-op leaves file timestamp/content,
   revision, and signal count unchanged; stale revision conflicts atomically.
2. **Protocol/service on private D-Bus:** codec bounds/depth/duplicates/exact
   types; object and name conflict/rollback/release; only user-layer mutation;
   malformed batch and persistence failure publish nothing; two clients conflict;
   directed replies and bounded invalidation; process restart yields new
   owner/epoch while reloading the committed value.
3. **Client:** subscribe-before-read; exact-owner destinations; late old-owner,
   old-epoch, regressing/colliding revision, malformed reply, timeout, retry,
   invalidation burst, activation failure/recovery, owner replacement, operation
   serialization, conflict refresh, confirmed rejection, uncertain commit
   resynchronization, and explicit session-bus `Disconnected` behavior.
4. **Shell bridge/privacy:** fail quiet before baseline; apply persisted true and
   false; retain last accepted value during loss; rebaseline on replacement;
   complete shell reconstruction re-fetches persisted value; DND never changes
   Active/Recent/host state; authenticated privacy denial still clears all
   projections and suppresses critical notifications.
5. **Settings UI:** real route/executable; keyboard Tab/Shift+Tab and Space/Enter;
   screen-reader role/name/description/checked state; visible loading, saving,
   conflict, timeout, and error states; disable or serialize while pending;
   rejected/uncertain writes do not falsely show success; conflict refresh
   exposes authoritative state and permits retry; dependency/source-shape test
   proves no shell presentation import. Update the old center QML test that
   currently proves a session-volatile direct writer
   (`tests/shell/qml/tst_notificationsurfaces.qml:116-180`).
6. **Restart/install/gates:** independent settings-service kill/restart, shell
   client reconstruction, both in sequence, staged activation and installed
   schema/executable/settings app/desktop entry, focused Debug and Release,
   complete registry, production build, QML lint, source-shape, strict docs/link,
   whitespace, and staged install. Offscreen/private-bus evidence must remain
   separate from live assistive-technology/desktop interaction, matching
   `docs/TASK_LIST.md:29-37` and the honesty boundary at
   `docs/wiki/development/testing-harness.md:292-320`.

## Remaining decisions to record before code hardens

- Dedicated key spelling/domain (recommended
  `services.notificationDoNotDisturb`, Boolean, default false).
- Confirm schema v2 migration rather than mutable v1.
- Confirm ordinary same-session user-override writers versus a new trusted-writer
  credential.
- Confirm D-Bus activation versus a new selective supervisor restart policy.
- Confirm fail-quiet-before-baseline/retain-last-on-loss behavior.
- Confirm removal/read-only conversion of the old notification-center toggle.
- Confirm one-call stateless optimistic commits; if explicit long-lived preview
  transaction handles are required now, ownership, disconnect rollback, expiry,
  and per-sender resource bounds need a separate protocol contract.

No provider/model identity or test result is claimed by this audit.
