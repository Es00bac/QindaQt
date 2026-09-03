# Olive Hazlett-Codex — Display Color C1 second repair handoff

- Timestamp: 2026-09-03T06:36:37-06:00
- Exact candidate commit: `4c4f2c4693ad519587a63077dbb30efc1cde38a0`
- Candidate tree: `8912e339a3ede6835c7f4308eb0a936f7d0c1da7`
- Candidate parent: `a4230a5e7f08f8bb95f5a4f2cf6f610ccf49f4ea`
- Rejected ancestor repaired: `57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd`
- Exact lane base: `a4ce1f98885d3400f9a9e4c81230ddec8249a9a6`

## Finding closure map

| Finding | Closing commit | Regression / proof |
| --- | --- | --- |
| P1.1 `..` hides linked root ancestor | `4c4f2c4693ad519587a63077dbb30efc1cde38a0` | `ProfileDiscoveryTests::rejectsParentReferencesBeforeCanonicalization` and `ProfileImportTests::rejectsParentReferencesBeforeWriting` construct `redirect/..` paths that resolve outside on `57bcd04`, then require empty discovery / `WriteFailed invalid-user-root` and no outside copy. |
| P1.2 destination inspected after root rejection | `4c4f2c4693ad519587a63077dbb30efc1cde38a0` | `ProfileImportTests::rejectsUnsafeRootBeforeDestinationInspection` plants an outside-root `x.icc` canary and injects a recording destination-filesystem seam; the rejected linked root returns `WriteFailed invalid-user-root`, the seam records zero inspections, and the canary remains byte-identical. |
| P3.1 invalid-origin marker overwritten | `4c4f2c4693ad519587a63077dbb30efc1cde38a0` | `ProfileDiscoveryTests::invalidOriginsStayInvalidDuringDescriptorAssembly` calls the private assembly boundary with origin `99`, requires `wireValid == false`, and requires C0 `MalformedMetadata`. |

The implementation replaces ancestor walking with one retained canonical-root
authority. Root paths containing `..`, failing canonicalization, or differing
from their lexical absolute path reject before enumeration/import destination
access. Discovery files and import destinations (or the existing parent of a
new destination) must canonicalize lexically within that root before any later
inspection. Import also retains one validated directory descriptor across the
destination check, digest, and atomic write.

## Changed paths

- `docs/wiki/adr/0057-discover-icc-profiles-from-injected-roots-and-persist-assignments-through-settings1.md`
- `docs/wiki/architecture/display-color-model.md`
- `docs/wiki/development/testing-harness.md`
- `src/services/display_color_discovery/src/icc_text_metadata.cpp`
- `src/services/display_color_discovery/src/import_writer.cpp`
- `src/services/display_color_discovery/src/import_writer_p.h`
- `src/services/display_color_discovery/src/path_safety_p.h`
- `src/services/display_color_discovery/src/profile_discovery.cpp`
- `src/services/display_color_discovery/src/profile_import.cpp`
- `src/services/display_color_discovery/src/profile_import_p.h`
- `tests/services/display_color_discovery/CMakeLists.txt`
- `tests/services/display_color_discovery/tst_profile_discovery.cpp`
- `tests/services/display_color_discovery/tst_profile_import.cpp`

## Final acceptance evidence

Both exact common-contract configure commands were run with build directories
`/home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/debug` and
`.../release`, the pinned initial cache, `BUILD_TESTING=ON`, all three required
build switches, host uinput disabled, and strict warnings enabled. Debug and
Release each exited 0; only the repository's known mixed-prefix RPATH warnings
were emitted.

The exact focused build command was run in both profiles:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/<profile> \
  --parallel 3 --target \
  qindaqt_display_color_model qindaqt_display_color_discovery \
  qindaqt_display_color_assignment qindaqt_color_header_validation_tests \
  qindaqt_color_catalog_tests qindaqt_color_model_tests \
  qindaqt_color_discovery_tests qindaqt_color_import_tests \
  qindaqt_color_assignment_document_tests qindaqt_color_assignment_store_tests
```

Final Debug exited 0 after 65 actions; final Release exited 0 after 10
incremental actions. No warning was promoted to an error. The focused repair
functions were also invoked directly in Debug: discovery/root/origin functions
passed 5/5 QtTest cases and import/root/pre-inspection functions passed 5/5.

The complete registered selector was then run from each build root:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/<profile>/test-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/<profile>/test-xdg \
  QT_FATAL_WARNINGS=1 \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/<profile> \
  -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 16/16 passed.
- Release: exit 0, 16/16 passed.
- `ctest -N` reported 16 rows in each profile; all three package/relocation
  rows passed in both, and `src/profiles/libqindaqt_profiles.a` remained absent
  in both build trees.

Final static gates:

- `./tools/validate-docs`: exit 0; 118 Markdown documents/navigation validated.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/display-color-c1/site`: exit 0.
- `./tools/check-source-shape`: exit 0; 1,821 files checked, with only the two existing threshold warnings (`tests/compositor/CMakeLists.txt` 500 and `tests/services/display_color_model/tst_color_model.cpp` 539).
- `git diff --check`: exit 0 before the product commit.
- `git diff --check 57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd..4c4f2c4693ad519587a63077dbb30efc1cde38a0`: exit 0.
- No JSON changed, so no JSON syntax gate applied.

During implementation, the first focused compile exited 1 because the private
inspector alias was missing; adding the declared alias closed it. The first
full selectors passed 15/16 in each profile because canonical-root enumeration
changed relative diagnostic paths to absolute; preserving the already-proven
raw spelling restored compatibility while canonical paths remain the admission
authority. A non-required `clang-format --dry-run --Werror` diagnostic exited
1 because the repository's existing files are not byte-identical to the local
formatter output; no formatter rewrite was applied. All required final gates
above were rerun from the committed product bytes and passed.

## Bounded caveats

This candidate claims deterministic in-process filesystem/model/package proof
only. It deliberately does not claim compositor ICC application, HDR/WCG
runtime behavior, Settings UI, colord interaction, nested sessions, physical
hardware, host D-Bus, uinput, or network evidence. No host desktop or service
was touched.

Requested next action: **independent exact review by Emmy Noether, then manager integration**.
