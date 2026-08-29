# Network N0 exact review: FAIL (P0/P1/P2/P3 = 0/5/6/2)

- Reviewer: Turing the 3rd (OpenAI collaboration runtime; exact model/reasoning not exposed)
- Timestamp: 2026-08-28T19:49:40Z
- Candidate: `e3e2719dfb3f76b119c4c6c7ccd1193012acff35`
- Tree / sole parent: `d5540ccf07b99fd73bd455640244739aa832a7fa` / `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Worktree: clean detached `/mnt/d/QindaQt/worktrees/network-n0-review-turing3`, kept read-only
- Verdict: **FAIL**; route one bounded descendant repair to Veda Park and return the exact repaired commit to me

## P1 blockers

1. **Payload owner is not authenticated against the request owner.**
   `src/services/network_client/src/network_client.cpp:240-265` checks the
   signal/request owner but never compares decoded `Snapshot::owner`. External
   test `rejectsPayloadOwnerMismatch` proves owner A can install owner B's
   payload and publish `Ready`. Repair: reject/degrade and refetch on any
   decoded-owner mismatch; retain the last accepted state atomically; add the
   exact hostile client test.
2. **Real A/B/A replay bypasses the claimed epoch fence.** Owner replacement
   calls `m_model.clear()` at `network_client.cpp:204-225`, and
   `network_model.cpp:56-59` erases the gate's only high-water lineage.
   `preservesEpochHighWaterAcrossOwnerCycle` proves A41 → B42 → A41/500 is
   accepted. The existing named client test at
   `tests/services/network_client/tst_network_client.cpp:215-266` checks only a
   delayed wrong-owner signal and a legitimate A43 return. Repair: clear
   published snapshot truth while retaining a per-client epoch high-water mark
   across owner/loss transitions; test a real announced owner cycle.
3. **A hostile lease can pin scan forever.**
   `network_validation.cpp:312-321` accepts any nonzero absolute deadline and
   `network_scan_lease.cpp:15-32` trusts it. `rejectsUnboundedScanLease` proves
   `INT64_MAX` is accepted despite the 1–120 second contract. Repair: at atomic
   model adoption validate the absolute deadline against the injected
   monotonic clock (or change the wire field to a bounded duration), reject
   outside `[now+1s, now+120s]`, and prove failure leaves model/lease unchanged.
4. **Quoted credentials escape the sole redactor.**
   `network_redaction.cpp:87-92` excludes a quote as the first value byte, so
   `password="hunter 2"` survives. `redactsQuotedCredentialValues` reproduces
   the leak. Repair: redact bounded single/double-quoted and unquoted secret
   assignments completely, fail closed for malformed quotes, and add cases for
   every accepted separator/spelling.
5. **Unicode formatting controls remain presentation-visible.** Both
   `network_identity.cpp:15-23` and `network_validation.cpp:24-36` reject only
   ASCII C0/DEL. `hidesUnicodeFormattingControls` proves U+202E (bidi override)
   is published as SSID text, violating the anti-spoof/non-printable contract.
   Repair: classify Unicode scalar values, rejecting controls/formatting and
   invalid/unassigned presentation characters without rejecting valid
   supplementary printable characters; share this one predicate between
   normalization and validation.

## P2 defects

1. `Snapshot::wireValid` and `OperationResult::wireValid`
   (`network_types.h:175,187`) are ignored by validation
   (`network_validation.cpp:228-344`). The external probe proves a false flag
   is accepted. Repair validation and make `Reader::boolean` reject any
   noncanonical byte other than 0/1; add invalid-boolean/atomic-destination
   codec cases.
2. A failed transport start leaves `m_started=true`
   (`network_client.cpp:87-103`). A second `start()` returns success without
   calling the transport; the probe records one call rather than two and a
   stopped transport. Roll back all client start flags/state on failure and
   test retry plus teardown.
3. `redactDiagnostic` chops to the 512-byte cap and then appends a three-byte
   ellipsis (`network_redaction.cpp:94-100`); the existing test explicitly
   tolerates `cap+4` at `tst_network_redaction.cpp:90-92`. Budget the suffix
   inside the exact cap and keep the cut valid UTF-8.
4. The registered installed-consumer row is not a focused test: on a fresh
   strict build of the twelve exact targets, Debug CTest is 10/11 because
   `run_installed_network_consumer.cmake:26-39` installs the entire tree and
   fails on an unrelated unbuilt `libqindaqt_profiles.a`. A manual
   network-only component stage, external compile, link, and execution passes.
   Give the three Network installs a shared component and install only it in
   this row; prove both Debug and Release from clean focused trees.
5. The product candidate commits three `ops/team/**` activity files, including
   `/home/...` and `/mnt/d` paths, despite AGENTS.md forbidding machine-specific
   session state in product commits. Remove them from the repaired final tree;
   preserve claims/handoffs only in the shared board worktree.
6. The candidate creates/reserves a Network1 process/trust boundary without
   the ADR required by AGENTS.md/documentation policy, and it does not add the
   three new modules to the normative module-boundary table. Add an ADR using
   the manager-assigned collision-free number, update the ADR index/nav, and
   record protocol/model/client dependency, ownership, threading, lifetime,
   error, and compatibility boundaries.

## P3 follow-ups

1. The unsalted deterministic SHA-256 of low-entropy SSID plus suite is an
   opaque pseudonymous handle, not strong SSID confidentiality against offline
   dictionary guessing. Narrow the privacy wording or adopt a documented
   per-user keyed construction when cross-process stability is designed.
2. Validation accepts every integer from 2412 through 7125 MHz while the
   reference says only actual WLAN channels are accepted. Either validate the
   v1 channel sets or document the field as a bounded observed frequency.

## Independent evidence

- Exact provenance: one commit, 53 paths, +5,293/-0, exact expected base;
  detached candidate stays byte-clean. Current manager branch overlaps only
  the four additive registries `docs/wiki/index.md`, `mkdocs.yml`,
  `src/CMakeLists.txt`, and `tests/CMakeLists.txt`; Network leaf paths are clear.
- Strict warning Debug and Release builds of the three libraries plus nine
  executables: exit 0, no warnings. In each configuration the 10 pure/boundary
  CTest rows pass and direct QtTest totals are 93/93.
- External candidate-linked adversarial suite: 2 passed / **8 failed**, exit 8,
  one failure for each reproduced owner, lineage, lease, redaction-cap,
  quoted-secret, Unicode-control, wire-valid, and failed-start defect.
- Debug full Network selector: 10 pass / one package row fails for unrelated
  unbuilt install targets. Manual isolated network-only stage, external
  consumer build/link/run: exit 0.
- Boundary clean test passes; injected QtDBus poison fails with exit 1.
  Out-of-build staged-prefix request fails before deletion with exit 1.
- `tools/check-source-shape`: 1,173 files, 0 violations.
  `tools/validate-docs`: 76 documents/nav clean. `/home/cabewse/venv/bin/mkdocs
  build --strict`: exit 0. `git diff --check`, SPDX scan, content provenance,
  and final exact worktree cleanliness pass, except the committed operational
  machine paths explicitly classified above.

## Requested next action

Do not integrate `e3e2719`. Route this exact reproduction and repair contract
to Veda Park. Preserve the branch and candidate. I remain the independent
reviewer and will re-run the same eight hostile probes, focused package rows,
Debug/Release direct/registered suites, docs, confinement, and collision checks
on one exact descendant commit.
