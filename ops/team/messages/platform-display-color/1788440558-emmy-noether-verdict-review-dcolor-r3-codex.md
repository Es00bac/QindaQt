# Emmy Noether — Display Color C1 second-repair exact-candidate recheck

- Persona: **Emmy Noether**, independent platform reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `4c4f2c4693ad519587a63077dbb30efc1cde38a0`
- Tree SHA: `8912e339a3ede6835c7f4308eb0a936f7d0c1da7`
- Parent SHA: `a4230a5e7f08f8bb95f5a4f2cf6f610ccf49f4ea`
- Base SHA: `a4ce1f98885d3400f9a9e4c81230ddec8249a9a6`
- Rejected product ancestor: `57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex`
- Review result: **ACCEPT**

The worktree was clean and detached at the exact candidate before and after
review. I used only the repository's registered in-process tests and their
small fixture directories under the assigned build root. I did not run any
`tests/session` or nested-compositor row and did not access a host D-Bus
service, hardware, uinput, or the network.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Repair-contract answers

### Canonical root/candidate containment and import ordering

Yes. `CanonicalRootContainment::resolve()` rejects an empty/NUL path and any
raw `..` component, resolves the lexical absolute and canonical root once, and
rejects failed canonicalization or any lexical/canonical mismatch
(`src/services/display_color_discovery/src/path_safety_p.h:19-36`). Candidate
admission canonicalizes an existing candidate and fails closed on an empty
canonical result (`path_safety_p.h:41-47`). A destination canonicalizes the
candidate when it exists or its existing parent when it does not; a failed
canonicalization is outside (`path_safety_p.h:49-64`). The final lexical test
admits only the canonical root itself where explicitly allowed or a path with
the canonical-root-plus-separator prefix (`path_safety_p.h:80-94`), avoiding
the sibling-prefix error.

Discovery resolves the injected root before enumeration and calls that
candidate admission check before `examineCandidateFile()` performs its
`QFileInfo`, open, or bounded metadata reads
(`src/services/display_color_discovery/src/profile_discovery.cpp:296-339`).
Import creates one `ImportRootAccess` only after canonical containment plus
ownership/mode validation and retains its directory descriptor
(`src/services/display_color_discovery/src/import_writer.cpp:62-75,151-163`).
`importProfileFromSource()` rejects failure to create that authority and checks
the canonical destination before invoking either the injected inspection seam
or the production inspector
(`src/services/display_color_discovery/src/profile_import.cpp:162-179`). The
production inspector repeats containment before `fstatat`/`openat`, and writes
remain descriptor-relative (`import_writer.cpp:194-288`). Thus the rejected
root path cannot reach destination stat, read, digest, temporary cleanup, or
write.

Registered coverage is direct and non-vacuous:

- `ProfileDiscoveryTests::rejectsParentReferencesBeforeCanonicalization`
  constructs `injected/redirect/../icc` beneath a linked ancestor and requires
  an empty catalog plus the rejection diagnostic
  (`tests/services/display_color_discovery/tst_profile_discovery.cpp:121-140`).
- `ProfileImportTests::rejectsParentReferencesBeforeWriting` constructs the
  analogous `redirect/../user` root and requires `WriteFailed`,
  `invalid-user-root`, and no outside copy
  (`tests/services/display_color_discovery/tst_profile_import.cpp:264-286`).
- `ProfileImportTests::rejectsUnsafeRootBeforeDestinationInspection` places an
  existing destination beyond a linked ancestor, injects a recording inspector,
  and requires `WriteFailed`, zero inspector calls, and unchanged canary bytes
  (`tst_profile_import.cpp:288-324`).
- Both functions belong to the registered discovery/import executables, whose
  CTest rows are declared at
  `tests/services/display_color_discovery/CMakeLists.txt:19-26,42-49`.

These assertions would fail against `57bcd04` by direct source comparison. Its
`injectedRootHasSymlinkedAncestor()` begins from
`QFileInfo(path).absolutePath()`, which lexically removes `redirect/..` before
walking ancestors (`57bcd04:path_safety_p.h:15-28`); its discovery then admits
the raw root and calls `examineCandidateFile()` without candidate
canonicalization (`57bcd04:profile_discovery.cpp:291-334`). The discovery test
would therefore see the outside profile instead of an empty catalog. The
ancestor import likewise opens the raw normalized-through-link root and can
write the outside copy, contradicting the expected `WriteFailed` and absence.
For the existing-destination case, ancestor
`profile_import.cpp:164-179` calls its digest helper and then
`QFileInfo(destination).exists()` before the writer's root rejection; the
outside canary therefore produces `DestinationConflict`, not the asserted
`WriteFailed`, and the ancestor has no pre-inspection seam. The current test
source is not source-compatible with that older private API, but its asserted
observable result is the opposite of the ancestor control flow.

### Invalid-origin validation

Yes. `assembleDescriptor()` now initializes `wireValid` before the origin
switch, so the default arm's `false` value survives common-field assembly
(`src/services/display_color_discovery/src/icc_text_metadata.cpp:259-292`).
`ProfileDiscoveryTests::invalidOriginsStayInvalidDuringDescriptorAssembly`
directly passes origin `99`, requires `wireValid == false`, and requires C0
validation to return `MalformedMetadata`
(`tests/services/display_color_discovery/tst_profile_discovery.cpp:159-171`).
On `57bcd04`, the default arm sets `false` but line 291 immediately overwrites
it with `true`, so both current assertions fail. This closes prior P3.1 with an
effective test.

## Commands and results

### Identity, ancestry, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse a4ce1f98885d3400f9a9e4c81230ddec8249a9a6
git merge-base a4ce1f98885d3400f9a9e4c81230ddec8249a9a6 HEAD
git status --porcelain=v1
```

Exit 0. Values match the header; the merge-base is the stated base; status was
empty before and after the review.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0. The identical command with `-B .../release` and
`-DCMAKE_BUILD_TYPE=Release` exited 0. Both emitted the repository's existing
mixed-prefix RPATH warnings and completed generation successfully.

### Focused build

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile> \
  --parallel 3 --target \
  qindaqt_display_color_model qindaqt_display_color_discovery \
  qindaqt_display_color_assignment qindaqt_color_header_validation_tests \
  qindaqt_color_catalog_tests qindaqt_color_model_tests \
  qindaqt_color_discovery_tests qindaqt_color_import_tests \
  qindaqt_color_assignment_document_tests qindaqt_color_assignment_store_tests
```

Debug exit 0; Release exit 0. These were incremental builds and each emitted
two AutoMOC/UIC actions with no compiler or linker failure.

### Registered selectors

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile> \
  -N -R '^qindaqt\.display-color-'
```

Exit 0 in each profile; 16 rows registered in each.

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile>/test-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile>/test-xdg \
  QT_FATAL_WARNINGS=1 \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile> \
  -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error
```

- Debug: exit 0, 16/16 passed, 0 failed; 3/3 package rows passed.
- Release: exit 0, 16/16 passed, 0 failed; 3/3 package rows passed.
- The unrelated `src/profiles/libqindaqt_profiles.a` was absent in both build
  trees after the selectors.

The exact repaired functions were also selected from the already-registered
Debug test executables under the same environment:

```sh
./qindaqt_color_discovery_tests \
  rejectsSymlinkedRootAncestors \
  rejectsParentReferencesBeforeCanonicalization \
  invalidOriginsStayInvalidDuringDescriptorAssembly

./qindaqt_color_import_tests \
  rejectsParentReferencesBeforeWriting \
  rejectsUnsafeRootBeforeDestinationInspection
```

Both commands exited 0. Discovery reported 5 passed (including init/cleanup),
0 failed; import reported 4 passed (including init/cleanup), 0 failed.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; 118 Markdown documents and `mkdocs.yml` navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/site-r3
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0; 1,821 source files checked. It reported only the two existing
threshold warnings: `tests/compositor/CMakeLists.txt` at 500 non-blank lines
and `tests/services/display_color_model/tst_color_model.cpp` at 539.

```sh
git diff --check
git diff --check 57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd..4c4f2c4693ad519587a63077dbb30efc1cde38a0
git diff --name-only 57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd..4c4f2c4693ad519587a63077dbb30efc1cde38a0 -- '*.json'
```

All exited 0; the JSON query returned no paths, so no `python3 -m json.tool`
invocation applied.

## Verdict

The second repair closes both prior P1 findings and the remaining P3 precision
finding with direct registered regressions. The implementation and updated
documentation agree on canonical path admission, fail-closed containment, and
pre-inspection ordering. All required Debug, Release, package/relocation, and
static evidence passed on the exact immutable candidate. Independent
exact-candidate acceptance is granted.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
