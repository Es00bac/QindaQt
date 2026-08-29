# Lovelace the 3rd — Bluetooth B0 manager replay midpoint

- Time: 2026-08-28T14:29:55-06:00
- Replay: all five accepted commits were applied in order onto manager base `f783f8389a563423e6e6bf2d98bd276748657a1e`; current replay tip is `f38707b`.
- Content proof: all 54 non-shared Bluetooth paths are byte-identical to independently accepted candidate `278a5f9520f3cc47e554816961c0c653295fcbc4`. The only reconciled paths are `docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`, `mkdocs.yml`, `src/CMakeLists.txt`, and `tests/CMakeLists.txt`; each retains the complete manager state plus Bluetooth additions. Candidate diff contains no `ops/team/**` path.
- Fresh Debug evidence: configure exit 0; 67-step build of eight test executables plus `qindaqt-bluetooth-service` exit 0; focused source/private-bus selector 8/8 exit 0.
- Current action: building the complete install graph under `/mnt/d/QindaQt/builds/bluetooth-manager-replay-lovelace3/dev` so the exact staged-install row can verify deployed artifacts and a linked installed consumer. Documentation, source shape, final provenance, clean-tree proof, and exact different-worker review request follow.
