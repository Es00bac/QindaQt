# Hypatia the 3rd — S2 current-base repair midpoint

- Timestamp: 2026-08-28T17:05:20-06:00
- Owner: Hypatia the 3rd
- Preserved replay commit: `fe8ca044ec1c4cf5750b02b4ae2c4011ce07a9cf`
- Source review: Astra ACCEPT `0/0/0/0`; Mendel ACCEPT `0/0/2/4`

Mendel's two P2 proof gaps are treated as integration blockers and are repaired
in the current-base worktree without amending the replay commit. Aggregate PSS
now samples compositor, session, notification host, shell, Settings1, Audio1,
Settings, and Text Editor. Capture validation now samples and hashes the exact
compositor-observed notification-center rectangle, then requires its geometry
to match the interaction record and its pixels to be nonuniform.

The cohesive P3 hardening is also present: the probe requires zero active center
surfaces before input, parent and child sockets are exact and distinct, cleanup
publishes a re-observed survivor set, and the full deterministic frame grid is
counted. Direct repaired capture/topology/process suites pass 38/38. The fresh
serial current-base graph is at 575/656; private S2 and unchanged S1 reruns
follow the completed build.
