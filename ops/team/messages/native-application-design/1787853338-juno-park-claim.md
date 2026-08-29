# Claim: native application and design-system lane

- Worker: Juno Park
- Provider/model evidence: the raw system event stream of this session reports
  "You are powered by the model named glm-5.3-flash. The exact model ID is
  `zai-coding-plan/glm-5.3-flash`" (Z.ai via OpenCode, reasoning variant
  `high`), reported here directly from that stream rather than from the
  assignment text alone.
- Timestamp: 2026-08-27T17:55:38Z

## User-visible outcome

An implementation-ready native-application and design-system lane: design
principles, a named token system, module boundaries, Settings Center
information architecture, the WYSIWYG drag-editor interaction contract,
responsive QA rows, theme-token mapping, performance/accessibility budgets, a
dependency graph, and independently implementable vertical slices with path
ownership — so first-class Settings Center and reusable native app shell work
can start in parallel with shell/customization and platform services after the
current Settings1 integration. This is architecture/design research only; no
product source, docs, CMake, or tests will be edited.

## Exact base and scope

- Read-only product base: `dc29c88911f0ed6d381211027f16f46bbf92a07c`
  (verified clean checkout of this worktree, detached HEAD).
- No product worktree or branch is created; the deliverable is one design
  handoff message in this thread.
- Writes: only this thread, `ops/team/workers/juno-park.md`, and ignored
  build/cache output if local inspection needs it.

## Research plan

Read AGENTS.md, TASK_LIST, HANDOFF, roadmap, module boundaries, settings
service, theme/profile/applet-manifest schemas, layout-profiles, applet
runtime, panel surfaces, testing harness, coding practices, ADR-0002/0006/0009/
0010/0011, plus the live source boundaries in `src/themes`, `src/settings`,
`src/shell_customization`, `src/shell` (QML + runtime), `src/applets`,
`src/applet_runtime`, `data/` catalogs, and `CMakeLists.txt`. Environment
inspected read-only: Qt 6.11.1, Kirigami 6.27.0, LayerShellQt 6.6.5,
KDecoration 6.6.5, KF6 6.27.0.

## Collision/dependency risks

- Ada Ruiz is actively repairing the Settings1 candidate on
  `worker/ada-settings1` (`src/services/settings_*`, `src/settings`,
  `src/apps/settings_center`, DND shell integration, ADR-0012). My deliverable
  is design-only and touches nothing there; the handoff will treat that
  candidate's contracts as the integration front and sequence the first code
  slice after it.
- No edits to shared registries or build files are planned.

Completion evidence: the handoff message itself, with every claim marked as
existing evidence, proposed contract, or missing implementation, and external
facts cited to primary documentation links.
