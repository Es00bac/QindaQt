# Mary Cartwright — first-party export midpoint

- Exact base: `f84d3ae8d1dde0016f5504fdcc8a7ccb1c760e6f`.
- Implemented an opt-in AppShell composition over the accepted exporter and real dbusmenu transport, with asynchronous registrar owner tracking, truthful XWayland/native-Wayland identity handling, exactly-once catalog activation, and close teardown.
- Wired the File Manager with one appended composition line while preserving its authoritative visible in-window menu.
- Added a private-bus row that launches the real File Manager and drives it through the production `GlobalMenuAppletComposition` using an injected authenticated identity snapshot.
- Debug selected verification is green at 44/44 rows. The first full run exposed a direct QML-root destruction lifetime bug; weak window tracking repaired it, and the same selector then passed completely.
- Release verification is in progress.
