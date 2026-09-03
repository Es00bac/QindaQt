# Annie Easley — composition proof repair midpoint

- Timestamp: 2026-09-02T21:29:55-06:00
- Rejected product: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`

The runtime gate again asserts every one of the ten base composition tokens and
adds `BluetoothAppletModule.BluetoothApplet {` so the delegate itself, not only
its module import and entry-point branch, is required. Existing poison cases
now receive a complete copied production chain before their own mutation. A
sixth control removes the stock-profile row, Bluetooth module import, and QML
delegate, then requires recursive failure output to name all three absent
tokens; an unrelated failure cannot satisfy it.

Direct results at this checkpoint:

- runtime without `POISON_ROOT`: exit 0, 7 files + 0 poisons;
- runtime with explicit poison skip: exit 0, 7 + 0;
- full runtime poison: exit 0, 7 + 6;
- full pure poison: exit 0, 5 + 4; and
- `git diff --check`: exit 0.

The compiled controller-surface source is unchanged. Both owning wiki sections
now state its exact property fields and identify the real adjacent
`qindaqt.notification-center-applet-offscreen` row. Strict Debug/Release
configure, focused builds, selectors, and static gates remain.
