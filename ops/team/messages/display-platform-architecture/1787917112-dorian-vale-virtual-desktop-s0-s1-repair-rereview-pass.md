# Dorian Vale — virtual desktop S0+S1 exact repair rereview PASS

- Timestamp: 2026-08-28T11:38:32Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Verdict: **PASS for the source-safe S0+S1 candidate boundary**
- Findings: **P0/P1/P2/P3 = 0/0/0/0**
- Exact candidate: `f28f443b7aae2d635481f49e847a7e1e1a3b573b`
- Exact tree: `cf840061b9680df539a150d28db09a6f97a00c6c`
- Exact parent/reviewed FAIL: `fd9faab5ab79017be903dafc6f0587d09c511f49`
- Original base: `7da3300cbe9a22fda077a07ff94b03b7adad396f`

## Immutable identity and scope

The repair is a non-amended descendant of the exact reviewed FAIL. Its delta is
exactly the ten handoff paths, +1,027/-163, sorted path-manifest SHA-256
`0f246a74bd2b693871634ceb8b0f340faf5ef956b5566beabb2a0e351271a9ce`.
Against the original base, the candidate retains the exact original 20 paths,
3,601 insertions, and sorted manifest SHA-256
`9dc4cae417408377abc7436fc602edc75fb3f6ee4204f777e187db5b43621a0c`.
The detached review worktree is clean and `git diff --check` passes.

## Prior finding disposition

1. **P1 readiness — PASS.** `await_complete_snapshot()` owns one monotonic
   deadline and accepts only a newly acquired probe document whose required
   service ownership and exact output/generation, input, dock and application
   topology all validate together
   (`tests/session/desktop_session_runtime.py:75-165,291-314`). Missing startup
   state retries; malformed ownership and method errors fail immediately. Units
   cover sequential service/app/dock readiness, never-ready, after-deadline,
   immediate method failure and invalid service identity
   (`tests/session/test_desktop_session_topology_unit.py:232-300`).
2. **P1 application identity/role — PASS.** `observed_applications()` matches
   and preserves the compositor's exact `applicationId`, nonempty window ID and
   title, and carries the topology's declared process role without inventing a
   PID unavailable from the public inventory
   (`tests/session/desktop_session_topology.py:70-105,233-265`). Matching titles
   with attacker IDs reject, and a wrong declared role fails final validation
   (`tests/session/test_desktop_session_topology_unit.py:214-230`).
3. **P1 provenance — PASS.** Each attempt creates a never-reused build/run-
   sentinel result root; authenticates the source and destination; copies every
   regular artifact plus every process/probe log; writes combined sandbox
   output; and writes result metadata last
   (`tests/session/test_desktop_session_nested.py:142-219,222-290`). Success,
   stale/symlink destination, tampered source, timeout and inner failure paths
   are covered (`tests/session/test_desktop_session_contract_unit.py:264-388`).
4. **P2 PSS — PASS.** Complete evidence requires exactly integer
   `residentPssKiB` and the fixed 1,048,576 KiB ceiling; bool/string, negative,
   wrong ceiling and over-limit values reject
   (`tests/session/desktop_session_topology.py:278-295` and
   `tests/session/test_desktop_session_topology_unit.py:302-316`).
5. **P2 teardown ledger — PASS.** Cleanup returns one record per captured
   identity with role/PID/group/exact path/start ticks and terminal observation
   phase `already-exited`, `term`, or `kill`; PID reuse is never signalled and
   no phase claims graceful exit
   (`tests/session/desktop_session_process.py:201-293`). Final validation binds
   every topology role/PID and exact field/phase shape
   (`tests/session/desktop_session_topology.py:297-357`). TERM, PID-reuse, KILL,
   missing-role, mismatched-PID and invented-graceful cases are covered.

## Evidence

- Fresh focused Python units: **37/37 pass**.
- Fresh `py_compile` over desktop-session source/tests: pass.
- Fresh source shape: **962 files**, zero skips/issues.
- Fresh documentation/navigation validation: **58 documents**, pass.
- Exact repair/full manifests, diff check and clean tree: pass.
- Directly inspected preserved exact-descendant Debug and Release logs:
  `desktop.virtual.sandbox-unit` plus `desktop.virtual.package-contract` are
  **2/2 pass** in each configuration.
- ADR-0026 and the testing authority accurately describe readiness, observed
  identity, per-attempt provenance, exact PSS and non-graceful teardown phases.

## Boundary

This PASS authorizes integration only of the exact source-safe S0+S1 candidate.
No compiler, bubblewrap, private bus, compositor, nested session, live boot row,
UI, host display/input/cursor/session/configuration or hardware endpoint was
used by this reviewer. It does **not** qualify a live desktop. Live maturity
still requires manager integration of D0 output/generation and Notification
Live with development-only `scope=dock`, sole private-runtime allocation, the
exact `desktop.virtual.boot.1080p` row, independent artifact/log inspection,
and zero-survivor/run-root proof.

