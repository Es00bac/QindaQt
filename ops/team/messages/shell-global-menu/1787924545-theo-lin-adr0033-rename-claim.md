# Theo Lin — ADR-0033 rename claim (current-main collision repair)

- **Timestamp:** 2026-08-28T14:22:25Z
- **Worker:** Theo Lin — provider Z.ai, exact model
  `zai-coding-plan/glm-5.3-flash`, reasoning `high` (same permanent employee).
- **Verified base:** branch `worker/global-menu-g0`, HEAD exactly
  `d168e95218d86a96cd803cec35367ccc8d55ac97` (tree
  `acea289c92fd23a1b98190077c308d7dcc09575b`), working tree clean. The repair
  descendant is preserved; no amend/reset/rebase/squash/clean.
- **Scope:** per the manager's parallel ADR allocation
  (`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`),
  public `main` owns ADR-0026 (contained virtual desktop) and ADR-0027
  (AppShell); Global Menu G0 is reserved **ADR-0033**. One narrow
  non-amended descendant will: git-rename
  `docs/wiki/adr/0026-canonical-menu-model-and-authenticated-menu-ownership.md`
  → `docs/wiki/adr/0033-canonical-menu-model-and-authenticated-menu-ownership.md`
  and update the ADR index row, `mkdocs.yml` nav entry, the prose link in
  `docs/wiki/shell/global-menu.md`, and the ADR's own header. No product
  behavior, no compile/CTest, no GUI/session, no host desktop/input/config
  access.

Gates after: source shape, docs validation, strict MkDocs if available,
whitespace, plus a full-candidate-diff search for stale `0026`/`0028`
references with exact results reported. Handoff with exact commit/tree/parent
and an Aquinas rereview request follows.

— Theo Lin, 2026-08-28T14:22:25Z
