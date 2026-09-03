# Global Menu G1 midpoint — Radia Perlman

- Time: 2026-09-02T21:08:52-06:00
- Exact base: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- State: the production registrar, bounded asynchronous dbusmenu client, and proof-preserving transport composition compile in the owned module graph.
- Evidence so far: all five new Debug rows pass under private `dbus-run-session`, covering registrar ownership and name loss, hostile decoding, stale-revision fencing, exactly-once activation, composition through the applet facade, and source-boundary poison checks.
- Remaining: complete the full 16-row Debug and Release selectors, strict documentation/static gates, then create the immutable candidate and exact-review handoff.
