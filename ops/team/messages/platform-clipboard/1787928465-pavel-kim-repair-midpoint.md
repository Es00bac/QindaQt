# Platform clipboard: repair midpoint — all P1/P2/P3 repairs implemented

- **Timestamp:** 2026-08-28T14:47:45Z
- **Worker:** Pavel Kim, Clipboard C0 service implementer
- **Claim:** [1787927499-pavel-kim-repair-claim.md](1787927499-pavel-kim-repair-claim.md)

## Status

All five P1 blockers, all five P2 findings, and all three P3s are repaired in
the working tree (not yet committed). Commit and handoff follow after final
self-review.

## Material findings during repair

- **Wire/instance consistency catch:** my first descriptor validator draft
  required a truncated preview to be exactly `kMaxPreviewCodeUnits` wide.
  That is wrong for instances with narrowed preview limits (their clamp
  width is instance-relative and unknowable to a peer), so the check would
  have broken legitimate round-trips. Replaced with the protocol-checkable
  invariant only: a truncated flag with an empty preview is impossible and
  refused; the exact clamp width is explicitly documented as not
  wire-enforced. Wiki states this honestly.
- **Order-dependence trap in my own new tests:** encode checks canonicality
  before duplicates, so a `"TEXT/PLAIN"` duplicate correctly refuses as
  `MediaTypeRejected`; the duplicate rejection test now uses two canonical
  spellings.
- **Test growth:** the history suite reached 728 non-blank lines, so it was
  split behaviorally into `tst_clipboard_history.cpp` (admission/eviction/
  dedup semantics) and `tst_clipboard_history_lineage.cpp` (stale
  generation, exhaustion, sanitization, purge/authority, gated search);
  shared helpers moved to the support header. Four suites total now.
- **Search fixture lesson:** the PNG-like fixture has an empty preview (no
  text/plain), so metadata search hits it by source label, not preview —
  assertions pinned accordingly.

## Evidence so far (static/docs only — no compile, per serialized lane)

- `git diff --check HEAD` clean; `tools/docs_validation.py` 65 docs clean;
  `tools/check-source-shape` exit 0; `mkdocs build --strict` clean (ADR
  0031 INFO matches all other ADRs).
- Full self-review of every changed file done in lieu of compilation;
  compiler and ctest gates remain deliberately not-run while Victor owns
  the serialized compiler lane.
