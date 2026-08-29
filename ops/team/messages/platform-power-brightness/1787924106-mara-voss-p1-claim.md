# Mara Voss — Power applet P1 presentation-slice claim

- Timestamp: 2026-08-28T13:35:06Z
- Worker: Mara Voss, Power applet P1 implementer (GLM
  `zai-coding-plan/glm-5.3-flash`, reasoning high)
- Exact base: PB-0 candidate `30783867d7f2f49c9ad740c90f1c824614510b72`
  (confirmed HEAD of my worktree; Priya exact rereview PASS `1787923474`)
- Branch/worktree: `worker/power-applet-p1` at
  `/home/cabewse/work_SPaC3/container-wm-workers/power-applet-p1`, clean tree
- Status: working, source/tests/docs/static gates only

## User-visible outcome

A bounded, presentation-only Power applet model: power/battery aggregation
rows, charging/discharging/unknown states, time remaining surfaced only when
known, critical/low/full semantics, brightness control request state with
failure feedback, keyboard-backlight/accessibility identities, and
loading/degraded/unavailable states, plus focused hostile tests. Pure
source/static slice; no configure, compiler, CTest, GUI, session, or hardware
evidence is claimed.

## Path ownership

New paths only: `src/shell/power_applet/**`, `tests/shell/power_applet/**`,
and my primary wiki page plus additive `mkdocs.yml` nav entry. I will not edit
PB-0 sources, production shell, QML registries, shared CMake, roadmap,
`TASK_LIST`/`HANDOFF`, or other applets; future additive registry seams will be
listed in my handoff for the manager.

## Dependency and collision risks

- PB-0 commit `30783867` remains provisional until manager integration; any
  new blocking Priya finding stops my work and preserves it on this branch.
- I consume only PB-0 public values (`power_protocol`) and pure composition
  (`brightness_model`) headers; no client seam exists yet, so the applet model
  accepts injected snapshots/results only. No UPower/sysfs/logind/Wayland/
  brightness hardware access and no policy ownership.
- No known path collisions: Devika is handing off (not live on product paths),
  Priya is not live, no other worker owns shell applet paths.

Next: read module/coding/docs wiki pages, then implement the slice.
