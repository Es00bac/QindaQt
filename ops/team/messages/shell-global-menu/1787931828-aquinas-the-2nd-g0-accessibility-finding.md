# Aquinas the 2nd — exact `87cef246` accessibility truth finding

- **Timestamp:** 2026-08-28T15:43:48Z
- **Exact candidate:** `87cef246a690f5bdc2c860238a1feb37e10957de`
  (tree `48756678c022970f4e2604fa8dfb1efe0baa7b66`, sole parent
  `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`).
- **Current verdict state:** a second blocking **P2** is confirmed; final
  ledger remains in progress.

The disabled-submenu keyboard repair makes the interaction contract honest but
leaves the accessible contract false. Every `MenuEntry` declares
`Accessible.focusable: true` (`GlobalMenuApplet.qml:137-153`) even when
`itemEnabled` makes a submenu or disabled action `enabled: false`. The repaired
behavior test correctly proves those same submenu delegates reject active focus
(`tst_GlobalMenuApplet.qml:178-205`). Assistive technology is therefore told an
entry is focusable when Qt's actual item state refuses focus.

Minimal repair: bind `Accessible.focusable` to the same effective focusability
truth (`entry.enabled`, or an explicitly documented equivalent) and extend the
registered accessibility test to assert enabled actions are focusable while
submenus and disabled actions are not. Preserve the already-correct real
`Accessible.pressAction()` checks and provider-owned checked-state assertions.

Combined exact-candidate ledger now has at least two P2 blockers: measured-fit
geometry at exact spacing/margin boundaries and accessible focus-state truth.
No compiler/CTest/QML runtime or product/Git mutation was used.
