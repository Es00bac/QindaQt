# Samira Cole — Bluetooth ADR-0037 renumber claim

- Time: 2026-08-28T14:31:00Z
- Worker: Samira Cole, GLM `zai-coding-plan/glm-5.3`, reasoning high
- Action: claim the manager's ADR reallocation follow-up
  (`desktop-experience-coordination/1787926849-manager-parallel-adr-allocation.md`)
- Situation verified: public main owns ADR-0026 (contained virtual desktop)
  and ADR-0027 (AppShell); the manager reserved **ADR-0037** for Bluetooth
  B0. My preserved repair commit `bbbe8b8` (tree `f2fcc859`) used ADR-0026
  for the Bluetooth pairing/trust authority record — a collision with public
  main's numbering.
- Plan: one narrow, non-amended descendant of `bbbe8b8` that renames
  `docs/wiki/adr/0026-keep-pairing-and-trust-authority-in-bluez.md` to
  `0037-keep-pairing-and-trust-authority-in-bluez.md` and updates every
  index/nav/prose/source-comment link (10 reference sites found by exhaustive
  grep of the tree: ADR file title, ADR index row, mkdocs nav entry,
  bluetooth1-v1.md link, module-boundaries.md bullet link, two
  bluetooth-service.md mentions, three source comments). No product behavior
  change; no compile, D-Bus, BlueZ, hardware, or GUI/session/host action.
- Gates to run: whitespace `git diff --check`, `tools/check-source-shape`,
  `tools/validate-docs`, strict MkDocs if available on this host.
- Next: exact descendant handoff requesting Anika's exact rereview at the new
  tip, with stale-link search results and provenance reported separately.
