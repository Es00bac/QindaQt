# Emmy Noether — Display Color C1 repair-descendant exact-candidate recheck

- Persona: **Emmy Noether**, independent platform reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `57bcd04e32b8e0e42e3eeeece9eae84db0d2e1dd`
- Tree SHA: `3f10877d332bafd75cd40fa6c8064e504f844579`
- Parent SHA: `a70d1d46f8bdbb0e219e7c3cb9f30ea74d93121b`
- Base SHA: `a4ce1f98885d3400f9a9e4c81230ddec8249a9a6`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex`
- Review result: **REJECT**

The worktree was clean and detached at the exact candidate before and after
review. All scratch sources, fixture directories, and generated output remained
under the assigned build root. No session/nested-compositor row, host D-Bus
service, hardware, uinput, or network was used.

## Findings ledger

### P0

None.

### P1

#### P1.1 — A `..` component hides an ancestor symlink from the new root checker, allowing discovery reads and import writes outside the apparent injected path

`injectedRootHasSymlinkedAncestor()` starts from
`QFileInfo(path).absolutePath()`
(`src/services/display_color_discovery/src/path_safety_p.h:15-28`). Qt
lexically cleans the parent path, so a root such as
`injected/redirect/../icc` loses the `redirect` component before the loop
checks it. The final checks and opens then resolve the original path through the
symlink: discovery passes the helper at
`src/services/display_color_discovery/src/profile_discovery.cpp:291-318`,
while import passes it and opens the raw path with `O_NOFOLLOW` applying only
to the final component at
`src/services/display_color_discovery/src/import_writer.cpp:65-80,152-160`.
This contradicts the owning page and ADR rule that a root containing any
symlink component is rejected before access.

Discovery reproduction:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-xdg \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build/candidate_repros \
  dotdot-symlink-parent \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Exit 1. Observed `profiles=1` with a source under
`injected/redirect/../icc/outside.icc`. Expected zero profiles and a
root-ancestor-symlink diagnostic.

Import reproduction:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-xdg \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build/candidate_repros \
  import-dotdot-symlink \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Exit 1. Observed `import_status=0 outside_created=1`: import reported
`Imported` and created `outside/user/x.icc`. Expected invalid-root
rejection and no file outside the apparent injected path. The fixture was
confined to the assigned build root and removed automatically.

The registered `rejectsSymlinkedRootAncestors` and import subcase cover only
`injected/redirect/icc`; neither assertion covers a path-resolution sequence
containing `redirect/..`.

#### P1.2 — Import inspects a destination beyond a straightforward symlinked ancestor before rejecting the root

For a normal `injected/redirect/user` path, the new check in
`existingFileDigestIfIdentical()` correctly returns no digest
(`src/services/display_color_discovery/src/import_writer.cpp:211-219`).
`importProfileFromSource()` then constructs a fresh path-based
`QFileInfo` and calls `exists()/isSymLink()`
(`src/services/display_color_discovery/src/profile_import.cpp:164-175`).
That stat follows the already-known ancestor symlink. Consequently an external
`x.icc` changes the result to `DestinationConflict` before
`atomicWriteProfileCopy()` can report the invalid injected root. The
registered import regression checks only the outside-destination-absent case,
so it does not prove rejection before destination access.

Reproduction:

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repro-xdg \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build/candidate_repros \
  import-symlink-existing \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Exit 1. Observed `import_status=8 reason=destination-conflict` when the
symlink target contained `x.icc`. Expected the same
`WriteFailed/invalid-user-root` outcome produced when that outside file is
absent, without inspecting the destination beyond the rejected root.

### P2

None.

### P3

#### P3.1 — The claimed descriptor-assembly defense for invalid origins is overwritten immediately

The default switch arm in
`src/services/display_color_discovery/src/icc_text_metadata.cpp:269-285`
sets `descriptor.wireValid = false`, but the unconditional assignment at
line 291 sets it back to true. The public discovery entry point now rejects
invalid origins first, so the predecessor's reachable invalid-origin defect is
closed; this is an unreachable defense-in-depth/source-precision defect rather
than a second blocking finding. The handoff's claim that descriptor assembly
also provides defense in depth is not true as written.

## Predecessor-finding recheck

- **Prior P1.1, simple ancestor symlink:** the original
  `symlink-parent` reproduction now exits 0 with `profiles=0`.
  `ProfileDiscoveryTests::rejectsSymlinkedRootAncestors` and the import
  subcase are registered and directly assert simple ancestor rejection.
  P1.1/P1.2 above show that the repaired invariant is still incomplete for
  path normalization and import destination inspection.
- **Prior P1.2, undiscoverable imported suffix:** `suffix` exits 0 with
  `import_status=6` and no discovered profile.
  `ProfileImportTests::rejectsNamesDiscoveryCannotEnumerate` asserts
  rejection and no stored file; the shared case-insensitive predicate is used
  by discovery and import.
- **Prior P1.3, body-only duplicate conflict:** `duplicate` exits 0 with
  `profiles=0 duplicate-collapsed=0 conflict=1`.
  `ProfileDiscoveryTests::rejectsBodyOnlyDuplicateConflicts` mutates an
  otherwise uninspected byte and asserts catalog rejection.
- **Prior P1.4, authoritative apply mismatch:** `mismatch` exits 0 with
  `apply_status=3 persisted=<empty>`.
  `AssignmentStoreTests::mismatchedAuthoritativeApplyIsUncertain` asserts
  `Uncertain`, the precise reason, no published persisted document, and no
  in-flight write.
- **Prior P2.1, invalid origin:** `invalid-origin` exits 0 with
  `profiles=0`. The registered discovery regression asserts an empty
  incomplete result and `invalid-origin` diagnostic. The reachable defect is
  closed; the private precision issue is P3.1.
- **Prior P2.2, synchronous completion:** `synchronous` exits 0 with
  `accepted=1 finished=1 store_write_in_flight=0
  client_write_in_flight=0`. The registered store regression asserts one
  `Applied` terminal outcome with the submitted document and both guards
  released.
- **Prior P2.3, focused installed consumers:** the exact selector passes
  16/16 in Debug and Release after building only the ten focused targets.
  `src/profiles/libqindaqt_profiles.a` remained absent in both build trees,
  proving the three package rows no longer rely on an unrelated full build.
- **Prior P2.4, disconnected output:** the old observational
  `disconnected-output` mode exits 1 because it intentionally returns 1 when
  the record is retained. Retention is now the expected policy:
  `AssignmentDocumentTests::retainsAssignmentsForDisconnectedOutputs`
  asserts the untouched disconnected record, and both the owning page and
  ADR-0057 specify retention until explicit removal.
- **Prior P2.5, index ownership:** `git diff
  a4ce1f98885d3400f9a9e4c81230ddec8249a9a6..57bcd04 --
  docs/wiki/index.md` is empty. The unauthorized predecessor delta is gone.
- **Prior P3.1, Settings1 wording:** the reference now says Settings1 enforces
  generic JSON/resource/top-level-object constraints and the Display Color
  consumer owns strict record decoding. This matches the schema and consumer.
- The prior ambient canary mode also exits 0 with zero profiles under
  redirected `HOME` and `XDG_DATA_HOME`.

The new regression assertions were inspected in the registered QtTest sources.
Their expected values are the opposites of the exact predecessor behaviors
recorded and executed in the first verdict; source comparison against
`a70d1d4` confirms the predecessor lacks the checks/state those assertions
exercise. The focused descendant executions below run those functions
directly in addition to the full registered selector.

## Commands and results

### Identity, ancestry, and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse a4ce1f98885d3400f9a9e4c81230ddec8249a9a6
git merge-base a4ce1f98885d3400f9a9e4c81230ddec8249a9a6 57bcd04
git status --porcelain
```

Exit 0. Values match the header and merge-base equals the base SHA. Status was
empty before and after review.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0. The identical command with `-B .../release
-DCMAKE_BUILD_TYPE=Release` exited 0. Both emitted the repository's known
mixed-prefix RPATH warnings.

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

Debug exit 0 (70 focused actions); Release exit 0 (31 incremental actions).
A second Debug invocation exited 0 with `ninja: no work to do`.

### Registered selectors

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile> \
  -N -R '^qindaqt\.display-color-'
```

Exit 0 in each profile; 16 rows registered.

```sh
env -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile>/test-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile>/test-xdg \
  QT_FATAL_WARNINGS=1 \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/<profile> \
  -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error
```

Debug exit 0, 16/16 passed; Release exit 0, 16/16 passed. Package rows were
3/3 in each profile. The unrelated profiles archive was absent after both
selectors.

Focused repair functions were also invoked directly in Debug:

- discovery ancestor/origin/body-duplicate functions: exit 0, 5/5 QtTest
  cases including init/cleanup;
- import root/suffix functions: exit 0, 4/4;
- assignment mismatch/synchronous functions: exit 0, 4/4;
- disconnected-retention function: exit 0, 3/3.

### Prior and additional scratch controls

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build --parallel 3
```

Exit 0. The scratch-only compiler emitted two `nodiscard` warnings for the
pre-existing reproducer's intentionally unchecked `SettingsClient::start()`;
candidate targets themselves built with strict warnings.

The exact predecessor modes produced:

| Mode | Exit | Descendant observation |
| --- | ---: | --- |
| `symlink-parent` | 0 | `profiles=0` |
| `suffix` | 0 | `import_status=6 discovered_profiles=0` |
| `duplicate` | 0 | `profiles=0 duplicate-collapsed=0 conflict=1` |
| `invalid-origin` | 0 | `profiles=0` |
| `disconnected-output` | 1 | `disconnected_record_retained=1`; now the specified policy |
| `mismatch` | 0 | `apply_status=3 persisted=<empty>` |
| `synchronous` | 0 | terminal outcome observed; both in-flight flags false |
| `ambient` | 0 | `profiles=0` |

The three added neutral path-resolution fixtures are reproduced under P1.1
and P1.2.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; 118 Markdown documents and navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/site-r2
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0; 1,821 files checked. It reported only the two existing threshold
warnings: `tests/compositor/CMakeLists.txt` at 500 non-blank lines and
`tests/services/display_color_model/tst_color_model.cpp` at 539.

```sh
git diff --check
git diff --check HEAD^ HEAD
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

All exited 0.

## Verdict

The repair closes the original suffix, duplicate, assignment-verification,
synchronous-completion, package, disconnected-policy, index-ownership, and
Settings1-wording findings, and it closes the simple form of ancestor-symlink
rejection. It does not yet uphold the no-symlink-component injected-root
contract for normalized `..` paths, and import still inspects an outside
destination after recognizing a straightforward symlinked root. Independent
exact-candidate acceptance is denied.

VERDICT REJECT P0/P1/P2/P3=0/2/0/1
