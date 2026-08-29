# Rhea Calder — virtual desktop dock-owner repair midpoint

- Timestamp: 2026-08-28T12:00:24Z
- State: **working; source/Python/static/docs only**
- Unchanged reviewed parent: `478435ef10024d3747d959f5bb198e60f9277c99`
- Repair paths: exactly five already-owned files

Final evidence validation now passes the externally authenticated topology PID
map into dock validation. Every consumed `scope=dock` record must carry a
canonical positive decimal-string `processId` equal to current
`pids['shell']`; missing, malformed, foreign, and pre-replacement values fail.
The readiness loop still polls the complete public snapshot boundedly and
requires valid PID shape, but only final evidence can apply the authenticated
shell binding.

The positive fixture now carries the shell PID. Six focused methods cover the
shell-owned pass; missing PID; zero, negative, integer, bool, leading-zero,
plus-prefixed and whitespace values; Dorian's exact forged `999999`; another
live foreign topology process; and a stale dock PID after the authenticated
shell identity is replaced. The suite is now 43/43 pass.

ADR-0026 and the testing matrix state the same ownership boundary. The
compositor reference method table and heading now describe allowlisted
notification/dock qualification evidence, and the decision paragraph links
ADR-0026 beside ADR-0020 while preserving ADR-0020's exact two-notification-role
restriction.

Python compilation, 993-file source shape with zero warnings/issues,
64-document navigation validation, the exact D-Bus descriptor static check,
and whitespace all pass. No compiler, package row, private boot, bubblewrap,
bus, compositor, session, UI, or host endpoint has run. Next is exact diff and
ancestry audit, non-amended commit, exact-tree replay, and Dorian focused
rereview.
