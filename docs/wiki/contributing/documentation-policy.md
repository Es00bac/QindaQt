# Documentation maintenance policy

QindaQt treats documentation as versioned source. Wiki content lives under
`docs/wiki`, navigation is declared in `mkdocs.yml`, and accepted architecture
decisions live under `docs/wiki/adr`. This policy is accepted in
[ADR-0003](../adr/0003-docs-as-code.md).

## Same-change rule

Update documentation in the same change whenever code changes:

- module ownership, dependencies, processes, or trust boundaries;
- public APIs, IPC, schemas, manifests, settings, or persisted data;
- visible interaction behavior, defaults, shortcuts, accessibility, or failure
  handling;
- development commands, supported environments, scenario matrices, or
  performance gates; or
- an accepted ADR's assumptions or consequences.

Small refactors that preserve every documented contract need no prose churn.
Comments cannot substitute for a wiki update when a fact matters outside one
local implementation.

## Page rules

- Put a fact on the page that owns the concept and link to it from consumers.
  Avoid copied descriptions that can drift.
- Describe current truth and clearly label planned behavior. Remove stale
  transitional text when implementation lands.
- Prefer short sections, tables for stable mappings, and relative `.md` links.
  Link symbols or paths only when they clarify a non-obvious ownership boundary.
- Update `mkdocs.yml` when adding, moving, or removing a page. Repair all inbound
  links in the same patch.
- Use original wording and assets. Cite externally derived protocols or design
  constraints in the page that relies on them.

Before handoff, run:

```sh
mkdocs build --strict
ctest --test-dir build/dev -R 'docs|links'
```

If a command is unavailable, report that explicitly and still perform a local
link/path inspection.

## ADR workflow

Create an ADR before implementing a durable, cross-cutting decision: compositor
or framework selection, new mandatory dependency, process/trust boundary,
extension model, persistence contract, compatibility break, or reversal of an
accepted choice.

1. Copy [the template](../adr/template.md) to the next zero-padded number.
2. Set status to **Proposed** while discussion or feasibility work remains.
3. Set status to **Accepted** only when the project has committed to the choice.
4. Do not rewrite an accepted decision to hide history. Add a new ADR with
   **Supersedes**, and mark the old record **Superseded by ADR-NNNN**.
5. Update the [ADR index](../adr/index.md), `mkdocs.yml`, affected architecture
   pages, tests, and agent contracts in the same change.

An ADR records why and consequences, not a file-by-file implementation plan.
