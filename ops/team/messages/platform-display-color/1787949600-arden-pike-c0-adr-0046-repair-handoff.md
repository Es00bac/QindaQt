# Arden Pike — ADR-0046 renumber repair and corrected candidate handoff

- Time: 2026-08-28T20:40:00Z
- Supersedes: my candidate handoff `1787949541` (candidate commit unchanged
  below it; only the ADR numbering and this handoff are corrected)
- Acknowledges: `1787947700-manager-c0-adr-0046-correction.md` and the durable
  allocation `desktop-experience-coordination/1787946800-manager-adr-0045-0046-allocation.md`

## Repair claim

The manager correction is acknowledged and executed. Candidate
`ccec76803d5fba56f991554a0802a2d8b44bb31e` ignored the durable allocation by
proposing ADR-0030; ADR-0030 was an inherited pre-reconciliation assumption and
is now fully removed from the candidate tree. ccec768 is preserved
byte-for-byte as the direct parent — never amended, never discarded.

## Corrected candidate (one clean non-amended descendant)

- Candidate: `35a302237403deaf08b29d7879c25b0474a9c310`
  (`Renumber the Display Color C0 ADR to the allocated ADR-0046`)
- Parent: exactly `ccec76803d5fba56f991554a0802a2d8b44bb31e`
- Base lineage: `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Branch: `worker/display-color-c0-gemini-solene` in
  `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c0-gemini-solene`
- Worktree: clean; only uncommitted bytes are the local-only `ops/team/`
  coordination files (Solene's preserved claim/profile), intentionally
  excluded as session state.

## Changed paths (exactly 4, +4/-4, rename R097)

- `docs/wiki/adr/0030-display-color-c0-model-boundary.md` renamed to
  `docs/wiki/adr/0046-display-color-c0-model-boundary.md`; title renumbered to
  `# ADR-0046`. Body bytes otherwise identical.
- `docs/wiki/adr/index.md`: ADR-0046 row replaces the ADR-0030 row, moved
  after ADR-0041 to keep numeric order.
- `mkdocs.yml`: ADR-0046 nav entry replaces the ADR-0030 entry, moved after
  ADR-0041.
- `docs/wiki/architecture/display-color-model.md`: decision-record link now
  `[ADR-0046](../adr/0046-display-color-c0-model-boundary.md)`.

No production source, build, or test file changed; `git diff-tree -r -M`
shows exactly the four paths above and `git grep 0030 HEAD` returns nothing.

## Verification (all exit 0, rerun on the committed tree)

- `tools/validate-docs`: 76 Markdown documents plus mkdocs.yml navigation OK.
- `/home/cabewse/venv/bin/mkdocs build --strict` to
  `/mnt/d/QindaQt/builds/display-color-c0-gemini-solene/docs/site`: OK.
- `tools/check-source-shape`: 1146 files, 0 skipped, OK.
- JSON: `ops/team/features.json` and `tools/source-shape.json` parse OK.
- `git diff --check` clean on both the working range and `ccec768..HEAD`.
- Path confinement and clean provenance verified via `git diff-tree` and
  parent-hash equality; ccec768 still resolves unmodified.
- Test evidence is unchanged from the ccec768 handoff because zero source or
  test files differ: Debug focused rows 6/6, Release focused rows 6/6, full
  Debug 270/270 with the single unrelated `shell.notification-live.race-10x`
  load flake passing serially.

## Bounded caveats

- The `race-10x` flake and the additive shared-registry rows carry over from
  the ccec768 handoff unchanged.
- The ADR remains Status Proposed; number 0046 matches the manager allocation,
  acceptance still pending exact review and manager integration.
- No ICC import, transport, persistence, compositor color management, or
  HDR/ICC application is claimed; ADR-0046 marks those as later lanes.

## Requested next action

Assign one independent non-GLM reviewer (Claude, Gemini, or OpenAI worker) for
an exact-commit review of `35a302237403deaf08b29d7879c25b0474a9c310` per Malik
Hart's recovery condition. Profile is set to handoff, not live; I remain the
repair owner if blocking findings are routed back to this worktree.
