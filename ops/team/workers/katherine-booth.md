---
name: Katherine Booth
role: Controls test-fixture implementer
provider: Z.AI
model: zai-coding-plan/glm-5.3
reasoning: high
status: handoff
feature: QQ-006.02 Controls visual gate host independence (byte-pinned fonts)
worktree: /home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts
started_at: 2026-09-02T21:10:34-06:00
updated_at: 2026-09-03T02:47:00-06:00
---

# Katherine Booth

- Role: Controls test-fixture implementer for the reusable Controls visual gate (QQ-006.02).
- Provider/model: Z.AI GLM `zai-coding-plan/glm-5.3`, reasoning high (regular speed for the
  repair session; earlier sessions of this same persona ran `glm-5.3-flash`).
- Status: handoff — exact repair candidate `b7b52083ab4bb11d36284020d42a6a6e854db260` (tree
  `ff493f6df69fdae4484a9b41b7150ef4411dbc9d`, base `cf2c2a9e00513b994c077cd451b774d1cbad0a34`,
  repairing rejected `bf1c83a`): every theme-catalog family now resolves to the registered
  repository families through pinned runtime theme copies (a Qt substitution cannot redirect a
  host-installed family), a checked-in host-Noto-hidden fontconfig canary row, valid OpenType
  checksums on the regenerated vendored fonts with validation in the pinning row, a scoped
  `ControlsQmlModules` installed-consumer install, zero baseline churn, and full Debug/Release
  plus fresh-root focused-build evidence (34/34).
- Exact base: `cf2c2a9e00513b994c077cd451b774d1cbad0a34` (repair parent; original lane base
  `ce9228d9694622d503d92a38d01986f8f124f188`).
- Branch: `worker/controls-visual-fonts`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/controls-visual-fonts`.
- Product authority: `tests/controls/**`, `docs/wiki/adr/0021-isolate-controls-visual-rows.md`,
  the Controls sections of `docs/wiki/development/testing-harness.md` and
  `docs/wiki/shell/controls.md`, this record, and the
  `ops/team/messages/native-application-design/` thread.

## Updates

- 2026-09-02T21:10:34-06:00 — claimed QQ-006.02 after the host Noto package update broke all 25
  visual rows with glyph drift; began vendoring fonts and byte-pinning them in
  `pinDeterministicFonts()`.
- 2026-09-02T23:01:24-06:00 — work preserved by the manager at `74b1246` when the GLM usage
  limit hit; Erna Schneider Hoover (Kimi) continued in the same lane and worktree.
- 2026-09-03T01:32:57-06:00 — resumed a second time after the Kimi limit: found the tree clean at
  `f5b182c` with fonts renamed to `QindaQt Sans`/`QindaQt Sans Mono`, all code, CMake wiring,
  baselines, and the three doc sections already in place; verified fc-scan families, the
  fail-closed fixture loader, and the negative-fixture runner (timestamp from the resumed
  session's first build-root action).
- 2026-09-03T01:46:58-06:00 — material finding: the pinning probe parsed the sfnt `numTables`
  field with a 32-bit read at offset 4; the field is big-endian uint16. Worked only because the
  `name` record sits inside the real table directory before the oversized bound left the buffer.
  Fixed with an `AGENT-GUARD`, rebuilt both profiles, re-ran the full matrix (rebuilt-binary
  timestamp).
- 2026-09-03T01:49:29-06:00 — verification: Debug visual 25/25 twice and 33/33 full selector;
  Release visual 25/25 and 33/33 full selector; `FONTCONFIG_FILE=/dev/null` row 1/1; mutation
  test proves the pinning row fails on a host-family substitution revert; baseline review against
  `ce9228d` shows identical dimensions on all 25 rows and glyph-only drift (visual inspection of
  three per-scale composites); static gates all exit 0.
- 2026-09-03T01:52:20-06:00 — handoff: candidate `bf1c83a15206c190d0c56950c261c1ada0196281`,
  message at `ops/team/messages/native-application-design/1788421769-katherine-booth-handoff.md`;
  requesting independent exact review then manager integration.
- 2026-09-03T02:10:47-06:00 — claimed the repair of `bf1c83a` after Jean Hall's REJECT
  (0/1/2/0); read the verdict and her no-Noto fontconfig/checksum tooling.
- 2026-09-03T02:20:00-06:00 — material finding: a Qt 6.11.1 probe proves
  `QFont::insertSubstitution` does not redirect a family the host has installed (requesting
  `Noto Sans` with the substitution installed still resolves the host face), so the verdict's
  literal mechanism alone cannot fix the normal-environment render; the fixture now rewrites
  every `data/themes/*.json` family onto the registered families in pinned runtime theme copies
  and keeps the substitutions for absent families.
- 2026-09-03T02:47:00-06:00 — handoff: repair candidate
  `b7b52083ab4bb11d36284020d42a6a6e854db260`, message at
  `ops/team/messages/native-application-design/1788424756-katherine-booth-handoff.md`;
  requesting Jean Hall recheck the exact SHA, then manager integration.
