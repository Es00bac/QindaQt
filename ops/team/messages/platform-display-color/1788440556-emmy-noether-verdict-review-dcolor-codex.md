# Emmy Noether — independent Display Color C1 exact-candidate review

- Persona: **Emmy Noether**, independent platform reviewer
- Provider/model: OpenAI Codex `gpt-5.6-sol`, reasoning high
- Exact candidate SHA: `a70d1d46f8bdbb0e219e7c3cb9f30ea74d93121b`
- Tree SHA: `e79a5ba4ee96867fc0f6a3d62c638fccdde00d62`
- Parent SHA: `f7c62a73d1c906f4eb1f15dc55678841281b3e79`
- Base SHA: `a4ce1f98885d3400f9a9e4c81230ddec8249a9a6`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c1-codex-review`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex`
- Review result: **REJECT**

The worktree was clean and detached at the exact candidate before and after review. Scratch sources and generated fixtures are confined to the assigned build root under `repros/`. The primary Stephanie Shirley handoff was read. The separately named resume evidence file, `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/display-color-c1-resume/last-message.md`, does not exist; no claims were inferred from it.

## Findings ledger

### P0

None.

### P1

#### P1.1 — Discovery follows a symlinked root ancestor and reads a profile outside the injected directory boundary

The authority contract says scanning never follows symlinks (`docs/wiki/architecture/display-color-model.md:168-176`). `ProfileDiscovery::discoverCatalog()` checks only `QFileInfo(root.path).isSymLink()` at `src/services/display_color_discovery/src/profile_discovery.cpp:203-223`; it does not reject a symlink in an ancestor component. A path such as `injected/redirect/icc`, where `redirect` points to an outside directory, therefore passes and is read.

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros symlink-parent /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Result: exit 1; observed `profiles=1` and a `source=.../injected/redirect/icc/outside.icc`. Expected: no profile read through a symlinked root ancestor and a bounded diagnostic. This violates the injected-root/no-symlink authority boundary.

#### P1.2 — Import reports success for a filename discovery can never enumerate

Import validates the source basename only with the generic identifier/file-name checks (`src/services/display_color_discovery/src/profile_import.cpp:123-129`) and writes that unchanged name (`:178-189`). It does not require `.icc` or `.icm`. Discovery filters all other suffixes at `src/services/display_color_discovery/src/profile_discovery.cpp:171-175,226-229`. This makes a successful import disappear immediately, contradicting the documented import/discovery round trip (`docs/wiki/development/testing-harness.md:1751-1757`).

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros suffix /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Result: exit 1; observed `import_status=0` (`Imported`) and `discovered_profiles=0` for a valid ICC file named `valid-profile.txt`. Expected: reject the unsafe/non-discoverable destination name, or ensure every successful import is discoverable.

#### P1.3 — Same-ID files with different content can be collapsed as exact duplicates

Discovery deliberately leaves `checksumSha256` empty and retains only bounded header/description metadata (`src/services/display_color_discovery/src/profile_discovery.cpp:139-149`). Duplicate handling compares only those descriptors (`:247-276`). Two profiles with identical inspected metadata but different bytes elsewhere therefore compare equal and emit `duplicate-collapsed`, despite the normative rule that the same stem with different content must drop both entries (`docs/wiki/architecture/display-color-model.md:194-197`; ADR-0057 lines 69-71).

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros duplicate /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Result: exit 1; two `same.icc` files differ in one uninspected body byte, but observed `profiles=1 duplicate-collapsed=1 conflict=0`. Expected: `profiles=0` with `conflicting-profile-id`.

#### P1.4 — Assignment apply accepts a different authoritative document as the submitted write

`applyDraft()` computes the intended merged document but does not retain it (`src/services/display_color_assignment/src/assignment_store.cpp:132-149`). `handleCommitFinished()` decodes any well-formed authoritative value and reports `Applied` solely from the Settings wire status (`:165-178`). This contradicts `ApplyStatus::Applied`'s public meaning (“decoded back to the expected document truth,” `assignment_store.h:41-48`) and the owning page's claim of verification against the authoritative current value (`display-color-model.md:246-250`).

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros mismatch
```

Result: exit 1; the draft submits profile `wanted`, the authoritative applied reply contains `different`, and the store reports `apply_status=0` (`Applied`) with `persisted=different`. Expected: `Uncertain`/failed verification, never `Applied` for a value different from the submitted document.

### P2

#### P2.1 — Invalid discovery origins silently become `BuiltIn`

`assembleDescriptor()` switches over `DiscoveryOrigin` without a default/rejection (`src/services/display_color_discovery/src/icc_text_metadata.cpp:252-272`). `IccProfileDescriptor` defaults to `ProfileOrigin::BuiltIn` (`src/services/display_color_model/include/qindaqt/services/display_color_model/color_types.h:92-103`), so an out-of-range public enum value becomes trusted built-in provenance instead of failing closed.

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros invalid-origin /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Result: exit 1; observed `profiles=1 origin=0` (`BuiltIn`) for `static_cast<DiscoveryOrigin>(99)`. Expected: reject invalid injected configuration with an `invalid-origin` diagnostic and publish no profile.

#### P2.2 — A synchronous legal completion strands the assignment store in-flight

`SettingsAssignmentStore::applyDraft()` sets `m_applyInFlight` only after calling `SettingsClient::setUserValue()` (`src/services/display_color_assignment/src/assignment_store.cpp:142-149`). If the injected transport emits a valid completion synchronously, `handleCommitFinished()` sees no pending store apply and ignores it (`:152-159`); `applyDraft()` then sets the flag forever. The production Qt transport is deferred, which is a bounded workaround, but the public injected seam does not make store correctness safe against direct signal completion.

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros synchronous
```

Result: exit 1; observed `accepted=1 finished=0 store_write_in_flight=1 client_write_in_flight=0`. Expected: exactly one terminal `applyFinished` and both in-flight flags false.

#### P2.3 — The new installed-consumer rows cannot run after the mandated focused build

Both new scripts run an unscoped whole-tree `cmake --install` (`tests/services/display_color_discovery/run_installed_cpp_consumer.cmake:25-38`; `tests/services/display_color_assignment/run_installed_cpp_consumer.cmake:27-40`). Under the required review recipe, only display-color libraries and focused tests are built, so installation fails on unrelated unbuilt libraries. The handoff itself acknowledges that its 16/16 evidence depended on a complete build. A full repository build is a bounded workaround, but it is prohibited by the exact review brief and makes the documented focused selector non-self-contained.

Reproduction (after the focused target build recorded below):

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev \
  -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error
```

Result: exit 8, 13/16 passed. The C0, discovery, and assignment installed-consumer rows all failed because `src/profiles/libqindaqt_profiles.a` was not built. Release produced the same exit 8 and 13/16 result. Expected: 16/16 after building only candidate/focused targets; at minimum the two new C1 package rows must stage only their transitive artifacts or own an explicit focused preparation target.

#### P2.4 — Disconnected-output persistence policy is implemented but not specified or tested

The review contract requires a persisted assignment for an output that no longer exists to be kept or dropped exactly as the owning page says. Neither `docs/wiki/architecture/display-color-model.md` nor ADR-0057 contains a disconnected/unplugged/orphan-output rule. The implementation currently keeps every untargeted record because `applyColorAssignmentDraft()` copies the whole document and mutates only draft targets (`src/services/display_color_assignment/src/assignment_document.cpp:169-217`); the tests do not identify that retained record as a disconnected-output contract.

Reproduction:

```sh
cd /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build
./candidate_repros disconnected-output
```

Result: exit 1; observed `disconnected_record_retained=1`. Expected: an explicit keep-or-drop rule in the owning page and a focused row pinning that rule. The current behavior may be reasonable, but it is not reviewable against the required normative contract.

#### P2.5 — `docs/wiki/index.md` is outside the lane's owned/additive path set

The lane grants the owning display-color page and one new ADR, plus enumerated additive registries. `docs/wiki/index.md` is neither owned nor listed as an additive shared edit, yet the candidate rewrites its Display Color summary.

Reproduction:

```sh
git diff --name-only a4ce1f9..a70d1d4 | sort
sed -n '/## Owned paths/,/## Verification/p' \
  /home/cabewse/work_SPaC3/builds/qindaqt/lanes/display-color-c1/lane.md
```

Result: the diff contains `docs/wiki/index.md`; the lane's owned/additive lists do not. Expected: no product path outside those lists, or explicit manager coordination adding that shared path before the immutable candidate is cut.

### P3

#### P3.1 — Settings1 reference overstates generic schema validation

`docs/wiki/reference/settings1-v1.md:127-128` says generic object validation rejects any malformed assignment record before persistence. The schema declares only `type: object`; `src/settings/src/settings_value_normalizer.cpp:100-107` accepts any canonical `QVariantMap` and has no assignment-record grammar. The assignment store correctly rejects malformed confirmed values later. The sentence should distinguish generic JSON/resource validation from the stricter consumer-side document validation.

## Review-question evidence

1. **Import/discovery validation:** Existing focused tests pass for empty, truncated, declared-size mismatch/oversize, final-component symlinks, tag bounds, import rejection, interrupted temporary recovery, and deterministic ordering. The ambient-root scratch control passed with `HOME` and `XDG_DATA_HOME` redirected to canary directories: `observed profiles=0`, exit 0. P1.1, P1.2, P1.3, and P2.1 show the remaining authority, round-trip, conflict, and enum-validation failures.
2. **Persistent assignment:** The codec rejects malformed and wrong-typed documents wholesale; the focused document/store rows pass, including conflict, timeout uncertainty, owner loss, and last-confirmed-document retention. P1.4 breaks authoritative-value verification; P2.2 breaks a completion ordering; P2.4 records the absent-output policy gap. The C0 revisioned model was not modified.
3. **Boundaries:** Source boundary and poison-negative rows pass. The new production modules do not import compositor, Wayland, colord, Qt GUI, host-display, or direct D-Bus APIs; assignment uses the public Settings client seam. No compositor application, Settings UI, or HDR/WCG runtime claim was added. P1.1 is nevertheless a filesystem-authority escape. No host bus, display, hardware, uinput, nested-compositor row, or network was used.
4. **Tests:** `ctest -N` registers 16 rows. The 13 non-package rows pass under the isolated bus environment and `QT_FATAL_WARNINGS=1` in Debug and Release. The full prescribed selector fails as P2.3. Existing positive tests do not contain the negative controls reproduced in P1.1-P1.4/P2.1-P2.2. There are no Qt GUI/offscreen rows in these C1 modules.
5. **Scope/shape/docs:** ADR-0057 is present in the ADR index and `mkdocs.yml`; the owning page keeps compositor application, Settings UI, colord, HDR/WCG runtime, and hardware qualification excluded. Static gates pass. P2.5 is the one path-ownership exception.

## Commands and results

### Identity and cleanliness

```sh
pwd
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-parse a4ce1f9
git status --porcelain
```

Exit 0. Values match the header; status output was empty before and after review.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Exit 0. CMake emitted the repository's dependency-prefix runtime-path warnings and generated the Ninja tree.

The same command with `-B .../release -DCMAKE_BUILD_TYPE=Release` exited 0 with the same class of configure warnings.

### Focused build

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev --parallel 3 --target \
  qindaqt_display_color_model qindaqt_display_color_discovery qindaqt_display_color_assignment \
  qindaqt_color_header_validation_tests qindaqt_color_catalog_tests qindaqt_color_model_tests \
  qindaqt_color_discovery_tests qindaqt_color_import_tests \
  qindaqt_color_assignment_document_tests qindaqt_color_assignment_store_tests
```

Exit 0 (`ninja: no work to do`; artifacts already current at the exact SHA). The identical Release target command exited 0 with the same result.

### Registered selectors

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev \
  -N -R '^qindaqt\.display-color-'
```

Exit 0; 16 tests registered.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev \
  -R '^qindaqt\.display-color-' --output-on-failure --no-tests=error
```

Exit 8; 13/16 passed, three installed-consumer rows failed during whole-tree install on missing unbuilt `libqindaqt_profiles.a`. Release: exit 8, 13/16 passed, same failures.

```sh
env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
  QT_FATAL_WARNINGS=1 \
  ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/dev \
  -R '^qindaqt\.display-color-' -E 'installed-cpp-consumer' \
  --output-on-failure --no-tests=error
```

Exit 0; 13/13 passed. Release: exit 0; 13/13 passed.

### Scratch negative controls

```sh
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros \
  -B /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/repros/build --parallel 3
```

Both exited 0. The scratch compiler reported two non-product warnings for intentionally ignored `SettingsClient::start()` return values; candidate targets were built separately with strict warnings.

The seven failing controls and their observations are recorded in P1.1-P1.4 and P2.1-P2.2/P2.4. The ambient canary control was:

```sh
env HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/ambient-home \
  XDG_DATA_HOME=/home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/ambient-xdg \
  ./candidate_repros ambient /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex
```

Exit 0; observed zero profiles despite valid canary profiles in ambient HOME/XDG directories.

### Static gates

```sh
./tools/validate-docs
```

Exit 0; 118 Markdown documents and navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-dcolor-codex/site
```

Exit 0.

```sh
./tools/check-source-shape
```

Exit 0; 1,820 files checked. It emitted two pre-existing threshold warnings: `tests/services/display_color_model/tst_color_model.cpp` at 539 non-blank lines and `tests/compositor/CMakeLists.txt` at 500.

```sh
git diff --check
python3 -m json.tool data/settings/schema-v2.json >/dev/null
```

Both exited 0.

## Verdict

The candidate violates injected-root authority, import/discovery round-trip truth, content-conflict handling, and assignment apply verification. It also has five bounded but blocking review defects in invalid-origin handling, completion ordering, focused package-test reachability, absent-output policy documentation, and path ownership. Independent exact-candidate acceptance is therefore denied.

VERDICT REJECT P0/P1/P2/P3=0/4/5/1
