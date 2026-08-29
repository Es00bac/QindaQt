# Exact-hash settings round-trip QA complete

- **Timestamp:** 2026-08-27T11:53:06-06:00
- **Reviewer:** Codex settings round-trip QA
- **Exact candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Checkout:** detached, clean
  `/home/cabewse/work_SPaC3/container-wm-workers/settings-roundtrip-qa`
- **Verdict:** **PASS for the assigned settings round-trip scope**
- **Blocking findings:** none

## Contract result

Fresh source inspection and executable evidence agree with the Settings1,
settings-service, ADR-0012, module-boundary, and testing-harness contracts:

- Recursive Object values normalize to `Nullptr`, Boolean, signed `LongLong`,
  finite `Double`, well-formed NUL-free `QString`, `QVariantList`, and
  `QVariantMap`. Nested null in both maps and lists is preserved.
- Signed minimum/maximum and accepted unsigned inputs through `INT64_MAX`
  normalize losslessly to `LongLong`; wider unsigned input rejects before
  mutation. Integral doubles in range, including `-0.0`, normalize to signed
  integers. Denormal, maximum finite double, `0.1`, and the representable
  values around both signed-64 boundaries retain exact double bits where the
  documented domain says they remain doubles. NaN and infinities reject.
- Strings and object keys reject embedded NUL and ill-formed surrogate
  sequences. Empty object keys reject. Protocol validation also applies UTF-8
  key/string byte limits.
- Persistence uses the explicit canonical JSON encoder. The focused save/load
  test proves exact metatypes and values after disk reconstruction and proves
  a rejected wide unsigned value leaves the previous file unchanged.
- Settings1 translates only canonical `Nullptr` to the fixed
  `QDBusSignature("v")` scalar and translates that exact marker back to
  `Nullptr`. Invalid QVariant, caller-supplied signatures, other Qt types,
  wide unsigned values, non-finite numbers, and bound overflows reject.
- One shared budget enforces per-value and aggregate bytes/nodes plus depth,
  list/map entries, keys, strings, operations, requested keys, changed keys,
  and fixed reply envelopes. Real private-D-Bus lazy arrays/maps are covered;
  resident startup rejects an oversized persisted user layer before object
  registration.
- The private-bus Qt transport test commits a nested Object with map/list null,
  integer and floating edges, validates exact client QVariant metatypes,
  replaces the resident service from the same saved file, and validates the
  same exact metatypes again. Profile fallback, same-object client restart,
  owner/epoch replacement, and local daemon loss also pass.
- Direct public `QtSettingsTransport::commit` input is encoded and validated
  before `requestMap`/libdbus. Unsupported input emits the normal bounded
  `requestFailed`/`MalformedClientRequest` result and returns. The adversarial
  transport test observes this synchronously for invalid QVariant with zero
  remote commit calls; its raw canonical-null call reaches the private service
  as `QMetaType::Nullptr` without aborting. The common encoder inspected on
  that path rejects every other unsupported value class above.
- DND controller coverage proves owner loss overrides pending accepted-save
  and conflict presentation, conflict intent is restored only after a fresh
  authority baseline, and confirmed persistence/validation/revision-exhaustion
  diagnostics survive automatic refresh until a new explicit write. The
  bridge/offscreen consumer tests also pass without input synthesis.

## Modular and documentation review

The settings model, protocol codec, resident service, asynchronous client/Qt
transport, DND projection, and QML remain on their documented dependency
boundaries. The repaired wire decoder is 423 nonblank lines and is split from
141-line encoding and 132-line envelope collaborators; no changed production
file crosses the repository's 500-line decomposition-review threshold. The
same change updates the owning settings-service page, Settings1 reference,
ADR-0012, notification presentation page, and testing matrix, and all are in
MkDocs navigation.

## Independent commands and counts

- `cmake --preset dev` — exit 0.
- `cmake --build --preset dev --parallel 2` — exit 0, **1,184/1,184 build
  steps** including all production and test targets.
- `ctest --test-dir build/dev -R '^qindaqt\\.settings-' ...` — exit 0,
  **14/14 passed**: schema, layers, persistence, migration, protocol, real
  D-Bus protocol, repository, service lifecycle, client, DND controller, Qt
  transport, adversarial transport, and both settings-app tests.
- Quieting bridge/offscreen regex from the wiki — exit 0, **3/3 passed**.
- Settings-app and production-shell QML lint targets — exit 0, **2/2**.
- `tools/validate-docs` — exit 0, **42 Markdown documents** plus navigation.
- `uvx --offline --from mkdocs mkdocs build --strict ...` — exit 0; no network
  access.
- `tools/check-source-shape --warnings-as-errors --largest 20` — exit 0,
  **767 files checked**, zero skips or violations.
- Exact-commit `git diff --check`, `git show --check`, worktree/cached diff,
  hash, and status checks — exit 0; final HEAD is exactly
  `08c7156c578eaac21116498ed563828be4c1a625` and product status is
  `## HEAD (no branch)` with no tracked or untracked product changes.

This review did not run the complete 158-test registry because that would
cross the assigned no-desktop/no-input scope. It used only ignored build
output, repository fixtures, temporary files created by the tests, private
D-Bus daemons, and offscreen Qt. It did not access a live desktop, user session
bus, compositor, input device, network, or unrelated files.

**Requested next action:** the manager may treat the exact candidate as passing
this independent settings round-trip QA gate and combine it with the other
exact-hash review results before deciding integration.
