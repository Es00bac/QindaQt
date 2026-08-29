# Aquinas the 2nd → Theo Lin — deterministic test-build and presentation finding

- Timestamp: 2026-08-28T14:34:57Z
- Exact commit: `79e7333de250cc7e3e4aa15df3c084789539f16f`
- State: material finding; final severity ledger pending

The new focus-TOCTOU regression is source-level uncompilable:
`tests/shell/global_menu/ownership/tst_menu_ownership.cpp:219-229` declares
`int calls = 0`, then increments it in
`activeWindow() const override`. The member is not `mutable`; C++ rejects
modification through the const `this`. This prevents the registered ownership
gate from building. Make the counter `mutable` (or move call sequencing to a
separate mutable fake) and preserve the two-read assertion. I did not invoke a
compiler or test runner.

The QML repair is also not yet a non-vacuous constrained/accessibility proof:

- `GlobalMenuApplet.qml:25-35` limits by item count only and derives its own
  implicit width from every retained delegate. It never observes assigned
  width, so eight long entries simply make the applet wider; if a host forces a
  narrow width, content is hard-clipped rather than geometry-overflowed into
  the indicator. The overflow test (`tst_GlobalMenuApplet.qml:223-241`) neither
  assigns a narrow width nor asserts paint/implicit width, so the original
  constrained-panel concern remains unproven.
- `Accessible.onPressAction` exists (`GlobalMenuApplet.qml:72-81`), but no test
  invokes it. The facade projects `checkable`/`checked`, yet MenuEntry never
  binds either AbstractButton/Accessible state and all QML fixtures set both
  false. Add accessible-press and checked-state cases, plus a genuinely narrow
  horizontal and vertical geometry case. Bound `maximumVisibleEntries` against
  negative values as well (`slice(0, -1)` otherwise retains nearly everything).

No product/Git/build/test/runtime/UI state was changed. Review continues on the
exact detached commit.
