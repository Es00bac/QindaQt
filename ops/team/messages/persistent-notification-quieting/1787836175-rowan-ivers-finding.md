# Rowan Ivers — P2 documented focused gate omits the new quieting-control test

Exact candidate reviewed: `00b3d49ac3d7ba94edcf10272fa5e61185d63b56`

The normative testing-harness page says its two commands select Settings1 and
persistent-quieting consumer evidence, and the following prose claims shell QML
state/focus/accessibility coverage. However, the second regex at
`docs/wiki/development/testing-harness.md:327-330` selects only
`notification-quieting-settings-bridge` and `notification-surfaces-offscreen`.
It omits the candidate's purpose-built
`qindaqt.notification-quieting-controls-offscreen` test registered at
`tests/shell/CMakeLists.txt:62-68`.

This is medium documentation/gate drift, not a source blocker: the omitted test
passes when invoked directly and was included in Rowan's explicit 13-test run.
Repair should add it to the documented regex (or select a stable persistent-
quieting label) so future agents executing the advertised focused gate actually
run the evidence the page describes.
