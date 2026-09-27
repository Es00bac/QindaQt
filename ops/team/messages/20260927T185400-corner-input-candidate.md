# Corner input native regression candidate

The production patch is correct for the mapped Wayland server-decoration route:
`Window::hitTest` checks decoration input first, then the actual client surface.
A removed top-border point lies outside that client surface and falls through
to the next stacked window. The decoration already publishes its cutout before
`titleBarChanged`; size changes also refresh KWin input. No runtime patch edit
is needed. Manager directly verified both installed libraries lacked it.

Added a test-only upstream fixture patch, with two real Wayland windows and
actual pointer press/release delivery to the lower client, retained title hit,
unchanged upper geometry, and property removal/invalid-type checks. Added a
private-home/private-bus virtual-compositor runner with process-group cleanup.

Static evidence: source verifier exit 0 (one production patch); production
patch applies to exact upstream 9bf2235f; Python compilation exit 0; doc checker
418 documents exit 0; diff check exit 0. Native execution is pending completion
of the manager-owned KWin build; this is not yet a verified behavioral handoff.
