# Mary Ellen Rudin-Codex — Font F1 rejection-repair handoff

- Timestamp: 2026-09-03T06:36:57-06:00
- Candidate commit: `84367aafe16a410fc51e39fda430abaafdcc39d6`
- Candidate tree: `0f15aab9bd1825e8d8148c6ee7f63a52b949193b`
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`
- Rejected candidate: `abc76f32b5d499b26c6d843bce45cb6dc33ab9b9`
- Preserved takeover commit: `48229fd3df92fcadaa3c4b0c095a4e2ab464f51d`
- Branch: `worker/font-discovery-f1`
- Requested next action: independent exact review, then manager integration.

## Finding closure ledger

- **P1-1 — production discovery composition:** closed by `48229fd3`. Shipped
  `FontSessionBootstrap::applyFromSessionSettings()` reads confirmed Settings1
  values, constructs `FontDiscoveryProvider` with
  `FontDiscoveryRequest::productionDefault()`, gates the family through the
  live catalog, and applies the preference. Registered
  `qindaqt.font-session-bootstrap`, especially
  `probeAppliesConfirmedPreferencesFromRealService`, exercises the production
  root against the real settings service on a private bus and staged default
  fontconfig.
- **P1-2 — injected directory without config read ambient state:** closed by
  `48229fd3`. Such a request is ill-formed and unavailable before fontconfig is
  entered. Registered `qindaqt.font-discovery-hostile` row
  `injectedDirectoryWithoutConfigurationIsRejected` runs with
  `FONTCONFIG_FILE=/dev/null`, `FONTCONFIG_PATH=/nonexistent`, and an ambient
  HOME fontconfig trap.
- **P1-3 — control characters escaped publication:** implementation closed by
  `48229fd3`, complete every-string regression closed by `84367aaf`. Registered
  `qindaqt.font-discovery-hostile` injects a newline style through a scan rule;
  registered `qindaqt.font-catalog` rejects controls independently in family,
  style, and PostScript identity.
- **P1-4 — wrong-typed Settings1 coercion:** exact-typed codec/bootstrap/bridge
  decoding landed in `48229fd3`; `84367aaf` additionally prevents a
  domain-invalid snapshot from granting write-baseline authority. Registered
  `qindaqt.font-preferences-codec`, `qindaqt.font-settings-bridge`, and
  `qindaqt.font-session-bootstrap` cover wholesale rejection, LKG retention,
  inert application defaults, write refusal, and recovery on a later valid
  snapshot.
- **P1-5 — malformed post-commit snapshot advanced writes:** closed by
  `84367aaf`. Registered `qindaqt.font-settings-bridge` row
  `malformedPostCommitSnapshotStopsTheSequence` marks the just-written result
  Uncertain, leaves later keys NotAttempted, emits completion, retains LKG, and
  proves zero next commits/no replay.
- **P1-6 — bootstrap placement and call shape:** call movement/helper policy
  landed in `48229fd3`; exact source regression closed by `84367aaf`. Registered
  `qindaqt.font-application-bootstrap-wiring` requires exactly one helper call
  before `QGuiApplication`/`QApplication` construction in all four first-party
  roots. The same script exits 1 against `abc76f3` (`file_manager` found zero
  valid calls). The four installed application rows pass with the preference
  source absent.
- **P2-1 — hostile and reversed-order controls:** closed by `48229fd3`.
  Registered `qindaqt.font-discovery-bounds` stages 3,000 entries and proves
  deterministic bounded truncation; `qindaqt.font-discovery-hostile` stages a
  broken font, symlink loop, and mode-000 file; `qindaqt.font-discovery` stages
  the same fixture bytes under reversed filenames and compares exact facts.

## Reproduction on `abc76f3`

All reviewer findings were reproduced before repair:

- `git grep -n -E 'FontDiscoveryProvider|productionDefault\\(' abc76f3 --
  'src/*' ':(exclude)src/services/font_discovery/**'` returned no production
  consumer (P1-1).
- Reviewer provider repro in `ambient` mode returned
  `wellFormed=1 available=1 ... facts=2354`, including ambient Arial/C059
  families (P1-2).
- The injected scan-rule repro published `Noto Sans Lycian|Bad` and
  `Style|NotoSansLycian-Regular` on separate lines (P1-3).
- Reviewer bridge repro printed
  `wrong-type bootstrap decode: accepted=1 ... applied=1 ... afterFamily='123'`
  and `wrong-type baseline: ... hasBaseline=1 ... family='123'`
  (P1-4).
- The same repro printed `malformed post-commit snapshot: ... sequenceActive=1
  nextCommits=1` (P1-5).
- Source-line reproduction found all four candidate calls after application
  construction: file manager `104/115`, settings `69/84`, terminal `75/142`,
  editor `65/119` (P1-6).
- Candidate test search found no symlink/chmod/unreadable/thousand/reverse
  regression controls. Its bounds row had eight copies and its determinism row
  repeated the same directory (P2-1).

The new source gate was also run directly against a detached `abc76f3` source
tree:

```sh
cmake \
  -DSOURCE_ROOT=/home/cabewse/work_SPaC3/builds/qindaqt/font-discovery-f1/abc76f3-source \
  -P tests/services/font_discovery/check_application_bootstrap.cmake
```

Exit 1 as required: `file_manager/main.cpp` had zero valid pre-application
`FontSessionBootstrap` calls.

## Changed paths, sorted

- `README.md`
- `docs/wiki/adr/0057-confine-fontconfig-behind-font-discovery.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/font-preferences.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/implementation-roadmap.md`
- `docs/wiki/development/testing-harness.md`
- `mkdocs.yml`
- `ops/team/messages/platform-fonts/1788431793-ruth-teitelbaum-handoff.md`
- `ops/team/workers/ruth-teitelbaum.md`
- `src/CMakeLists.txt`
- `src/apps/file_manager/CMakeLists.txt`
- `src/apps/file_manager/main.cpp`
- `src/apps/settings_center/CMakeLists.txt`
- `src/apps/settings_center/main.cpp`
- `src/apps/terminal/CMakeLists.txt`
- `src/apps/terminal/main.cpp`
- `src/apps/text_editor/CMakeLists.txt`
- `src/apps/text_editor/main.cpp`
- `src/services/font_discovery/CMakeLists.txt`
- `src/services/font_discovery/include/qindaqt/services/font_discovery/font_discovery.h`
- `src/services/font_discovery/include/qindaqt/services/font_discovery/font_session_bootstrap.h`
- `src/services/font_discovery/src/font_discovery.cpp`
- `src/services/font_discovery/src/font_session_bootstrap.cpp`
- `src/services/font_preferences/CMakeLists.txt`
- `src/services/font_preferences/include/qindaqt/services/font_preferences/font_fact.h`
- `src/services/font_preferences/include/qindaqt/services/font_preferences/font_preferences_codec.h`
- `src/services/font_preferences/include/qindaqt/services/font_preferences/font_settings_bootstrap.h`
- `src/services/font_preferences/include/qindaqt/services/font_preferences/font_settings_bridge.h`
- `src/services/font_preferences/src/font_preferences_codec.cpp`
- `src/services/font_preferences/src/font_settings_bootstrap.cpp`
- `src/services/font_preferences/src/font_settings_bridge.cpp`
- `tests/CMakeLists.txt`
- `tests/services/font_discovery/CMakeLists.txt`
- `tests/services/font_discovery/check_application_bootstrap.cmake`
- `tests/services/font_discovery/check_boundary.cmake`
- `tests/services/font_discovery/fixtures/LiberationMono-Regular.ttf`
- `tests/services/font_discovery/fixtures/NotoSansLycian-Regular.ttf`
- `tests/services/font_discovery/fixtures/NotoSansOgham-Regular.ttf`
- `tests/services/font_discovery/fixtures/README.md`
- `tests/services/font_discovery/font_discovery_test_support.h`
- `tests/services/font_discovery/font_session_bootstrap_test_support.h`
- `tests/services/font_discovery/installed_consumer/CMakeLists.txt`
- `tests/services/font_discovery/installed_consumer/installed_font_discovery_consumer.cpp`
- `tests/services/font_discovery/run_installed_font_discovery_consumer.cmake`
- `tests/services/font_discovery/tst_font_discovery.cpp`
- `tests/services/font_discovery/tst_font_discovery_bounds.cpp`
- `tests/services/font_discovery/tst_font_discovery_hostile.cpp`
- `tests/services/font_discovery/tst_font_session_bootstrap.cpp`
- `tests/services/font_preferences/CMakeLists.txt`
- `tests/services/font_preferences/check_boundary.cmake`
- `tests/services/font_preferences/installed_consumer/CMakeLists.txt`
- `tests/services/font_preferences/installed_consumer/installed_font_consumer.cpp`
- `tests/services/font_preferences/run_installed_font_preferences_consumer.cmake`
- `tests/services/font_preferences/tst_font_catalog.cpp`
- `tests/services/font_preferences/tst_font_preferences_codec.cpp`
- `tests/services/font_preferences/tst_font_settings_bootstrap.cpp`
- `tests/services/font_preferences/tst_font_settings_bridge.cpp`

## Executed evidence

Debug configure, exit 0:

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/font-discovery-f1/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Release configure used the identical command with `release` and
`-DCMAKE_BUILD_TYPE=Release`; exit 0.

Both profiles built the following strict focused targets with `--parallel 3`;
exit 0 in each:

```text
qindaqt_font_discovery qindaqt_font_preferences
qindaqt_font_discovery_tests qindaqt_font_discovery_hostile_tests
qindaqt_font_discovery_bounds_tests qindaqt_font_session_bootstrap_tests
qindaqt_font_catalog_tests qindaqt_font_preferences_tests
qindaqt_font_preferences_codec_tests qindaqt_font_bootstrap_tests
qindaqt_font_preferences_coordinator_tests qindaqt_font_settings_bridge_tests
qindaqt_font_settings_bootstrap_tests qindaqt-editor qindaqt-file-manager
qindaqt-terminal qindaqt-settings
```

For each profile, the exact sanitized environment was:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  FONTCONFIG_FILE=/dev/null FONTCONFIG_PATH=/nonexistent \
  HOME=<profile>/test-home TMPDIR=<profile>/test-tmp \
  ctest --test-dir <profile> -R '^qindaqt\.font-' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 16/16 passed.
- Release: exit 0, 16/16 passed.

The four installed application rows ran under the same profile-local sanitized
environment:

```sh
ctest --test-dir <profile> \
  -R '^(qindaqt\.(settings-app-installed-routes|editor-installed-theme-and-metadata|file-manager-installed-runtime|terminal-installed-metadata))$' \
  --output-on-failure --no-tests=error
```

- Debug: exit 0, 4/4 passed.
- Release: exit 0, 4/4 passed.

Static gates:

```sh
./tools/validate-docs
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/font-discovery-f1/site
./tools/check-source-shape
git diff --check
```

All exit 0. `validate-docs` validated 118 Markdown documents plus `mkdocs.yml`;
strict MkDocs completed. Source-shape checked 1,812 files and emitted only the
two pre-existing 500/539-line decomposition-review warnings. No JSON file
changed, so the JSON syntax gate was vacuous.

No `tests/session`, nested compositor, host D-Bus, hardware, uinput, network,
or whole-repository build was run.

## Bounded caveats

- Production-default discovery was exercised with a staged default
  `FONTCONFIG_FILE`; this candidate does not claim the reviewer/user's live host
  font inventory or mutate it.
- The outcome does not include the later Settings font route, a resident Font1
  service, live catalog refresh, global monospace/DPI application, rendered
  typography baselines, live assistive-technology proof, or physical DPI/font
  qualification.
- Evidence is private-bus, injected-fixture, package-stage, and offscreen only.
  It makes no nested-session, host-service, hardware, or visual claim.
