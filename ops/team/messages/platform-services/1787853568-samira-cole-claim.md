# Claim: focused platform-services parallelization plan

- Worker: Samira Cole
- Provider/model: OpenAI Codex, GPT-5 family; exact serving identifier and
  reasoning level are not exposed to this collaboration worker.
- Timestamp: 2026-08-27T11:59:28-06:00

## User-visible outcome

An implementation-ready dependency graph and set of independently deliverable
platform-service outcomes covering audio, power, brightness, Bluetooth,
network, display, color, fonts, portals, and clipboard. The plan will specify
what QindaQt should reuse from supported Linux/Qt infrastructure, each focused
process and public API boundary, failure/threading/ownership policy, exact path
ownership, packaging and UI consumers, private-bus/nested-display and deferred
hardware evidence, documentation/ADR work, and safe integration order. It will
explicitly avoid a monolithic Platform service.

## Exact base and scope

- Read-only product base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
  in `/home/cabewse/work_SPaC3/container-wm`; no product source, documentation,
  build files, test files, or live desktop/session/device configuration will be
  changed.
- Board worktree: `/home/cabewse/work_SPaC3/container-wm-workers/team-board`,
  branch `ops/team-board`.
- Writes are limited to this thread and `ops/team/workers/samira-cole.md`.

## Collision and dependency risks

- The active Settings1 candidate owns `src/settings/**`,
  `src/services/settings_*`, `src/apps/settings_center/**`, notification/DND
  shell paths, and broad shared build/documentation registries. The plan will
  not treat those as implementation-safe until the exact candidate integrates.
- Native-app and future shell consumers need one route/app-shell contract and
  stable service-client state semantics before provider and UI work meet. Any
  cross-lane question will be a separate append-only record under this thread,
  per the manager's coordination contract.
- Shared root/src/tests CMake registries, MkDocs navigation, task/handoff state,
  and any settings-shell route registry remain manager-owned integration glue;
  implementation workers should receive unique module paths.

## Completion evidence

The final handoff will link every cross-lane question, cite repository evidence
and upstream primary documentation, and distinguish runnable virtual/private
evidence from hardware-later qualification. No build or runtime success will be
claimed for this design-only lane.
