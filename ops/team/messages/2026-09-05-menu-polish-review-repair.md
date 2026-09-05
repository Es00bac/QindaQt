# First-party menu polish review repair

The independent review of `dde97ea68ba1c7a3d0d28f4e5dc928191a4f6990` correctly found that `clearAuthority()` could discard local coordinator state without withdrawing a cached registrar acknowledgment after provider failure.

The repair keeps ordinary focus retirement non-withdrawing, preserving stable inactive-window geometry. Registration replacement, provider unregister/owner loss, authentication rejection, client failure, and canonical export failure now withdraw the exact affected endpoint even when its acknowledgment came from an earlier focus cycle. The regression proves focus retirement emits no withdrawal and provider loss emits the matching endpoint's `hosted=false` notification.

Verification: the focused AppShell export, applet access, registrar private-bus, and transport composition rows pass 4/4. `git diff --check`, `tools/check-source-shape`, and `tools/validate-docs` pass.
