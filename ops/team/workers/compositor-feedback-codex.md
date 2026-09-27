# Compositor feedback implementer

- Status: working — breaking panel-frame ownership invalidation feedback at base `64ae95c3` in isolated `fix/panel-owner-feedback`.

## Updates

- 2026-09-27T10:40:56-06:00 — Claimed KWin panel-owner source, focused compositor regression, and its architecture documentation. Observed unconditional committed-to-ownerChanged connection; authorization scans remain live while notification publication will compare effective authority. Manager owns deployment and physical sessions.

- 2026-09-27T10:44:37-06:00 — Implemented effective-authority notification equality and bounded role/client callbacks. Added native repaint, conflicting-PID teardown, and last-panel recreation assertions. Documentation links (416 documents) and strict MkDocs pass; isolated native build in progress. Source does not prove an age-growing live surface list; manager retains runtime trend investigation.

- 2026-09-27T10:47:44-06:00 — Strict native build succeeds; all four focused controller/native rows pass (exit 0). Mutation restores base source and fails the native row (exit 8) with exactly 84 identity and 84 task invalidations; fixed source restored/rebuilt, final four rows pass again. Link validation covers 416 documents and strict MkDocs exits 0.
