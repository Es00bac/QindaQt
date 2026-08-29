# Nadia Park — losing-lease repair material midpoint

- **Time:** 2026-08-28T11:40:57-06:00
- **Status:** working
- **Exact base:** `e53a3505ec50a0819bbf0ccd4204d2926fe657fd`

## Material finding and repair

The transaction repository already retains an immutable committed profile
independently of its published provisional snapshot through
`LayoutEditingCoordinator::committedProfile()`. The P1 was confined to the
editor seam: it exposed no authoritative readiness/lease result, and the
session inferred permission from `hasPreview() == false` even when the adapter
had lost the lease.

The live candidate now:

- distinguishes unique-coordinator readiness from snapshot readability;
- exposes the coordinator-retained committed profile only after readiness;
- rejects mutation and Apply with typed `EngineUnavailable` before any write
  when another coordinator owns the repository; and
- keeps a foreign-preview-born session fail-dirty/read-only, then adopts the
  retained committed profile before its first successful post-release edit.

The two requested production-composition regressions are in the dedicated
dirty-state suite. They prove no file is created by Apply under a foreign
lease, and foreign BeginPreview → construct losing session → foreign Cancel
→ release → edit → Undo restores exact content and clean dirty truth.

## Evidence so far

- Strict incremental compile of the real editor/session/dirty targets: exit
  `0`.
- `qindaqt.customize-editor-session` and
  `qindaqt.customize-editor-dirty-state`: **2/2 CTest targets passed**, exit
  `0`; the dirty suite contains all four production lifecycles.
- `./tools/check-source-shape`: **1,032 files passed**, exit `0`.
- `git diff --check`: exit `0`.

I read Elion's exact verdict and the latest Shell thread updates. Other live
Shell lanes are disjoint; no collision or blocker is open. Concrete request:
Elion Brooks remains the retained exact rereviewer for the clean descendant.
I am running the complete strict serial focused/adjacent, docs/link/MkDocs,
provenance, current-main collision, and clean-tree gates now.

— Nadia Park, live repair midpoint.
