# Shannon the 3rd — Network N0 exact repair handoff

- **From:** Shannon the 3rd
- **To:** Turing the 3rd
- **CC:** Platform services, manager
- **Outcome:** QQ-005 Network N0 rejected-candidate repair
- **Exact commit:** `c619acd34f051e715a1b3532e44fcfcfcce45116`
- **Tree:** `4f85c643e00186e1b9a27892fc8126204d67d03a`
- **Sole parent:** `e3e2719dfb3f76b119c4c6c7ccd1193012acff35`
- **Commit distance:** exactly one non-amended descendant
- **Branch:** `worker/network-n0-repair-shannon3`
- **Worktree:** `/mnt/d/QindaQt/worktrees/network-n0-repair-shannon3`
- **Worktree state:** clean at handoff

## Outcome

The exact P0/P1/P2/P3 `0/5/6/2` rejection is repaired. Decoded payload owner
must equal the signal/request/current owner; the model retains accepted epoch
high-water across owner loss and rejects real A→B→A retirement; scan leases use
a bounded 1–120 second duration and local monotonic adoption rejects invalid
clocks and overflow including `INT64_MAX`; every public collection/text/timing/
intent traversal cap is fail-closed; quoted, unquoted, suffix-shaped, malformed,
or unsafe secret diagnostics cannot enter public state/error/signal strings;
unsafe Unicode presentation scalars are hidden/rejected while safe supplementary
Unicode remains valid; boolean bytes and `wireValid` are canonical; and failed
transport start rolls back completely so a second start invokes transport
again without sticky Ready truth.

The three libraries share focused install component `QindaQtNetworkN0`.
Registered hostile proof contains the exact eight rejected regressions, and the
registered source-policy negative row requires rejection of QtDBus, `QTimer`,
and NetworkManager poison. ADR-0045 and the normative module, protocol,
architecture, and testing pages record ownership, dependency, compatibility,
threading/lifetime, secret, lease, and N1+ boundaries. The SSID digest wording
now correctly calls it a correlation pseudonym, and frequency is documented as
a defensive observed range rather than an actual-channel allowlist.

Veda Park is credited for the inherited Network N0 foundation. Her operational
records remain preserved on the shared Team Board and the three machine-local
`ops/team/**` files committed by the rejected product candidate are removed
from this final tree.

## Changed paths

- Docs/ADR/nav: `docs/wiki/adr/0045-fence-network1-pure-boundary.md`,
  `docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`,
  `docs/wiki/architecture/network-service.md`,
  `docs/wiki/reference/network1-v1.md`,
  `docs/wiki/development/testing-harness.md`, `mkdocs.yml`.
- Product source/API/package registrations: all changed files are confined to
  `src/services/network_protocol/**`, `src/services/network_model/**`, and
  `src/services/network_client/**`.
- Focused tests/registrations: changed files are confined to
  `tests/services/network_protocol/**`, `tests/services/network_model/**`, and
  `tests/services/network_client/**`; new files are
  `tst_network_adversarial.cpp` and `check_boundary_negative.cmake`.
- Product-tree operational cleanup: deleted
  `ops/team/messages/platform-services-network-n0/1787942648-veda-park-claim.md`,
  `ops/team/messages/platform-services-network-n0/1787944071-veda-park-finding.md`,
  and `ops/team/workers/veda-park.md`.

No integration ledger, Task List, Handoff ledger, unrelated module, host
NetworkManager/D-Bus/radio/secret/session/UI state, or broad install registry
was changed.

## Executable evidence

- Rejected base reproduction: unchanged Turing external hostile executable
  **2 passed / 8 failed**, exit 8, exactly the eight terminal blockers.
- Fresh strict-warning Debug build under `/mnt/d`: requested three libraries
  plus ten focused executables built warning-clean, exit 0.
- Debug exact registered selector: **13/13 passed**, exit 0, including hostile,
  isolated installed-header consumer, clean boundary, and poison boundary.
- Debug direct QtTest execution: **118/118 passed**, exit 0; the hostile
  executable contributes eight functional cases plus init/cleanup, **10/10**.
- Fresh strict-warning Release build under `/mnt/d`: same focused targets built
  warning-clean, exit 0; exact registered selector **13/13 passed**, exit 0.
- Installed component consumer: passed independently in both Debug and Release;
  no unrelated whole-tree archive was required.
- Out-of-build install-prefix probe: inner script rejected before deletion with
  `Refusing to replace a staged prefix outside the test build tree` (expected
  inner exit 1; assertion wrapper exit 0).
- `python3 tools/validate-docs`: 77 documents/nav validated, exit 0.
- `mkdocs build --strict`: exit 0.
- `python3 tools/check-source-shape`: 1,175 files, zero violations, exit 0.
- `git diff --check`, changed-source SPDX scan, added-content machine-path
  scan, exact one-commit provenance, and final worktree cleanliness: pass.

## Remaining bounded caveats

N0 intentionally has no resident Network1 service, concrete transport,
NetworkManager/secret-agent integration, persistence, Settings/shell UI,
physical radio mutation, or hardware qualification. Those remain N1+ and do
not weaken the pure boundary evidence above.

## Requested next action

Turing the 3rd: please perform the retained exact rereview of
`c619acd34f051e715a1b3532e44fcfcfcce45116`, rerunning the same external hostile
probes, focused Debug/Release selectors, component consumer, policy poison,
docs, provenance, and collision checks. Manager: do not integrate on this
handoff alone; await Turing's exact terminal verdict on this hash.
