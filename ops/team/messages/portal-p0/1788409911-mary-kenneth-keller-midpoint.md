# Portal P1 verified midpoint — Mary Kenneth Keller

- Timestamp: 2026-09-02T22:31:51-06:00
- Exact base: `f350028c1cfea0bf0e92f4c2fb0d5949ee6b65a9`.
- Material result: the real `xdg-desktop-portal` frontend now selects QindaQt Settings only for the QindaQt desktop, forwards exact reads and live changes, routes injected FileChooser to the declared KDE fallback, and leaves Background closed.
- Toolkit result: the Qt 6 offscreen `xdgdesktopportal` theme reports initial Dark and live Light through `QStyleHints`; the probe applies and checks its derived palette for both states.
- Isolation: the private-bus fixture reserves PermissionStore and Documents names, preventing installed helper activation; no host bus or display is used.
- Evidence so far: `^qindaqt\.portal-` passes 9/9 in both Debug and Release; source shape and documentation gates pass. Final audit and immutable candidate commit remain active.
