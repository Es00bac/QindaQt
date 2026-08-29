# Material findings: exact-owner and bounded-lease contracts are not enforced

- Reviewer: Turing the 3rd
- Timestamp: 2026-08-28T19:39:58Z
- Exact candidate: `e3e2719dfb3f76b119c4c6c7ccd1193012acff35`
- Current gate: focused strict Debug build underway; hostile standalone reproduction next

## Blocking candidates under adversarial verification

1. `src/services/network_client/src/network_client.cpp:240-265` checks the
   transport signal/request owner but never requires decoded `Snapshot::owner`
   to equal it. A payload carried on owner A can therefore install owner B's
   lineage and publish `Ready`, contradicting the exact-owner public contract.
2. `network_client.cpp:204-225` calls `m_model.clear()` on every owner change.
   `NetworkModel::clear()` erases the only lineage high-water mark
   (`network_model.cpp:56-59`), so a real A → B → A owner cycle can accept A's
   retired epoch after each refetch. The existing client test named for A/B/A
   covers only a wrong-owner delayed signal and a legitimate newer return, not
   this old-epoch owner cycle (`tst_network_client.cpp:215-266`).
3. `network_validation.cpp:312-321` accepts every nonzero
   `ScanLease::deadlineEpochMs`; `network_scan_lease.cpp:15-32` then trusts that
   absolute deadline. A hostile snapshot may use `INT64_MAX` and keep scan
   intent busy effectively forever, contrary to the documented one-second to
   two-minute maximum and the anti-pinning rationale.

I will classify only after executable reproductions and the remaining gates.
No product bytes have been changed. If reproduced, these go back to Veda as one
bounded descendant repair with direct regression tests; I remain available for
the exact rereview.
