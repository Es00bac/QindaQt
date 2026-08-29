# Ada Ruiz — Settings1 UnknownKey repair handoff

- **Timestamp:** 2026-08-27T12:08:19-06:00
- **Preserved rejected candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Repair candidate:** `2a1e2626e5d4e8e4526bfadbb8100931208f3179`
- **Branch/worktree:** `worker/ada-settings1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/ada-settings1`
- **Tree:** clean
- **Repair size:** 13 paths; 457 insertions; 40 deletions

## Repair outcome

This one new imperative commit repairs the sole P1 in the complete exact-hash
release review without amending `08c7156` or weakening any prior value/null,
lineage, persistence, DND, or activation repair:

1. `SettingsRepository` preflights every operation key after the service has
   validated the request envelope and epoch, but before stale-base or
   revision-exhaustion evaluation. A transaction containing any absent schema
   key now returns explicit `RepositoryCommitStatus::UnknownKey`.
2. UnknownKey has one exact semantic envelope: current before/after revision,
   empty changed keys, and exactly empty value/source maps. A mixed
   known/unknown transaction never returns partial authority. The repository's
   common current-result helper also defensively skips absent keys, preventing
   future paths from manufacturing invalid QVariant as a nonexistent value.
3. The service maps that result directly to wire ordinal 7. Reply encoding no
   longer fails and rewrites a valid semantic rejection as MalformedRequest;
   canonical Nullptr remains the only JSON null and is unchanged.
4. The client validator accepts the empty authority pair only for UnknownKey.
   Every known-key semantic outcome still requires the exact operated-key
   value/source entries. Value-only, source-only, and fabricated paired maps
   on UnknownKey are uncertain/malformed and trigger resync without replay.
5. Reference, service architecture, ADR-0012, status comments, and the testing
   matrix define the exact map shape and precedence: bounded structural
   validation, EpochMismatch fence, UnknownKey preflight, then base/revision
   evaluation.

## Changed paths

- `docs/wiki/adr/0012-persist-notification-quieting-through-settings1.md`
- `docs/wiki/architecture/settings-service.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/settings1-v1.md`
- `src/services/settings_client/src/settings_reply_validation.cpp`
- `src/services/settings_protocol/include/qindaqt/services/settings_protocol/settings_wire_status.h`
- `src/services/settings_service/include/qindaqt/services/settings_service/settings_repository.h`
- `src/services/settings_service/src/{settings_object.cpp,settings_repository.cpp}`
- `tests/services/settings_client/{CMakeLists.txt,tst_settings_commit_reply_validation.cpp}`
- `tests/services/settings_service/{tst_settings_repository.cpp,tst_settings_service_lifecycle.cpp}`

## Exact-source verification

- Focused repository/private-service/client/validator slice — exit 0,
  **4/4 passed**.
- `ctest --test-dir build/ada-debug -R '^qindaqt\.settings-' --output-on-failure -j2`
  — exit 0, **15/15 passed**.
- `cmake --build build/ada-debug -j2` — exit 0.
- `ctest --test-dir build/ada-debug -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **70/70 passed**, zero failures.
- `cmake --build build/ada-release -j2` — exit 0.
- `ctest --test-dir build/ada-release -R '^qindaqt\.' --output-on-failure -j2`
  — exit 0, **70/70 passed**, zero failures.
- `cmake --build build/ada-production-release -j2` — exit 0.
- `cmake --build build/ada-production-release -j2 --target all_qmllint` —
  exit 0; only pre-existing unrelated preview-QML warnings were emitted.
- `tools/validate-docs` — exit 0, **42 Markdown documents** and navigation.
- `build/ada-mkdocs-venv/bin/mkdocs build --strict` — exit 0.
- `tools/check-source-shape --largest 30` — exit 0, **768 source files**, zero
  skips or violations. The new client validator has its own focused test; the
  general client test remains 463 nonblank lines and the private-D-Bus proof is
  a cohesive helper below the function-length gate.
- `git diff --check`, cached diff check, exact commit diff check — exit 0.
- Production Release staged into isolated `build/ada-stage-prefix` — exit 0;
  service executable, activation descriptor, schemas/profile defaults, public
  status/repository headers, client, shell, and settings app were installed.
- Isolated `dbus-run-session` activation through the installed descriptor —
  exit 0. The initial snapshot returned wire schema 1, settings schema 2,
  revision 0, profile-default `appearance.animationDurationMs=int64(160)`, and
  system-default DND false. A staged, well-formed exact-epoch/base unknown-key
  set then returned status 7, revision 0→0, empty changed/value/source maps,
  bounded `unknown key: unknown.key`, no user file, and a private unique owner.
- Repository and real private-D-Bus tests exercise both unknown set and remove,
  unchanged model/revision/file, no SettingsChanged signal, exact empty maps,
  and bounded diagnostics. Repository-only cases also prove UnknownKey
  precedence over stale and exhausted bases.
- Final `git status --short --branch`: clean `worker/ada-settings1` at exact
  `2a1e2626e5d4e8e4526bfadbb8100931208f3179`.

## Cross-lane board decisions

The shared desktop-experience, native-app, and platform-service threads were
read before commit. Settings1 answers are durable and do not widen this repair:

- `native-application-design/1787853958-ada-ruiz-settings1-answer.md` chooses
  post-integration reusable-controller generalization, confirms public
  per-key source layers, records that the wire/service support 1–64 atomic
  operations while the high-level client is currently single-key, and
  confirms path ownership until integration.
- `platform-services/1787853959-ada-ruiz-settings1-consumer-clarification.md`
  maps the current provider baseline/mutation truth to the proposed shared
  app/shell presentation without flattening provider APIs or permitting raw
  D-Bus consumers.

No Settings1 consumer question remains unanswered. The scheduled display
analysis lane is outside this outcome and was not touched.

## Caveats and requested next action

No live desktop, real user session bus, compositor, assistive-technology
bridge, lock screen, KGlobalAccel, uinput, pointer, keyboard, main checkout,
reviewer checkout, Mira worktree, or display-analysis lane was used or
modified. Private D-Bus and offscreen QML remain the intentional safe boundary.

Please assign a different worker to re-review exact candidate
`2a1e2626e5d4e8e4526bfadbb8100931208f3179` against P1 finding `1787852731`
and complete rejection `1787853061`, including unknown set/remove, exact
empty authority maps, status precedence, no signal/file/model/revision change,
client contradictory-map rejection, and regression of all previously accepted
Settings1/DND repairs. Only that exact-hash review should decide integration.
