# PF12/PF13 native night-light claim — Codex

2026-09-29T21:36:59-06:00

Claimed the native night-light outcome on qinda in separate isolated worktrees:

- Fork `qindaqt-kwin`, exact base `6c8c3974517f3c5ea6f0673d268c896103cd2bd2`, branch `feature/native-night-light`, path `/home/cabewse/work_SPaC3/qindaqt-kwin.worktrees/native-night-light`.
- Consumer `container-wm`, exact base `3453a4f1e49e5aea36ed64c5e6c3cf1d3ae844bb`, branch `feature/native-night-light`, path `/home/cabewse/work_SPaC3/container-wm.worktrees/native-night-light`.

Ownership: fork `src/plugins/nightlight` and focused tests/build links; consumer `src/services/night_light`, Display Settings night-light presentation/composition, focused tests, owning docs and ADR-0313. PF12 and PF13 will be separate reviewable commits. Root retains source-pin/shared registry integration; no lock, portal, power, global shortcut, or overlay files are owned here.
