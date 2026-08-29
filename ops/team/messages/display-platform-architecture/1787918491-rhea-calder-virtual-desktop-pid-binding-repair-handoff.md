# Rhea Calder — virtual desktop dock-owner repair handoff

- Timestamp: 2026-08-28T12:01:31Z
- Candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Tree: `3d703cde297a10b5c0dfc4b6ff1009240fa2ee45`
- Exact parent: reviewed FAIL `478435ef10024d3747d959f5bb198e60f9277c99`
- Repair paths: 5
- Repair manifest SHA-256: `8fa09a1d30b9672b8a7d0b7021a119cdf853564e2594c5e5c4d9b94877dcee1b`
- Full public-first-parent boundary: unchanged 23 paths, manifest `6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`

## Finding closure

Final boot evidence now supplies the authenticated process PID map to dock
validation. Every consumed `scope=dock` record must have a canonical positive
decimal-string `processId` exactly equal to current `pids['shell']`. The
readiness loop retains bounded complete-snapshot polling and validates PID
shape, while a valid-looking foreign PID can advance only to final validation,
where it fails against authenticated shell identity.

The positive fixture uses the shell PID. Six focused tests cover the positive
row; missing PID; zero/negative/int/bool/noncanonical string values; exact
forged `999999`; another authenticated foreign process; and a stale dock PID
after authenticated shell replacement. ADR-0026 and the testing authority state
the binding. The compositor reference table and heading cover allowlisted
notification/dock qualification evidence, and its decision paragraph links
ADR-0026 beside ADR-0020 without editing or weakening ADR-0020's two-role
Notification boundary.

## Exact-tree evidence

- Focused desktop-session units: **43/43 pass**.
- Python compilation: pass.
- Compositor D-Bus descriptor static check: pass.
- Source shape: 993 checked, zero skipped/warnings/issues.
- Documentation/navigation: 64 documents, pass.
- Exact five-path/full-manifest identity, whitespace, reviewed ancestry, and
  clean worktree: pass.
- No compiler, package row, private boot, bubblewrap, private bus, compositor,
  session, UI, host endpoint, or hardware action ran.

## Requested action

Dorian: perform a focused exact rereview of
`dc377388af530411c3c281cb0171ccfc74590b0e`, reproducing exact `999999`, missing,
malformed, foreign and stale/replacement dock identities and confirming the
reference correction. The private boot remains prohibited until that rereview
passes and the manager explicitly allocates the runtime lane.
