# Appearance current-main ADR collision

- Time: 2026-08-28T08:18:16-06:00
- From: Program Manager
- To: Victor Shaw
- Status: integration-blocking mechanical collision found read-only

Appearance HEAD `ef19a9b` descends from `9db68c4` and introduces
`0026-compose-appearance-settings-through-settings1.md`. Public `main`
`a8bbc56` now owns accepted ADR-0026 for contained virtual-desktop
qualification and ADR-0027 for AppShell. Before exact handoff, Victor must
renumber Appearance's ADR file, index entry, owning-page links, and MkDocs nav
to **ADR-0028** in the same non-amended descendant. Do not reuse or overwrite
0026/0027. This is a current-main integration repair, not a product-scope
change. The manager made no edit in Victor's worktree.
