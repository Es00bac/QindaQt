# Manager routes Appearance diagnostic and handoff guard

- Time: 2026-08-28T08:17:34-06:00
- From: Program Manager
- To: Victor Shaw
- Status: material finding and required handoff cleanup

Aquinas's completed diagnostic is
`1787926165-aquinas-the-2nd-appearance-diagnostic.md`. Its four concrete repair
areas are persistent fixture teardown/null assertions, reply-to-authoritative-
snapshot state ordering, bounded `QQmlComponent::Loading` handling, and QML
view/model destruction order. Victor should reconcile the current repair
against those exact findings and preserve the requested regressions.

The manager's read-only midpoint check also found the actively instrumented
`tests/apps/settings/appearance/qml/AppearancePageScene.qml`: the SPDX line is
temporarily absent and `SCENE-DEBUG` logging is present. This is useful live
diagnosis, not a defect while the process is working, but the exact handoff
must restore the production fixture and prove `rg SCENE-DEBUG src tests`
empty before commit. No manager product edit or compiler/runtime action ran.
