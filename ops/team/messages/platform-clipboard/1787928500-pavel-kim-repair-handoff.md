# Platform clipboard: repair handoff at exact commit `fa65d41` — rereview requested

- **Timestamp:** 2026-08-28T14:48:28Z
- **Worker:** Pavel Kim, Clipboard C0 service implementer
- **Exact repaired commit:** `fa65d41567ae3caff85212e62a518555ca33427a`
- **Exact tree:** `61735995574a2fcba8cc6610e9e9ee73e68a5013`
- **Exact parent (preserved reviewed candidate):**
  `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20` — non-amended, single-parent
  descendant; base lineage `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- **Branch / worktree:** `worker/clipboard-c0` at
  `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0`; `git status`
  clean after commit
- **FAIL verdict repaired:** [1787927125-hopper-the-2nd-exact-review-fail.md](1787927125-hopper-the-2nd-exact-review-fail.md)
- **Repair claim:** [1787927499-pavel-kim-repair-claim.md](1787927499-pavel-kim-repair-claim.md)
- **Repair midpoint:** [1787928465-pavel-kim-repair-midpoint.md](1787928465-pavel-kim-repair-midpoint.md)

## Requested next action

**Hopper the 2nd**: please detach at `fa65d41567ae3caff85212e62a518555ca33427a`
and run the exact rereview against the FAIL findings. Manager: after
acceptance, integrate with the ADR-0031 identity preserved (manager-reserved
number, no renumbering needed).

## P1 disposition (all repaired)

1. **CapacityRefused atomicity** — `admit()` now precomputes victims on
   shadow `entriesAfter`/`bytesAfter` state and removes them only after the
   fit is proven; refusal cannot partially evict. Hopper's exact
   reproduction (10-byte total, pinned 7 bytes, unpinned 2 bytes, admit 4)
   is pinned in `capacityRefusalIsFullyAtomic`, asserting entries, bytes,
   revision, generation, pin states, and the surviving "89" preview, plus a
   success-path companion.
2. **Ceilings before allocation/copy** — `sanitizeLimits`/`sanitizeCounters`
   clamp in the constructor (Release-safe; the invalid-limit Release-path is
   tested in `constructorSanitizesWidenedLimits`). Admission's measure pass
   enforces count/media/duplicates/class/size rules before any payload
   copy; `encodeValue` measures before writing. Oversized-no-copy and
   single/aggregate oversize regressions added on both admission and codec
   paths.
3. **Descriptor hostile floor** — one `validateDescriptor` used by encode
   and decode: valid identity, nonempty formats (`EmptyValue` on zero),
   canonical unique media (`DuplicateFormat`), negative claimed bytes
   (`MalformedData`), per-format and aggregate ceilings (`OversizedValue`),
   32-byte fingerprint, sanitized label/preview (control/format characters
   refused), truncated-flag-with-empty-preview refused. Hostile regressions
   added for each rule, including a decode-side aggregate-overflow blob
   patched to stay framing-perfect.
4. **Encode/decode symmetry** — `encodeValue` enforces the identical rule
   set in the identical error vocabulary; duplicate rejection (exact and
   case-alias forms) and oversized single/aggregate encode regressions
   added.
5. **ADR identity** — renamed to manager-reserved **ADR-0031** everywhere:
   `docs/wiki/adr/0031-volatile-bounded-clipboard-history.md`, title, ADR
   index row, `clipboard-service.md` and `module-boundaries.md` prose links.
   `grep 0028` over docs/src/tests returns nothing.

## P2 disposition (all repaired)

1. **Mixed-class precedence** — class truth is accumulated across all
   formats before refusing (sensitive > one-time > non-storable); all six
   two-format permutations tested in `mixedClassRefusalsAreOrderIndependent`.
2. **Revision contract** — documented as **content-only lineage** in the
   public header contract block and the wiki; authority transitions are
   observed via snapshot `historyEnabled`/`privacyAllowed` flags plus the
   generation bump. No API rename needed.
3. **Lineage exhaustion** — new `LineageExhausted` outcome; generation purge
   at the 32-bit ceiling destroys content unconditionally and latches
   exhaustion; serial exhaustion latches after the final serial is
   assigned; revision exhaustion refuses via the gate. A sanitized
   `HistoryCounters` constructor seam (AGENT-NOTE-documented, zero values
   cannot alias the exhausted state) enables boundary tests for all three
   ceilings in `counterExhaustionFailsClosed`.
4. **Searchable history** — the model gains deterministic bounded metadata
   search: case-insensitive substring over the sanitized source label and
   bounded preview only (payloads unreachable), most-recent-first, capped
   with `truncated`, `maxResults` sanitized into [1, kMaxEntries], gated by
   the full refusal order, revision-neutral. The wiki assigns the integrated
   user-facing search outcome to C1 (lock-state gating, private-bus
   surface, any payload-derived matching); the handoff explicitly does not
   credit C0 with the integrated `QQ-005.06` outcome — features.json is
   manager-owned.
5. **Test/package boundary** — testing-harness gains a "Clipboard C0 model
   proof" section: the `-R 'clipboard'` selector, four-suite coverage
   statement, static-only caveat, and the explicit deferred
   installed-header/link consumer + packaged qualification boundary assigned
   to C1 integration.

## P3 disposition (all fixed)

1. Descriptor header magic doc corrected to `QCBD`/`QCDL` (matching
   implementation and wiki).
2. Descriptor lists now report a dedicated `TooManyEntries` error for entry
   overflow (encode and decode), with a 65-entry regression.
3. The model header states the thread-confinement/synchronization contract;
   the wiki start page (`docs/wiki/index.md`) links the clipboard page.

## Manifest (22 files, +1204/-336)

- Module: `clipboard_types.h` (LineageExhausted, TooManyEntries,
  HistoryCounters, SearchOutcome, sanitize declarations),
  `clipboard_history.h` (contracts, seam, search), `clipboard_history.cpp`
  (sanitized construction, exhaustion gate, guarded purge),
  `clipboard_history_mutations.cpp` (measure-before-copy, class
  accumulation, atomic capacity, serial latch), **new**
  `clipboard_history_search.cpp`, **new** `clipboard_normalize.cpp`,
  `clipboard_codec.cpp` (measure-before-write, duplicate rejection),
  `clipboard_descriptor.cpp` (central validator, TooManyEntries),
  `clipboard_descriptor.h` (doc fix), `clipboard_media.cpp` (isValidLimits
  moved out), module `CMakeLists.txt` (+2 sources).
- Tests: `tst_clipboard_history.cpp` (split, 471 non-blank), **new**
  `tst_clipboard_history_lineage.cpp` (282 lines), `tst_clipboard_codec.cpp`
  (+104 lines), support header (shared helpers), tests `CMakeLists.txt`
  (+1 suite → four total).
- Docs: ADR rename 0028→0031, ADR index, `clipboard-service.md` (search
  section, revision/lineage contracts, codec symmetry, C1 assignments),
  `module-boundaries.md`, `testing-harness.md`, wiki `index.md`.

## Gates (exact, and honestly scoped)

Run (read-only/static/docs only, per the serialized compiler lane):

- `git diff --check HEAD` — clean (exit 0).
- `python3 tools/docs_validation.py` — 65 Markdown documents + mkdocs nav
  validated, no issues.
- `python3 tools/check-source-shape` — exit 0; largest clipboard production
  file 302 lines (mutations), test files 471/320/282 non-blank.
- `mkdocs build --strict` (mkdocs 1.6.1, isolated venv) — clean; the ADR
  not-in-nav INFO matches every existing ADR.
- Full line-by-line self-review of every changed file performed in lieu of
  compilation.

**Not run — declared honestly:** compiler builds and ctest execution are
intentionally not performed this round while Victor owns the serialized
compiler lane. The prior candidate's 1546/1546 build and 238/238 registry
run applied to `b523740`, not to this descendant. Compiler and test gates
are routed to Hopper's source rereview and the manager's serialized-lane
integration; no executable, compiled, or runtime claim is made for
`fa65d41`.

## Remaining caveats (bounded)

- Compilation of the repaired sources is unverified until the serialized
  compiler lane or integration build runs; the rereview is the intended
  source-level check.
- The `HistoryCounters` seam is a public constructor overload by design; it
  is sanitized and AGENT-NOTE-documented, but it is a wider surface than a
  fully private test hook would be.
- Installed-package and staged-consumer evidence remain deferred to C1
  integration by documented decision, not omitted silently.
