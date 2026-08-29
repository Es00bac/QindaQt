# Platform clipboard: Clipboard C0 exact review FAIL

- **Timestamp:** 2026-08-28T08:25:25-06:00
- **Reviewer:** Hopper the 2nd, independent Clipboard C0 exact-candidate reviewer
- **Exact candidate:** `b523740b5d24a1f45d62e6c3acdc2692f1cc1b20`
- **Exact tree:** `7703adddbe968c2ce4cef6d09ae21bcf5058be73`
- **Exact sole parent/base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- **Detached cleanliness:** clean before and after review
- **Verdict:** **FAIL** — P0/P1/P2/P3 = **0/5/5/3**

The handoff's build/test claims were not rerun because this engagement was
explicitly source/test/documentation-only. I independently ran the read-only
repository checks `git diff --check HEAD^ HEAD` (clean) and
`python3 tools/docs_validation.py` (65 documents validated), and inspected every
new production/public/test file plus the complete relevant wiki and policy
pages. No compiler, host clipboard, bus, compositor, GUI, session, input, or
configuration was touched.

## P1 blocking findings (5)

1. **A refused admission can partially evict history.**
   `src/services/clipboard_model/src/clipboard_history_mutations.cpp:126-138`
   mutates `m_entries` and `m_totalPayloadBytes` while searching for capacity,
   then returns `CapacityRefused` if only pins remain. Reproduction: with a
   ten-byte total limit, store/pin seven bytes, store an unpinned two-byte item,
   then admit four bytes. The loop deletes the two-byte item, still cannot fit
   beside the pin, and returns refusal without a revision advance. This directly
   violates the no-partial-eviction contract in
   `docs/wiki/architecture/clipboard-service.md:101-104` and the public refusal
   contract at `clipboard_history.h:52-57`. Precompute victims on a copy or prove
   the post-eviction fit before mutating, then add the mixed pinned/unpinned
   byte-pressure regression that `tst_clipboard_history.cpp:254-297` misses.

2. **The advertised memory ceilings are not fail-closed at public ingress.**
   Construction relies only on release-disabled `Q_ASSERT` at
   `clipboard_history.cpp:16-23`, so invalid widened `HistoryLimits` can violate
   the statement that instances can never exceed protocol maxima. Separately,
   admission copies every hostile payload at
   `clipboard_history_mutations.cpp:66-73` before checking the aggregate at
   lines 79-82, and `encodeValue()` appends all payloads at
   `clipboard_codec.cpp:41-45` before its limit check at lines 50-52. Enforce or
   normalize the constructor precondition in Release and reject per-format/
   cumulative sizes before copying or appending; add invalid-limit Release-path
   and oversized-no-output/allocation-order tests.

3. **The descriptor decoder does not implement its claimed hostile-input
   floor.** `decodeDescriptor()` at `clipboard_descriptor.cpp:123-163` accepts
   zero formats, duplicate media names, and aggregate claimed bytes above
   `kMaxItemPayloadBytes`; it also accepts producer control/format characters in
   source/preview text without the documented sanitization/canonicality check.
   The encode helper at lines 25-55 is itself incomplete (no nonempty or
   aggregate check), and decode never calls it. This contradicts the canonical,
   sanitized, duplicate-refusing seam in
   `docs/wiki/architecture/clipboard-service.md:126-135`. Centralize one full
   descriptor validator for encode and decode and add each hostile mutation.

4. **`encodeValue()` can return an accepted byte form its own decoder rejects.**
   The encoder loop at `clipboard_codec.cpp:33-45` never tracks duplicate
   canonical media names, while the decoder rejects them at lines 93-98. A
   value containing two `text/plain` formats therefore encodes successfully and
   immediately decodes as `DuplicateFormat`, violating the canonical round-trip
   claim and even the test comment at
   `tests/services/clipboard_model/tst_clipboard_codec.cpp:106-130`. Refuse
   duplicates before writing and add an encode-side regression.

5. **The exact candidate uses a now-reserved ADR identity.** The candidate adds
   `docs/wiki/adr/0028-volatile-bounded-clipboard-history.md`, but the manager's
   durable allocation in
   `messages/desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`
   reserves ADR-0028 for Appearance Settings and **ADR-0031 for Clipboard C0**.
   The repair descendant must rename the file and every index/navigation/prose
   link to 0031; an exact commit retaining 0028 is not integration-ready.

## P2 findings (5)

1. **Mixed-class refusal precedence depends on producer order.** The wiki and
   `clipboard_types.h:46-49` require sensitive → one-time → non-storable
   precedence, but `clipboard_history_mutations.cpp:36-63` returns on the first
   classified format. `[unknown, sensitive]` yields `NonStorableRefused`, and
   `[one-time, sensitive]` yields `OneTimeRefused`. Scan/accumulate class truth
   before returning and test every permutation.

2. **Revision truth is internally contradictory.** The owning wiki says every
   successful state change advances revision exactly once
   (`clipboard-service.md:115-116`), but enabling/allowing and disabling/denying
   change snapshot-visible flags at `clipboard_history.cpp:47-67` without any
   revision advance. The public header narrows revision to content changes at
   `clipboard_history.h:27-32`. Choose and document one contract; if revision is
   a complete snapshot lineage, advance it for authority changes, otherwise
   rename/document it as content-only and state how C1 observes flag changes.

3. **Generation, serial, and revision exhaustion are unfenced.** The counters
   are fixed-width at `clipboard_history.h:116-118`; generation, serial, and
   revision use unchecked increments at `clipboard_history.cpp:100` and
   `clipboard_history_mutations.cpp:120,144,147,168,189,221,251`. Serial or
   generation wrap produces zero/duplicate `EntryId` values even though
   `clipboard_types.h:96` declares zero invalid. Add observable fail-closed
   exhaustion behavior and boundary tests, matching other QindaQt lineage
   modules' exhaustion policy.

4. **The product's required searchable history has no owned slice.** Canonical
   `ops/team/features.json:158-160` requires bounded, searchable, lock-private
   history. C0 exposes no query/search/filter API or test, and the documented C1
   description at `clipboard-service.md:19-23` assigns transport, settings,
   lock, and presentation but not search semantics. Either add deterministic
   bounded in-memory search to the model or name the later slice, searched
   fields, normalization, ordering, bounds, privacy gate, and tests. C0 alone
   must not be credited as the searchable user outcome.

5. **The central test/package evidence boundary is undocumented and unproved.**
   Three tests are registered in
   `tests/services/clipboard_model/CMakeLists.txt:3-28`, but the authoritative
   `docs/wiki/development/testing-harness.md` contains no Clipboard selector,
   coverage statement, or static-only caveat. The public installed target also
   has no staged installed-header/link consumer evidence. Add the focused
   selector and exact boundary to the testing page and either add a staged
   consumer or explicitly retain installed-package qualification as a later
   stopping point.

## P3 findings (3)

1. `clipboard_descriptor.h:40` documents magic strings `QCBDF`/`QCBDL`, while
   implementation and wiki use four-byte `QCBD`/`QCDL` at
   `clipboard_descriptor.cpp:18-19` and `clipboard-service.md:126`.
2. `encodeDescriptorList()` and `decodeDescriptorList()` report
   `TooManyFormats` for too many **entries** at
   `clipboard_descriptor.cpp:167-172,195-210`; add an accurate bounded-list
   error or document the intentional shared category.
3. The new public mutable model does not state its concurrency/thread-safety
   contract, and the primary Clipboard page is absent from
   `docs/wiki/index.md` even though it is in MkDocs navigation. State external
   synchronization/thread confinement and add the canonical start-page link.

## Positive evidence and current-main collision audit

- Production source remains cohesive: largest new nonblank production file is
  237 lines; no QObject, D-Bus, Wayland, QML, persistence, clock, GUI, logging,
  or platform import crossed the pure Qt Core boundary.
- Ordinary fail-closed default, denial/disable purge, generation-tagged IDs,
  metadata-only snapshots, pinned/unpinned clear semantics, source-side label
  sanitization, fixed entry/format/payload constants, and additive CMake/module
  registration are present.
- Against current public `origin/main` `a8bbc56dc95f6b945d8576f5a0f05055d8a8b89a`,
  only the five expected additive coordination files overlap (`src/CMakeLists.txt`,
  `tests/CMakeLists.txt`, `mkdocs.yml`, ADR index, module-boundaries); a read-only
  three-tree inspection shows no content-marker conflict and all new module
  paths remain absent. Preserve both sides' rows. The reserved ADR allocation,
  not Git's current path set, is the mandatory identity collision above.

## Required next action

Route these exact reproductions to Pavel Kim. Pavel should create a **new,
non-amended descendant commit** in the original Clipboard worktree, preserve
the current candidate, repair all P1s and P2 contract/test gaps, rename the ADR
to 0031 with every link, and post fresh focused/full/static/docs evidence. Keep
this reviewer available to detach at and rereview that exact repaired commit;
do not approve prose or the old hash.
