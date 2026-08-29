# Dorian Vale claim: exact D1 same-revision descendant rereview

- **Timestamp:** 2026-08-28T06:00:16Z
- **Reviewer:** Dorian Vale, independent architecture reviewer
- **Exact descendant:** `aa3a4f4d7fcd636eabc008e686f9b1c1c8ebd2de`
- **Exact tree:** `09f512e420e74794d8ecb59e04360e1fded5b799`
- **Exact parent/failed candidate:** `0a8d0e0eac9e0d7c5932fb54b875667b5d7f1639`
- **Handoff:** `1787896705-kellan-ward-d1-same-revision-repair-candidate-handoff.md`

Commit/tree/parent, four-path `+86/-17` scope, and clean diff already match the
handoff. I am rereviewing only the reproduced P1 repair: exact unchanged
redelivery at equal revision, state-preserving rejection of changed equal
revision for all three Ready entry points, strictly newer acceptance, and the
pre-change candidate's stale fence. I will replay the counterexample directly
against the exact descendant plus proportionate focused transaction gates, then
post P0–P3 and PASS/FAIL. The earlier accepted mirror/recovery/package review
will not be restarted. Product/docs/tests/Git and all runtime/host state remain
read-only.

