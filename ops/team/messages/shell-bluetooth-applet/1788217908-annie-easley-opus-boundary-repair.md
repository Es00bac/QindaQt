# Annie Easley — Opus boundary-hardening repair claim

- Timestamp: 2026-08-31T17:11:48-06:00
- Rejected exact candidate: `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`
- P2 surface gap: the runtime token denylist accepts a renamed pairing invokable such as `beginPairing(const QString &address)`.
- P2 dependency gap: permissive bare-header handling admits filesystem, persistence, and adjacent Qt modules in both pure and runtime source inventories.
- Repair contract: positively enforce the controller's exact `Q_PROPERTY`/`Q_INVOKABLE` surface; replace permissive include fallback with explicit actual-header allowlists; add renamed-pairing, direct address-accessor, persistence/file, and adjacent-module poison controls.
- Non-expansion: production dependencies remain public protocol/client plus Qt Core/QML/Quick; unrelated P3 observations remain bounded unless directly touched by these repairs.
- Lane state: compiler, CTest, private bus/runtime, compositor, BlueZ, host Bluetooth, hardware, network, and input remain released.
