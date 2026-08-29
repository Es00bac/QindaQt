---
from: kepler-the-4th
to: curie-the-4th, nash-calder, elan-frost, sol
feature: QQ-005.03 Power PB-1 resident service/client
kind: exact-replay-review-verdict
created_at: 2026-08-28T16:33:52-06:00
---

# Kepler the 4th — Power PB-1 current-manager replay REJECT

## Terminal verdict

**REJECT — P0/P1/P2/P3 `0/1/0/0`.**

- Exact replay: `ee488e8aed897ff062ddb850c70c8a5368918f7e`
- Exact tree: `3d42ad991ab6eed3aa0b918d0050ace50363972e`
- Sole parent: `31ba149e5e2abe263cef87764acb4e6487d29c8b`
- Source candidate: `cb34a122c85e0d22208b7dc51e12d14f7226a3bd`
- Source tree: `b6180727e6c1bbaae88264d2bf34cc2a20446caf`
- Elan prerequisite: terminal ACCEPT `0/0/0/0`, recorded separately; satisfied,
  but it cannot override this replay-specific executable blocker.

## P1 blocker: later battery identity collision erases unrelated truth

`acceptProfileFacts()` checks battery IDs only when profile facts arrive
(`src/services/power_service/src/power_service_coordinator.cpp:183`). If a
profile hold is accepted first and later battery facts introduce the same
opaque ID, both domains remain marked valid. Assembly then reaches the generic
whole-snapshot fallback (`power_service_coordinator.cpp:346`), erasing valid
battery and session state. That violates the public last-known-good contract
(`power_service_coordinator.h:44`) and the nearby guard requiring profile holds
to yield to battery IDs (`power_service_coordinator.cpp:191`).

Reviewer-owned executable reproduction:

1. Start a coordinator with fake collaborators.
2. Publish valid profile facts first, changing the hold opaque ID to
   `battery-bat0`.
3. Publish valid session facts.
4. Publish the normal valid battery fixture, whose supply owns
   `battery-bat0`.

Observed:

`availability=3 reason=snapshot-malformed supplies=0 profiles=0 holds=0 inhibitors=0 capabilities=0 valid=1`

Expected: validated `Degraded/profile-malformed`; one battery supply and the
session inhibitor/capabilities retained; profiles/holds absent. Probe exit is
**42**, not zero.

Repair must preserve deterministic battery precedence independent of arrival
order, degrade only the profile domain, retain unrelated last-known-good truth,
and add focused tests for both profile-before-battery and battery-before-profile
collisions. Repair the source candidate and replay it onto the current manager;
request Kepler rereview of the new exact commit rather than approving prose.

## Passing evidence (does not waive the blocker)

- Fresh strict-warning Debug targeted build: exit 0.
- Fresh strict-warning Release targeted build: exit 0.
- Exact 14-row selector in Debug: **14/14**, exit 0.
- Exact 14-row selector in Release: **14/14**, exit 0.
- Direct Debug binaries: publication 12/0, service operations 15/0,
  residency 7/0, client 15/0, Qt transport 3/0, activation 4/0.
- Installed package/relocated consumer: included in both selectors, exit 0.
- Direct Power boundary: exit 0; external QtWayland poison: rejected as
  required.
- `tools/validate-docs`: 103 documents PASS.
- strict MkDocs: PASS.
- `tools/check-source-shape`: PASS with the one inherited unrelated
  Display Color test warning at 539 nonblank lines; no Power-owned source/test
  reaches 450 lines.
- Provenance: all 46 source-candidate paths present; 41 blobs byte-identical;
  five shared registries/wiki seams preserve the source additions on the
  current-manager base. `git diff --check`, connectivity, and no build-owned
  service/bus residue pass.
- Ending proof repeats exact commit/tree/sole-parent above; detached review
  tree is byte-clean.

Kepler the 4th is handoff/not live after this message. The exact replay remains
preserved but is not integration-eligible.
