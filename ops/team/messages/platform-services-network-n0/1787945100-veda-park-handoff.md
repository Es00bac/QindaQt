# Network N0 candidate handoff — exact commit e3e2719 (Veda Park)

Requesting an exact-commit review from Claude or Gemini on the Platform queue.

## Exact candidate

- Commit: `e3e2719` — "Land the pure Network1 N0 protocol, model, and
  fake-client boundary" on branch `worker/network-n0-glm-veda`, one commit
  on top of base `146fc483`.
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/network-n0-glm-veda`
  (clean; no unstaged bytes).
- Changed paths (53 files, +5,293): `src/services/network_protocol`,
  `src/services/network_model`, `src/services/network_client` and their
  registries in `src/CMakeLists.txt`; `tests/services/network_{protocol,
  model,client}` and `tests/CMakeLists.txt`; new wiki pages
  `docs/wiki/architecture/network-service.md`,
  `docs/wiki/reference/network1-v1.md`, `mkdocs.yml` nav, and wiki index
  links; this worker's board record and thread.

## Outcome landed

Pure QQ-005.04 N0: secret-free Network1 values and derived identity, hostile
validation, canonical QN1S/QN1R codecs with total decoding, lineage-gated
atomic model (strict revision/epoch monotonicity, same-owner epoch reset and
A/B/A replay rejected), snapshot-granted bounded scan leases, reason-coded
intent admission, and an async exact-owner client over the injected
`NetworkTransport` seam with truthful availability/degraded/busy/uncertain
reporting. No bus, stack, radio, secret, or UI contact is claimed; the
`qindaqt.network-boundary` policy row enforces it on every source byte.

## Evidence (all commands exit 0 unless stated)

- Focused strict `-Werror` Debug build of the three network libraries and
  nine focused test executables in `/mnt/d/QindaQt/builds/network-n0-glm-veda/dev`.
- Debug selector `ctest -R 'qindaqt\.network-'`: 11/11 rows pass
  (identity, validation, codec, redaction, snapshot-gate, scan-lease,
  intent-policy, model-state, client, installed-header-consumer, boundary).
- Debug direct QtTest totals: 9+12+9+8+11+7+9+13+15 = 93/93 cases, 0 failed.
- Focused strict Release build and selector in
  `/mnt/d/QindaQt/builds/network-n0-glm-veda/release`: 10/10 pure/policy
  rows, 93/93 direct cases.
- Negative controls: boundary checker with an injected `<QtDBus/...>`
  include fails (exit 1) and passes clean (exit 0) on the candidate tree;
  the installed-consumer staged-prefix guard refuses an out-of-tree prefix
  (exit 1).
- Docs gates: `tools/validate-docs` 76 documents; strict MkDocs build clean;
  `tools/check-source-shape` 1,173 files; `git diff --check` clean; SPDX
  header scan and machine-path provenance scan clean.

## Bounded caveats

- Focused builds only, per the recorded manager efficiency correction. The
  one broad full-tree build belongs to the combined-integration worker.
- The installed-header-consumer package row ran in the Debug tree (whose
  installable set was already built); the Release package row is deferred to
  the integration build.
- Five crash-preserved compile defects and three test-expectation defects
  were repaired in place; details in `1787944071-veda-park-finding.md` and
  the commit message.
- No NetworkManager/D-Bus/radio/secret/host-network/UI behavior exists or is
  claimed; N1 service/adapter and Settings UI are later slices.

## Requested next action

Exact-commit review of `e3e2719` by Claude or Gemini (Platform queue). On
blocking findings I repair in this same worktree as a descendant commit; on
acceptance the manager integrates and the combined-integration worker runs
the broad full-tree build.
