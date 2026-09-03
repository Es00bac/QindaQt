# Independent exact review — Launcher L2 production shell hosting

- **Reviewer persona:** Annie Cannon, independent shell-composition reviewer
  (slug `annie-cannon`)
- **Provider/model:** Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high
- **Exact candidate commit:** `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1`
  (branch `worker/launcher-composition`, subject "Host the launcher in
  production shell panels")
- **Candidate tree:** `9bfab0137d8a623bb0ccbf6ad44aa5e3654523ab`
- **Parent SHA:** `ae1e0f1611f7b4e653130ab0905294585974182b`
- **Base SHA:** `ae1e0f1611f7b4e653130ab0905294585974182b`
- **Review worktree (read-only, detached at candidate):**
  `/home/cabewse/work_SPaC3/container-wm-workers/launcher-composition-glm-review`
- **Build root:** `/home/cabewse/work_SPaC3/builds/qindaqt/review-launchercomp-glm`
- **Review date:** 2026-09-03

`git rev-parse HEAD` matched the candidate before and after the review;
`git status --porcelain` was empty before and after. No product path was
edited, committed, amended, or rebased. All scratch output stayed under the
assigned build root.

## Review questions answered

### 1. Least authority and seams — PASS

- `launcherDataRoots` (src/shell/runtime/launcherappletcomposition.cpp:68)
  resolves XDG data-home/data-dirs only from an explicitly passed
  `QProcessEnvironment` plus home directory: relative entries are ignored,
  first occurrence/order kept, XDG default `/usr/local/share:/usr/share`
  applied only when `XDG_DATA_DIRS` is empty. The scanner receives explicit
  roots; it never reads the environment. The production caller
  (`ShellRuntimeApplication::initializeLauncherRuntime`,
  src/shell/runtime/shellruntimeapplication.cpp:179) supplies
  `QProcessEnvironment::systemEnvironment()` — the composition root resolves
  the snapshot, exactly as the wiki documents.
- The composition borrows the shell's single public Settings1 client
  (`*m_settingsClient`), now constructed with the key set
  `services.doNotDisturb`, `shell.launcher.pinned`, `shell.launcher.recent`
  (src/shell/runtime/shellruntimeapplication.cpp:181-187) — the same one
  client the notification quieting bridge uses; no second transport.
- The production constructor owns the real seams:
  `Launcher::QProcessLaunchSpawner` and `Launcher::SessionBusActivator`
  over `QDBusConnection::sessionBus()`
  (src/shell/runtime/launcherappletcomposition.cpp:92-105). The test-only
  second constructor injects `RecordingSpawner`/`RecordingActivator`
  (pure recorders — tests/shell/launcher/launcher_runtime_test_support.h:49-80;
  they append to lists and never spawn).
- `applicationsLaunchGranted`
  (src/shell/runtime/launcherappletcomposition.cpp:39-64) evaluates the full
  audited boundary: manifest lookup → `AuditedBuiltin` package identity →
  host selection requiring `InProcessAuditedBuiltin` → compiled
  `BuiltinAppletRegistry::firstParty()` membership → policy evaluation, and
  admits only an affirmative `applications.launch` decision. The controller
  refuses `activate()` when the grant is absent
  (src/shell/launcher/src/launcher_applet_controller.cpp:196-199).
- Stale truth clearing on Settings1 owner loss:
  `LauncherPersistenceController::handleClientState` calls
  `clearAuthoritativeTruth()` on every non-`Ready` client state, and a null
  snapshot clears too (src/shell/launcher/src/launcher_persistence.cpp:72-77,
  157-184). The focused test was truthfully inverted from
  "keeps last confirmed values" to "cleared"
  (tests/shell/launcher/tst_launcher_persistence.cpp:259-275).
- **No test starts a real application.** I inspected every launcher test's
  spawner/activator usage: the composition tests inject the recording seams;
  the only real children anywhere in the slice are the inert `/bin/true` and
  `/bin/false` executor fixtures (pre-existing, L1). I also ran the whole
  `^qindaqt\.launcher-` selector (17 rows, Release) with a before/after
  `ps` snapshot diff: only kernel `kworker` thread-name churn appeared — no
  user-space application process was created.
- The composition test's bus is `dbus-run-session` (a private bus started by
  the test itself, tests/shell/launcher/CMakeLists.txt:163-167); the host
  session bus is never contacted (ambient `DBUS_SESSION_BUS_ADDRESS` unset
  by ENVIRONMENT_MODIFICATION).

### 2. Composition symmetry — PASS

- Registry/manifest/policy were already present from L1 and are untouched by
  this candidate (diff touches neither `data/applets/launcher.json` nor
  `data/applet-policy/`).
- Profile edits are appended placements of exactly one
  `{"id": "applications", "plugin": "launcher"}` in each of the nine family
  profiles (`windows-modern.json` inserts it in zone order between
  `center-tasks` and `status`, leaving every pre-existing entry untouched);
  the default `qindaqt.json` already carried one from L1. The new
  `stockProfilesPlaceOneResolvedLauncher` row
  (tests/applet_runtime/tst_applet_instance_resolver.cpp:235-267) loads all
  ten real profiles, resolves each launcher instance through the real
  resolver, and asserts `ready`, entry point `qindaqt.applets.launcher`, and
  grant set exactly `{applications.launch}` — non-vacuous.
- QML/runtime edits mirror the Audio pattern exactly:
  `launcherAppletAccess` threaded RuntimePanel → PanelContent →
  rows/columns → dispatcher property, compiled module import in
  `BuiltinAppletContent.qml`, factory injection in
  `runtimepanelwindowfactory.{h,cpp}`. The one justified deviation — a
  depth-bounded ancestor lookup for the facade, because `AppletChip.qml` is
  outside the lane's ownership — is documented in an AGENT-CONTRACT comment
  and proven by `tst_launcher_dispatcher.qml` (production row carries the
  facade; preview renders the compiled disabled fallback with null access).
- `git diff ae1e0f1..7ecdb36 -- src/shell/audio_applet src/shell/power_applet
  src/shell/bluetooth_applet` is empty: Audio, Power, and Bluetooth applet
  sources are byte-identical to the base.
- Keyboard/accessibility claims hold under fatal warnings: the offscreen row
  (`QT_FATAL_WARNINGS=1`, offscreen software, host endpoints unset) and the
  dispatcher row (same isolation) passed in Debug and Release. Popup search
  focus, Escape-close, and keyboard parity are the compiled module's L1
  offscreen assertions, which still pass unchanged.

### 3. Packaging — PASS

- A `LauncherAppletRuntime` install component ships `qindaqt-shell`, the
  launcher manifest, `qindaqt.json` profile, theme, policy,
  `qindaqt_controls_qml` + `qindaqt_shell_launcher_qml` in the install
  libdir, and `qindaqt_tokens_qml` at the sibling `Tokens` path
  (src/shell/CMakeLists.txt:362-401). The three sibling applet components
  and the default component gained the launcher backing library they now
  directly link — an additive closure requirement, not a rewrite.
- `qindaqt.shell-runtime-component-closure` now installs each of the five
  shell-carrying components alone, requires and authenticates the staged
  launcher library resolution, and runs the staged shell with ambient
  loader/display/bus variables cleared
  (tests/shell/audio_applet/run_shell_component_closure.cmake).
- `qindaqt.launcher-installed-package` performs a real relocation (stage
  renamed after component-filtered install), stages KF6 GlobalAccel and
  Tokens manually, authenticates loader resolution with `LD_LIBRARY_PATH`
  unset, verifies compiled-QML evidence in the backing library, then runs
  the staged shell `--list` and the staged-QML probe under broken
  source-poison manifests/profiles/themes/policy and poisoned
  `XDG_DATA_*`/override variables with explicit staged paths
  (tests/shell/launcher/run_installed_launcher.cmake). Both profiles pass.

### 4. Boundaries and docs — PASS

- `git diff ae1e0f1..7ecdb36 -- src/compositor src/shell/global_menu` is
  empty; the excluded paths are untouched.
- `qindaqt.launcher-runtime-boundary` now also scans the production
  composition sources for settings-service internals, QML/Quick, and
  environment reads, with a poison negative control that must be rejected
  (tests/shell/launcher/check_launcher_boundary.cmake).
- `./tools/check-source-shape` exits 0 (2009 files; only the three
  pre-existing decomposition-review warnings — same as base).
- The wiki claims match the evidence: `launcher.md` gained the production
  hosting section, the focused-test table rows all exist and pass, and the
  Non-claims section still disclaims activation tokens, Settings1 schema
  registration, real session-bus activation, and nested-session behavior.
  `applet-runtime.md` now says the dispatcher renders all six entry points
  (true: six renderers in `BuiltinAppletContent.qml`, contract-text-gated).
  The module-boundaries table moves production composition authority to
  `src/shell/runtime`, matching where the code lives. No stale
  "keeps last confirmed" claim remains anywhere in the lane's docs/tests.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

1. **Persistence clearing is broader than the wiki's enumeration.**
   `handleClientState` clears pinned/recent truth on every non-`Ready`
   state, which includes `ClientState::Degraded` (snapshot timeout,
   malformed snapshot, token exhaustion —
   src/services/settings_client/src/settings_client.cpp:77,314,438), while
   `docs/wiki/shell/launcher.md` enumerates "Settings1 owner or transport
   loss". The behavior is strictly more fail-closed, matches the in-code
   AGENT-GUARD ("owner loss, replacement, or malformed resync"), and
   converges on the next Ready snapshot, so this is a wording-precision
   note only. Reproduction: src/shell/launcher/src/launcher_persistence.cpp:157-169
   vs docs/wiki/shell/launcher.md:156-158.
2. **Bounded ancestor lookup uses a fixed depth of 4.**
   `inheritedLauncherAccess()` walks at most four ancestors
   (src/shell/qml/BuiltinAppletContent.qml:44-56). Today the facade is
   found at depth 1 (chip → row/column), and the failure mode of a deeper
   hierarchy is fail-closed (disabled launcher, no crash), but the bound is
   magic and only pinned by the dispatcher test's current hierarchy. A
   future panel refactor that deepens nesting would silently disable the
   launcher rather than produce an error.

## Evidence — exact commands and results

All commands ran from the review worktree unless noted. `<ROOT>` =
`/home/cabewse/work_SPaC3/builds/qindaqt/review-launchercomp-glm`.

### Configure

- Debug: `cmake -S . -B <ROOT>/debug -G Ninja -C
  /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
  — exit 0 (one pre-existing CMake warning about Qt library directories in
  the deps root, unrelated to this lane; also present for unrelated
  compositor test targets).
- Release: same with `-B <ROOT>/release -DCMAKE_BUILD_TYPE=Release` — exit 0.

### Builds (focused targets)

`cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt-shell
qindaqt-shell-preview qindaqt_desktop_entry_parser_tests
qindaqt_application_catalog_tests qindaqt_launcher_category_model_tests
qindaqt_launcher_search_ranker_tests qindaqt_launcher_pinned_recent_tests
qindaqt_launcher_presentation_tests qindaqt_launcher_scanner_tests
qindaqt_launcher_composition_tests qindaqt_launcher_execution_tests
qindaqt_launcher_executor_tests qindaqt_launcher_persistence_tests
qindaqt_launcher_controller_tests qindaqt_launcher_qml_tests
qindaqt_launcher_installed_probe qindaqt_applet_manifest_tests
qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests
qindaqt_shell_runtime_options_tests qindaqt_profile_json_validation_tests
qindaqt_profile_tests qindaqt_profile_value_validation_tests`

- Debug: exit 0 (665/665).
- Release: exit 0 (665/665).

### Test runs (all under `env -u DBUS_SESSION_BUS_ADDRESS -u DISPLAY -u WAYLAND_DISPLAY`)

| Selector | Profile | Result |
| --- | --- | --- |
| `ctest -R '^qindaqt\.launcher-' --output-on-failure --no-tests=error` | Debug | exit 0, 17/17 passed |
| same | Release | exit 0, 17/17 passed |
| `ctest -R '^(qindaqt\.applet-(manifest\|catalog\|runtime-resolution)\|qindaqt\.shell-runtime-(options\|catalog\|component-closure)\|qindaqt\.(audio\|bluetooth\|power)-applet-installed-package)$' --output-on-failure --no-tests=error` | Debug | exit 0, 9/9 passed |
| same | Release | exit 0, 9/9 passed |
| `ctest -R '^qindaqt\.profile-' --output-on-failure --no-tests=error` | Debug | exit 0, 3/3 passed |
| same | Release | exit 0, 3/3 passed |

The 17 launcher rows are: desktop-entry-parser, application-catalog,
category-model, search-ranker, pinned-recent, presentation,
application-scanner, composition (private dbus-run-session bus), executor,
persistence, controller, offscreen, panel-dispatcher, runtime-boundary,
contract-text, installed-package, execution. Process monitoring around the
full selector run showed no user-space application children (only kernel
kworker name churn), confirming the recording-seam claim.

### Static gates

- `./tools/validate-docs` — exit 0 ("Validated 126 Markdown documents and
  mkdocs.yml navigation").
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir <ROOT>/site` — exit 0.
- `./tools/check-source-shape` — exit 0, 2009 files checked, skipped 0; only
  the three pre-existing decomposition-review warnings
  (tests/compositor/CMakeLists.txt, tst_color_model.cpp,
  tst_audio_applet_controller.cpp — identical set to the base).
- `git diff --check` — exit 0.
- `python3 -m json.tool data/profiles/<each of the nine changed>.json` —
  exit 0 for all nine.

### Integrity

- `git rev-parse HEAD` → `7ecdb36f53c60ea03bea40197f17c9cc6bf9b9e1` (before
  and after review).
- `git rev-parse HEAD^{tree}` → `9bfab0137d8a623bb0ccbf6ad44aa5e3654523ab`.
- `git rev-parse HEAD~1` → `ae1e0f1611f7b4e653130ab0905294585974182b`.
- `git status --porcelain` → empty (before and after review).
- Sibling/excluded paths: `git diff ae1e0f1..7ecdb36 -- src/shell/audio_applet
  src/shell/power_applet src/shell/bluetooth_applet src/compositor
  src/shell/global_menu` → empty.

### Coverage not run

- No `tests/session` nested-compositor rows, host D-Bus system/session
  services, hardware, uinput, or network were invoked, per the review
  contract. The composition row's `dbus-run-session` bus is test-private and
  was the only bus exercised.

## Verdict

The candidate delivers the claimed L2 outcome with truthful evidence: the
production composition resolves explicit XDG roots, borrows the shell's real
Settings1 client, owns the real spawner/activator seams behind the audited
`applications.launch` grant, clears stale identity truth on authority loss,
renders the compiled module in production panels and the preview fallback,
places exactly one resolved launcher in all ten stock profiles, and ships a
relocatable `LauncherAppletRuntime` component proven by closure and
installed-package rows under source poison. Documentation matches the code.
Two P3 precision notes do not block integration.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/2
