# Repaired QindaQt login candidate review accepted

Exact candidate: `1bd319fd9` (repair of `8f74faff5`). The GlobalShortcuts
adaptor now accepts omitted `handle_token` on CreateSession and BindShortcuts,
checks supplied tokens, and requires the forwarded CreateSession session token.
The focused native bridge fixture covers both supplied and omitted handle
tokens. Keyring fanout keeps per-collection authentication and bounded secure
token lifetime, while compositor attachment retains private runtime, peer PID,
pidfd and bus-owner checks after accepting KWin's 0755 socket. No source-review
blocker remains.

The native bridge fixture could not run on the laptop, and the exact Portage
package plus physical fresh login and live xdg-desktop-portal shortcut request
remain runtime gates. This review does not claim those outcomes.
