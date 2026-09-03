# Olive Hazlett-Codex — Display Color C1 repair handoff

- Timestamp: 2026-09-03T06:08:06-06:00
- Candidate commit: `57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd`
- Candidate tree: `3f10877d332bafd75cd40fa6c8064e504f844579`
- Exact product base: `a4ce1f98885d3400f9a9e4c81230ddec8249a9a6`
- Rejected predecessor: `a70d1d46f8bdbb0e219e7c3cb9f30ea74d93121b`
- Requested next action: independent exact review by Emmy Noether, then manager integration.

## Finding closure map

- **P1.1** — `57bcd04`: discovery and import reject a symlink in any injected-root ancestor before enumeration/open. Regressions: `ProfileDiscoveryTests::rejectsSymlinkedRootAncestors` and the named P1.1 subcase in `ProfileImportTests::failsClosedWithoutAUsableUserRoot`.
- **P1.2** — `57bcd04`: import and discovery share the exact case-insensitive `.icc`/`.icm` predicate. Regression: `ProfileImportTests::rejectsNamesDiscoveryCannotEnumerate`.
- **P1.3** — `57bcd04`: same-ID/equal-metadata candidates are boundedly compared as raw bytes; different or unverifiable content conflicts. Regression: `ProfileDiscoveryTests::rejectsBodyOnlyDuplicateConflicts` (the existing exact-duplicate row remains the positive control).
- **P1.4** — `57bcd04`: Applied is accepted only when the authoritative decoded document equals the exact merged submission. Regression: `AssignmentStoreTests::mismatchedAuthoritativeApplyIsUncertain`.
- **P2.1** — `57bcd04`: invalid public `DiscoveryOrigin` values reject the root before access, with descriptor assembly defense in depth. Regression: `ProfileDiscoveryTests::rejectsInvalidInjectedOrigins`.
- **P2.2** — `57bcd04`: expected document and store in-flight state are armed before crossing the injected client seam. Regression: `AssignmentStoreTests::synchronousCompletionTerminatesTheApply`.
- **P2.3** — `57bcd04`: all three installed-consumer scripts select explicit development components containing only their transitive public artifacts. Registered package rows `qindaqt.display-color-{model,discovery,assignment}-installed-cpp-consumer` pass from clean focused builds in both profiles while unrelated `src/profiles/libqindaqt_profiles.a` is absent. The C0 script has the smallest three-line parallel component-selection edit because the required full selector includes that pre-existing row.
- **P2.4** — `57bcd04`: the owning page and ADR now specify that disconnected-output intent persists until explicit removal. Regression: `AssignmentDocumentTests::retainsAssignmentsForDisconnectedOutputs`.
- **P2.5** — `57bcd04`: the predecessor's unauthorized `docs/wiki/index.md` delta is reverted. `git diff a4ce1f98..57bcd04 -- docs/wiki/index.md` is empty.
- **P3.1** — `57bcd04`: Settings1 documentation now limits generic enforcement to JSON/resource/top-level-object constraints and assigns strict record validation to the Display Color consumer. `validate-docs` and strict MkDocs pass.

The reviewer's seven executable modes (`symlink-parent`, `suffix`, `duplicate`, `invalid-origin`, `disconnected-output`, `mismatch`, and `synchronous`) were first run against `a70d1d4`; each exited 1 with the observation recorded in the rejection. The first six behavioral defects now have registered hostile controls; P2.4's policy ambiguity is closed by its registered retention control plus normative text.

## Changed paths from exact base (sorted)

- `data/settings/schema-v2.json`
- `docs/wiki/adr/0057-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md`
- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/display-color-model.md`
- `docs/wiki/architecture/module-boundaries.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/reference/settings1-v1.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `src/services/display_color_assignment/CMakeLists.txt`
- `src/services/display_color_assignment/include/qindaqt/services/display_color_assignment/assignment_document.h`
- `src/services/display_color_assignment/include/qindaqt/services/display_color_assignment/assignment_store.h`
- `src/services/display_color_assignment/src/assignment_document.cpp`
- `src/services/display_color_assignment/src/assignment_store.cpp`
- `src/services/display_color_discovery/CMakeLists.txt`
- `src/services/display_color_discovery/include/qindaqt/services/display_color_discovery/profile_discovery.h`
- `src/services/display_color_discovery/src/icc_text_metadata.cpp`
- `src/services/display_color_discovery/src/icc_text_metadata_p.h`
- `src/services/display_color_discovery/src/import_writer.cpp`
- `src/services/display_color_discovery/src/import_writer_p.h`
- `src/services/display_color_discovery/src/path_safety_p.h`
- `src/services/display_color_discovery/src/profile_discovery.cpp`
- `src/services/display_color_discovery/src/profile_import.cpp`
- `src/services/display_color_discovery/src/profile_import_p.h`
- `tests/CMakeLists.txt`
- `tests/services/display_color_assignment/CMakeLists.txt`
- `tests/services/display_color_assignment/check_boundary.cmake`
- `tests/services/display_color_assignment/check_boundary_negative.cmake`
- `tests/services/display_color_assignment/installed_consumer/CMakeLists.txt`
- `tests/services/display_color_assignment/installed_consumer/installed_cpp_consumer.cpp`
- `tests/services/display_color_assignment/run_installed_cpp_consumer.cmake`
- `tests/services/display_color_assignment/tst_assignment_document.cpp`
- `tests/services/display_color_assignment/tst_assignment_store.cpp`
- `tests/services/display_color_discovery/CMakeLists.txt`
- `tests/services/display_color_discovery/check_boundary.cmake`
- `tests/services/display_color_discovery/check_boundary_negative.cmake`
- `tests/services/display_color_discovery/installed_consumer/CMakeLists.txt`
- `tests/services/display_color_discovery/installed_consumer/installed_cpp_consumer.cpp`
- `tests/services/display_color_discovery/run_installed_cpp_consumer.cmake`
- `tests/services/display_color_discovery/support/icc_file_builder.h`
- `tests/services/display_color_discovery/tst_profile_discovery.cpp`
- `tests/services/display_color_discovery/tst_profile_import.cpp`
- `tests/services/display_color_model/run_installed_cpp_consumer.cmake`

`docs/wiki/index.md` is deliberately absent: its candidate-vs-base diff is empty.

## Executed evidence

- Exact Debug configure from the lane recipe: exit 0.
- Exact Release configure from the lane recipe: exit 0.
- `cmake --build <ROOT>/debug --target clean`: exit 0; removed 2,586 stale artifacts.
- `cmake --build <ROOT>/release --target clean`: exit 0; removed 2,586 stale artifacts.
- `cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt_display_color_model qindaqt_display_color_discovery qindaqt_display_color_assignment qindaqt_color_header_validation_tests qindaqt_color_catalog_tests qindaqt_color_model_tests qindaqt_color_discovery_tests qindaqt_color_import_tests qindaqt_color_assignment_document_tests qindaqt_color_assignment_store_tests`: exit 0 in each profile (70 focused actions after clean). The unrelated profiles archive was absent.
- `ctest --test-dir <ROOT>/{debug,release} -N -R '^qindaqt\.display-color-'`: exit 0; 16 rows registered in each profile.
- `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent HOME=<ROOT>/<profile>/test-home XDG_DATA_HOME=<ROOT>/<profile>/test-xdg QT_FATAL_WARNINGS=1 ctest --test-dir <ROOT>/<profile> -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error`: Debug exit 0, 16/16 passed; Release exit 0, 16/16 passed. Package rows: 3/3 in each profile.
- Final comment-only regression rebuild, `cmake --build <ROOT>/{debug,release} --parallel 3 --target qindaqt_color_import_tests`: exit 0 in each profile; isolated `qindaqt.display-color-discovery-import` rerun exit 0, 1/1 in each profile.
- `./tools/validate-docs`: exit 0; 118 documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0; 1,821 source files checked, with only the two pre-existing threshold warnings (`tests/compositor/CMakeLists.txt` 500 and `tests/services/display_color_model/tst_color_model.cpp` 539).
- `git diff --check`: exit 0 before commit; `git diff --check HEAD^ HEAD`: exit 0 at the candidate.
- `python3 -m json.tool data/settings/schema-v2.json`: exit 0.

## Bounded caveats

- This candidate does not claim compositor application, Settings UI, colord integration, HDR/WCG runtime behavior, nested-session behavior, or physical hardware qualification.
- No host desktop, D-Bus service, hardware, uinput, network, or nested-compositor row was used.
- The configured build trees emitted the repository's known mixed-prefix RPATH warnings; strict owned targets compiled successfully with warnings-as-errors.

Independent exact review then manager integration.
