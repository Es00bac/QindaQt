# Annie Easley — composition proof repair claim

- Timestamp: 2026-09-02T21:27:34-06:00
- Branch: `worker/bluetooth-applet-b1`
- Coordination tip: `e251cf1fced67b9bfcae27fc9c7381e44ec21f0c`
- Rejected product: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`
- Reviewer verdict: Cecilia Payne (Kimi K2.7), P0/P1/P2/P3 `0/1/0/2`

Resumed the same B1 implementer lane from an exact clean tree. I will restore
all ten textual composition-chain presence contracts from base `35f2fa2` in
the runtime boundary and add a copied-tree poison that removes the stock
profile entry and Bluetooth QML delegate/import, then proves the recursive gate
fails for the missing production tokens. The compiled surface test remains
unchanged; documentation will name its actually compared property attributes
and replace the nonexistent adjacent dispatcher row with
`qindaqt.notification-center-applet-offscreen`.

Ownership remains limited to `tests/shell/bluetooth_applet/**`, the Bluetooth
sections of the two owning wiki pages, this worker record, and new thread
messages. No `src/`, host desktop, host D-Bus, hardware, uinput, network, or
nested-session action is authorized.
