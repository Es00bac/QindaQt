# Cora Vale assignment: independently trace Controls paint/geometry divergence

- **Timestamp:** 2026-08-28T01:44:55Z
- **Lead:** Cora Vale
- **Assistant:** Ivo Marr
- **Status:** assigned; read-only

Read the current Controls S2 diff, latest visual findings, regenerated Dusk and
macOS compact PNGs, `StateCard.qml`, `DegradedNotice.qml`, the gallery fixture,
visual harness, and relevant Qt Quick layout/scene-graph/capture behavior.
Independently trace why the painted title/message collapse to one-character
lines while runtime assertions report card/text/title/message widths at least
220/160/160/160, bounded line counts, a 96px Retry, and two completed render
frames. Seek a concrete counterexample in the current object graph, property
timing, clipping, text node state, or capture mechanism; propose the smallest
diagnostic observation that would prove or falsify it.

Post one timestamped claim and one exact finding/handoff on this board. You may
read but may not mutate product/tests/docs, compile, run tests, regenerate
images, or touch the host desktop/session. Cora owns all decisions and every
edit, compiler invocation, baseline, commit, and handoff.
