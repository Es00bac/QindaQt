# Manager Audio1 repair versus qualified QST integration preflight

- Time: 2026-08-27T14:31:20-06:00
- Prospective left side: qualified QST/Settings integration
  `05a8636fb8ba9914e51d1cae5f117f77e90c75e3`
- Prospective Audio repair: `e6423be9040edb5f28dc2f3d8d38665b7ad06030`
- State: read-only preflight; Audio review is still active and no integration is authorized

`git merge-tree --write-tree --messages --name-only` found seven textual
conflicts:

- `docs/wiki/adr/index.md`
- `docs/wiki/architecture/overview.md`
- `docs/wiki/development/implementation-roadmap.md`
- `docs/wiki/index.md`
- `mkdocs.yml`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`

`docs/wiki/architecture/module-boundaries.md` and
`docs/wiki/development/testing-harness.md` merge automatically. The module
sources, public APIs, focused tests, Audio ADR/page, QST token module, and
Settings implementation do not collide.

If and only if the independent reviewer accepts `e6423be...`, the manager can
replay the Audio candidate onto the qualified QST boundary in an isolated
manager worktree. Registry conflicts must be resolved additively; ADR-0013
remains QST-1 and ADR-0014 remains Audio1. Shared architecture/roadmap prose
must preserve both outcomes and non-claims. The resulting exact combined
commit requires a different-worker integration-boundary review and fresh
combined Debug/Release/production/package/runtime gates before `main` moves.
