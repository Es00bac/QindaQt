# Theo Lin — ADR-0033 rename handoff for exact rereview

- **Timestamp:** 2026-08-28T14:23:34Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Exact descendant commit:** `79e7333de250cc7e3e4aa15df3c084789539f16f`
  (single, non-amended; branch `worker/global-menu-g0`).
- **Tree:** `0161a2ec0ec40ca26a0bd9c6b370aa87268b1b2b`.
- **Parent:** `d168e95218d86a96cd803cec35367ccc8d55ac97` — the reviewed
  repair commit, preserved untouched beneath it; ancestry remains
  `d168e95` → `782792e` → `9db68c4023257b49421101fa1b13c73bbc2cfa85`. No
  amend/reset/rebase/squash/clean was performed; the worktree is clean.

## What changed (4 files, +13/−4, one git rename)

Per the manager's reservation
(`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`):

- `docs/wiki/adr/0026-canonical-menu-model-and-authenticated-menu-ownership.md`
  → `docs/wiki/adr/0033-canonical-menu-model-and-authenticated-menu-ownership.md`
  (git rename; 90% similarity). Header retitled ADR-0033 and given a short
  provenance note recording the reservation; status stays Proposed and all
  content is otherwise unchanged.
- `docs/wiki/adr/index.md`: row link and label now ADR-0033.
- `mkdocs.yml`: nav entry `"ADR-0033: Canonical menu model"` with the new
  path.
- `docs/wiki/shell/global-menu.md`: prose link now
  `[ADR-0033](../adr/0033-canonical-menu-model-and-authenticated-menu-ownership.md)`.

No product behavior change; no compile, CTest, GUI, session, or host
desktop/input/config access.

## Stale 0026/0028 reference search — exact results

- `git grep -n "0026\|0028"` over the full worktree: exactly two hits, both
  inside the renamed ADR's provenance note ("authored as ADR-0026 on an
  older parallel base. Public `main` subsequently took 0026 … and 0027 …").
  These are intentional historical references, not stale links.
- `git diff 9db68c4` (complete candidate diff) filtered for `0026|0028`:
  only those same two added note lines. No `0028` reference exists
  anywhere in the candidate.
- `git grep "0026-canonical\|ADR-0026"` excluding the 0033 file: zero
  matches — no stale filename or label references remain.

## Gates

- `python3 tools/check-source-shape` — PASS (exit 0, 1048 files).
- `python3 tools/validate-docs` — PASS (65 Markdown docs + mkdocs nav,
  exit 0).
- `git diff --check` — PASS (exit 0).
- `mkdocs build --strict` — unavailable on PATH (reported; the
  dependency-free `tools/validate-docs` covers nav/links).

## Requested next action

Aquinas: please rereview exactly `79e7333de250cc7e3e4aa15df3c084789539f16f`
— this narrow rename descendant, then the still-pending full rereview of
`d168e95218d86a96cd803cec35367ccc8d55ac97` against the FAIL verdict.
Manager: compiler/ctest lanes remain with Victor; ADR-0033 stays Proposed
until integrated. Material-collision note: if public main advances into the
0033 reservation, I will report immediately per the allocation policy.

— Theo Lin, 2026-08-28T14:23:34Z
