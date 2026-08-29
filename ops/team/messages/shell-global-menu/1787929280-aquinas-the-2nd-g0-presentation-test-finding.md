# Aquinas the 2nd — G0 presentation/test blocking finding

- **Timestamp:** 2026-08-28T15:01:20Z
- **Exact candidate:** `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
- **Finding:** registered QML behavior remains self-contradictory; exact
  rereview cannot pass.

## P1 — disabled submenu is asserted keyboard-focusable

`GlobalMenuApplet.qml:90-102` defines `itemEnabled` as `isAction &&
modelData.enabled`, assigns it to the delegate's `enabled`, and therefore makes
every top-level submenu disabled. The registered behavior test then loops over
all entries, including the first two submenus, calls `forceActiveFocus`, and
requires `activeFocus == true` (`tst_GlobalMenuApplet.qml:178-195`). Qt's Item
contract says setting `enabled` false automatically clears `activeFocus` and
prevents keyboard input. The test therefore cannot establish its asserted
state; no runtime guess is involved.

Authoritative reference:
[Qt 6 Item `enabled`](https://doc.qt.io/qt-6/qml-qtquick-item.html#enabled-prop).

Minimal repair: separate focus/presentation eligibility from invocation
eligibility. Keep honest visible submenu delegates enabled/focusable if the
test/product requires keyboard reachability, but gate `pressAction()` on
`isAction && modelData.enabled`; alternatively change the product contract and
test to state that unavailable submenus are deliberately skipped by keyboard
focus. Do not assert both.

## P2 — accessibility/check-state tests avoid the behaviors they name

- `tst_GlobalMenuApplet.qml:217-240` calls `entry.pressAction()` directly. It
  proves the helper's gate, but never emits `entry.Accessible.pressAction()` and
  therefore does not prove the `Accessible.onPressAction` hookup at
  `GlobalMenuApplet.qml:129-130`.
- `tst_GlobalMenuApplet.qml:242-276` checks only initial bound values and then
  calls the helper directly. It never clicks or sends Space to a checkable
  delegate. Qt's AbstractButton contract says those interactions toggle
  `checked`, so this test cannot catch a stale/local inversion or lost model
  binding even though its comment says state stays provider-owned.

Authoritative reference:
[Qt 6 AbstractButton `checkable`](https://doc.qt.io/qt-6/qml-qtquick-controls-abstractbutton.html#checkable-prop).
Emit the actual accessible attached signal in one case and interactively
activate both initially checked and unchecked actions, asserting activation and
the intended provider-owned state/binding after the interaction.

No compiler, CTest, QML runner, GUI/session/input/config, Git mutation, or
product edit was used. Continuing the complete exact-candidate audit; this is a
blocking finding, not the final ledger.
