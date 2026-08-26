# ADR-0003: Maintain documentation as code

- **Status:** Accepted
- **Date:** 2026-08-25
- **Owners:** Project-wide
- **Supersedes:** None
- **Superseded by:** None

## Context

QindaQt spans compositor, shell, services, extensions, applications, and a large
test matrix. Future work will often be performed by agents that do not retain
the conversations behind earlier decisions. An external or manually maintained
wiki would drift from the code and would not participate in review or CI.

## Decision

Keep the canonical MkDocs wiki under `docs/wiki`, its navigation in
`mkdocs.yml`, and numbered ADRs under `docs/wiki/adr`. Behavior and architecture
changes update their owning pages in the same patch. CI treats broken links,
invalid navigation, and documentation build warnings as failures.

Accepted ADRs are immutable history except for status/supersession metadata and
corrections that do not change the decision. A reversed decision receives a new
ADR. Source comments use the agent-focused conventions in
[Coding practices](../development/coding-practices.md) and link here instead of
duplicating long rationale.

## Consequences

- Documentation changes are reviewable, versioned, branch-consistent, and
  available offline with the source.
- Contributors must maintain navigation, links, relevant pages, and ADR
  consequences as part of feature completion.
- The repository carries documentation build/link checks and concise authoring
  conventions defined by the
  [maintenance policy](../contributing/documentation-policy.md).

## Revisit when

Reconsider the renderer or publication mechanism if MkDocs becomes unmaintained
or cannot meet accessibility and search needs. Keep Markdown sources and the
same-change/ADR history requirements regardless of renderer.
