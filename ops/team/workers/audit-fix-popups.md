# Audit popup and activation repair

- Status: working — verified candidate; preparing exact-commit review handoff.
- Base: 9728612046940b55d69f85c3811eb38a08a0963b

## Updates
- 2026-09-05T15:48:00Z — Claimed assigned shell applet and activation paths; inspecting focused gates.
- 2026-09-05T15:55:06Z — Popup and generation changes implemented; focused independent build compiling. Tests now target the separate popup window.
- 2026-09-05T16:03:00Z — 20/20 focused CTests pass, including 30px host popup geometry, Escape/pairing cancel, stale generation/queued gestures, natural menu size, boundary and API surface gates. MkDocs strict and validate-docs pass.
