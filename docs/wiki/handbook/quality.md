# Quality, documentation, and contribution

QindaQt qualifies observable outcomes, including their failure modes. A source
file, worker assignment, or successful compiler exit cannot establish that a
feature works. The outcome ledger is `ops/team/features.json`; activity records
are deliberately separate.

## Maturity vocabulary

| State | Meaning within the declared outcome |
| --- | --- |
| ABSENT / UNVERIFIED | No accepted integrated implementation evidence. |
| MODELLED | Bounded model/contract with deterministic evidence. |
| WIRED | Production participants composed; accepted end-to-end execution still missing. |
| EXECUTABLE | End-to-end behavior runs within its stated boundary, with named gaps. |
| QUALIFIED | Complete declared scope and required failure, accessibility, persistence, matrix, and documentation evidence accepted. |

These values have weighted scoring rules in [team-board policy](../contributing/team-board.md).
A qualified narrow step does not qualify an entire platform or release. This
handbook preserves ledger labels and caveats without recalculating progress from
activity or smoothing over mixed maturity.

## Verification layers

Pure mutations need invariants and round trips. Schemas need malformed input and
migration coverage. Services need permissions, owner loss, restart, timeout, and
version mismatch coverage. UI needs keyboard, focus, accessibility, theme, and
resolution checks. Compositor/display work needs isolated nested execution and
appropriate physical qualification. Installed-consumer checks verify public
interfaces and package closure rather than accidentally using build-tree files.

The declared display matrix includes 1080p, WUXGA, 1440p, representative
100/125/150% scale, light/dusk/dark themes, and multi-output arrangements.
The [harness](../development/testing-harness.md) identifies which rows prove which
behavior; unsupported scenarios and physical coverage remain explicit. Preview
screenshots and offscreen controller tests cannot substitute for live input,
lock transitions, or actual service convergence.

## Documentation gates

```sh
mkdocs build --strict
./tools/validate-docs
ctest --test-dir build/dev -R 'docs|links' --output-on-failure --no-tests=error
```

A missing tool or build tree must be reported as unavailable. New wiki pages
belong in `mkdocs.yml`. Put facts on their owner page, link from consumers, and
update this handbook's relevant explanations/catalogs alongside the canonical
page. Durable architectural choices need an ADR; ordinary documentation
organization does not change a runtime or persistence contract.

## Working on the project

Read root `AGENTS.md`, the wiki index, module owner pages, and accepted decisions
before editing. Preserve unrelated work. Keep modules cohesive and dependencies
visible; review decomposition at 500 nonblank production lines and obtain a prior
ADR before introducing a file above 600. Mark comments `AGENT-NOTE`,
`AGENT-GUARD`, or `AGENT-CONTRACT` when recording non-obvious rationale,
invariants, and shared requirements for future maintainers.

The file-based delivery loop gives implementers isolated worktrees, exact base
commits, owned paths, and executable acceptance evidence. A different worker
reviews the exact candidate; repairs are re-reviewed; the manager integrates and
reruns relevant gates before advancing product state. Worker records and
new timestamped messages must truthfully reflect liveness and progress.
Do not infer a live process from a recent assignment.

See [coding practices](../development/coding-practices.md),
[documentation policy](../contributing/documentation-policy.md), and
[flow-team workflow](../contributing/flow-team-workflow.md). The [documentation
catalog](catalog/reading.md) indexes all decisions; [index](index.md) returns
to the handbook.
