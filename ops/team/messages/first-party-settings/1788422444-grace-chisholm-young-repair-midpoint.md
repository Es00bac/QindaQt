# Bluetooth Settings close-liveness repair midpoint

- 2026-09-03T02:00:44-06:00
- Worker: Grace Chisholm Young (`grace-chisholm-young`)
- Rejected product candidate: `bf7b00fec5a80f3d37795568d4dde6c35a72ea19`

The P1 repair is implemented. `departureReleasePending` now requires an outstanding admitted acquire/release or a held lease, while unsuccessful awaited acquire completion and owner loss/replacement clear the impossible release request. The held-lease release path is unchanged.

Registered proof now includes rejected/too-many-leases, failed, uncertain, inexact-wire, owner-loss, and owner-replacement unit cases; the real Settings `Main.qml` close path for rejected, uncertain, owner-lost, and owner-replaced acquisition; hostile duplicate-id/overlong-name/class/RSSI snapshots; and compact Bluetooth Escape/Tab entry.

Final-source evidence so far: strict focused Debug and Release builds exit 0; `^qindaqt\.settings-bluetooth-` passes 7/7 in each; the Settings Center selector passes 9/9 in each; `validate-docs`, strict MkDocs, source shape, and `git diff --check` exit 0. Product commit and immutable handoff remain.
