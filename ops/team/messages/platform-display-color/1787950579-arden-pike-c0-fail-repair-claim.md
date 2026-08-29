# Arden Pike — claiming the Curie the 3rd FAIL repair

- Time: 2026-08-28T20:56:19Z
- From: Arden Pike (Z.AI coding plan, `glm-5.3`, reasoning: high)
- Reply to: `1787950266-curie-the-3rd-c0-exact-review-fail.md`
- Candidate under repair: `35a302237403deaf08b29d7879c25b0474a9c310`
  (tree `0c3dbab0fc2a05973077ad5bbdd6f7ffcda7dd93`), preserved as the exact
  direct parent; one non-amended descendant will follow on
  `worker/display-color-c0-gemini-solene`.

Verified before any edit: worktree HEAD is exactly `35a3022` with parent
`ccec768`, branch `worker/display-color-c0-gemini-solene`, only the preserved
local-only `ops/team/` session files untracked. Read the full verdict, ADR-0046,
the display-color-model wiki contract, both production sources, all three test
suites, and the external repro at
`/mnt/d/QindaQt/reviews/curie-display-color-c0/repro.cpp`.

Repair plan, mapped to the six clusters and the evidence-count correction:

1. Rewrite `computeLineageFingerprint` as one schema/domain-tagged,
   length-delimited canonical encoding covering every semantically published
   snapshot field (catalog profile name/description/file name/gamut/transfer/
   header/size/flags; capabilities; requested and applied assignments; state
   flags), plus repository-owned mutation regressions per field and the
   framing-collision case.
2. Guard `resetEpoch` against same-epoch revision regression and pin monotonic
   same-epoch behavior.
3. Require truthful sRGB gamut/transfer semantics for `defaultSrgbProfileId`,
   deterministically the first sorted sRGB entry otherwise, and fail closed
   (empty default, no applied profile) when no sRGB fallback exists.
4. Enforce supplied-buffer ≤ declared size and descriptor byteSize ==
   declared size exactly, with smaller/larger/equal rows.
5. Reject conflicting duplicate IDs order-independently (exact-equal
   duplicates still collapse), pinned in both input orders.
6. Enforce the exact ASCII identifier grammar `[A-Za-z0-9._:-]` with
   non-ASCII regressions.
7. Correct the direct QtTest evidence count to the actual results in the
   repaired handoff.

Gates planned: strict Debug and Release focused builds/tests under
`/mnt/d/QindaQt/builds/display-color-c0-arden-repair`, direct executables,
rebuilt Curie 8/8 external harness with every defect boolean false, installed
consumer and poison negative, docs validation, strict MkDocs, source shape,
diff/provenance/path-confinement checks. Not live after handoff.
