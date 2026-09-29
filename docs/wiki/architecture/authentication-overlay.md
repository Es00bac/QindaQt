# Authentication overlay platform boundary

`QindaQt::AuthenticationOverlay` owns only the Wayland keyboard-exclusive
authentication surface. Polkit and the native keyring prompt use its public
`OverlaySurface` API; requester policy, credentials, persistence, layout and
appearance remain in those consumers.

`initializePlatform()` must run before QGuiApplication. On the GUI thread,
`configure(QQuickWindow&,scope,error)` borrows a hidden window that has not
created its native handle. It retains no pointer. It configures a full-output
layer-shell overlay with exclusive keyboard interactivity before any show.
Invalid lifetime/thread/order, unsupported platform or unavailable role returns
false and an owned non-secret error; callers must not show a fallback window.
Compatibility is source-level; consumers must preserve the pre-show ordering.

Output selection uses the screen under Qt's global cursor, otherwise primary.
Wayland can report an unavailable cursor as(0,0), so exact multi-output pointer
placement remains unqualified until native output selection is tested.
This platform module does not prove the prompt's requester, authenticate a
password, manage Qt text allocations or suppress process dumps; each consumer
owns those contracts.

See [Polkit agent](../apps/polkit-agent.md),
[native keyring daemon](keyring-daemon.md) and
[ADR-0296](../adr/0296-native-keyring-daemon-boundary.md).
