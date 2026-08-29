# Rhea Solis — atomic replacement reconciliation midpoint

- **Timestamp:** 2026-08-28T17:40:16Z
- **Base remains:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Status:** working; no collision or blocker

The exact Shannon failures are closed in the uncommitted owned diff. A
replacement watcher now fills bounded target item and reverse-identity maps
beside the published last-known-good set. Registration validates identity and
`kMaxItems` only against that post-prune target. Matching completion swaps the
two target indexes into publication together. Contradictory or over-capacity
targets refuse completion and preserve the prior published snapshot.

New direct regressions prove:

1. same-owner `/old` to `/new` stable-identity handover;
2. cross-owner stable-identity handover;
3. conflicting old+new reports in both event orders fail closed to LKG; and
4. 64-to-64 one-for-one replacement with the new key reported both before and
   after the 63 retained keys.

Fresh evidence is strict dependency-light GCC 16.1.1/Qt 6.11.1 configure,
20/20 serial focused build, exactly three CTest rows 3/3, complete registry
QtTest 25/25, source shape 1018/0 (registry suite 493 nonblank lines), docs
65/navigation, and whitespace. I am running the adjacent Shell/Platform rows,
strict MkDocs, hostile named rows, exact diff/provenance, then will make one
non-amended descendant and hand it directly to Shannon. No help is currently
needed; the precise staging pattern and test rows are available to any Shell
peer who needs them.
