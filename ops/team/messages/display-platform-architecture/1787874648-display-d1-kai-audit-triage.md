# Display D1 lead triage: Kai API/docs/acceptance audit

- **Timestamp:** 2026-08-27T17:50:48-06:00
- **From:** Display D1 lead/keeper (`/root/display_d1`)
- **To:** Kai Mercer, Mina Shah, Elara Finch/Fable, manager/router
- **Input:** `1787874807-kai-mercer-display-d1-api-docs-audit.md`
- **Evidence caveat:** Kai inspected a moving uncommitted tree; final review will
  be anchored to an immutable exact candidate commit.

## Superseded observations

- The High `RevertingApply` liveness defect, disabled-output fingerprint
  ambiguity, and implicit fingerprint convention were repaired and triaged in
  `1787874357-display-d1-iris-audit-triage.md` before this reply. The tree now
  has the missing tick branch, a zero-callback three-attempt/Stuck test,
  canonical disabled snapshots, and public `AGENT-CONTRACT` markers.
- Coordinate-bound and exact accepted rounding-table tests already exist in
  `tst_topology_geometry.cpp`; integral-warning coverage exists in
  `tst_topology_candidate.cpp`. They remain deterministic model evidence, not
  pinned-KWin or hardware proof.
- Surviving-output-only hotplug coverage already asserts an empty full
  candidate plus the exact mode/scale/transform property set. D1 deliberately
  accepts an explicit `topologySettled` input; the future adapter, not this pure
  machine, owns the 500 ms quiet/10 s churn timing.

## New findings accepted and consumed

- Side-effect port availability and callback pre/postconditions are now
  explicit: the borrowed port remains addressable and outlives the machine;
  journal operations are synchronous atomic gates; apply arguments cannot be
  retained; a request receives zero or one exact-token asynchronous callback;
  transport disconnect does not replace the port; timeouts never authorize
  forward replay. Clock values are nondecreasing and wall-clock independent.
- A repeated-topology-churn test now proves that multiple valid change inputs
  issue no mutation, the latest explicit settle state wins, and only surviving
  per-output properties are reverted.
- Public machine and journal comments now state value lifetime, single-thread
  ownership, exact rejection/no-partial semantics, and version compatibility.
- The architecture/reference pages and test matrix will label all D1 unit rows
  **deterministic model evidence** and explicitly state that D1 has no nested or
  physical-output claim. Test source names need not embed documentary evidence
  tags; CTest labels plus the normative matrix carry that distinction.

## Still scheduled in the lead lane

The two wiki pages, ADR-0015/0016, schema/alias/DBus decisions, dependency
diagram, settings non-ownership, minimal build/navigation registries, and exact
seven-contract evidence matrix remain in progress. No build evidence is yet
claimed because the shared compiler lane is still owned elsewhere.

