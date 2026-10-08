# Integrate reviewed compact Audio Settings

- Timestamp: 2026-10-08T11:42:35+00:00
- Product candidate: d050c1f3d3ee017db9e535be642c121131e45ba5
- Independent source/private-native review: 1a75d62a343bed9d29a02f9825aae5844f767834
- Base: 40d7f209661a9257812aca96503a3d9675cb81f3
- Manager previous HEAD: daf5c217235be7a5eaed7d8272737a8ccdc0d15e

Integrated the exact accepted candidate. ADR-0362 and its index are Accepted; only the device-presentation clauses of ADR-0288 are superseded. Corrected the primary wiki's minor traversal wording to include Details. Preserved reviewer board and replies byte-for-byte. No product/test edits beyond the reviewed candidate.

Root independently read the full review receipt and recomputed every global source-shape error path's Git blob equality: 63 errors on 61 base-unchanged paths. The full gate remains failing; 318-line changed QML warning is explicitly reviewed. Raw summary retained at .cache/audio-density-integration-20261008/source-shape-inherited.json.

Candidate actual gates remain seven strict owning targets, nine passing CTests, seven full Qt summaries 57 passed/0 failed/0 skipped/0 blacklisted, four normal/2x wide/compact captures; independent review verifies archive and all33 indexed payloads. Integrated reruns are next, not inferred.

R18 is still installed. No release package, installed layout, live physical audio, full mixer, Windows/Android or entire ED completion is claimed. Windows first repaired synthetic attempt is retained as a timeout, with an observed too-long Unix socket path; no retry is granted. Source-only old-ASan policy authoring remains separate.
