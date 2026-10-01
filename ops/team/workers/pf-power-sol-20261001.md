# Native power worker — 2026-10-01

- Status: working — auditing native lock-before-sleep authority
- Outcome: selected-session logind lifecycle, owned sleep delay inhibitor, and protected manual suspend handoff
- Base: 53bf486bac9c067e5fff8143e2457e69e4a7b01d
- Branch: worker/pf-power-sleep-20261001
- Worktree: .cache/pf-power-sleep-20261001
- Ownership: native_sleep new module/tests; NativeLockRuntime focused APIs/tests; SessionActions suspend handoff/tests; ADR0321 and affected primary wiki pages. Shared composition/build/navigation seams requested from Program Manager.
- Resource state: no compiler/private/native runtime slot claimed; awaiting grants before builds/fixtures

## Updates

- 2026-10-01T09:55:26Z — Claimed isolated exact-base worktree after origin fetch. No pre-existing logind-lock/sleep candidate found in fetched hub refs. Found unused protected runtime callback and direct SessionActions login1 Suspend bypass; proposed dedicated transport/policy/facade module and minimal supervisor composition seam to manager.
- 2026-10-01T10:12:00Z — Implemented dedicated NativeSleep transport/coordinator/facade and Sleep1-only manual SessionActions handoff. Shared seams explicitly granted. Real ordinary socket/PIDFD + production native targeted receipt ports compose private logind wire tests; delay FD is pipe-backed for closure evidence. Existing large SessionActions query source decomposed into cohesive availability/dispatch helpers. Root granted qinda compiler only; runtime fixtures still queued.
- 2026-10-01T10:21:18Z — First private acceptance run PASS 7/7 at 9add7dab (32.85s, no skips). Production native target/nonce receipt and actual ordinary socket/PIDFD are exercised. Added bounded uncertainty reporting after dispatched sleep, initial PreparingForSleep snapshot/newer-signal fencing, session-removal revocation. Closing startup capability convergence with owner-checked Sleep1 Changed invalidations and focused resume/cancel/restart tests before final immutable handoff.
- 2026-10-01T10:24:50Z — Expanded final private acceptance found supervisor watcher self-deletion crash; source fix retires callbacks immediately and defers sender destruction while FD closes synchronously. Fake Changed signal export and action/bus deadline race repaired. Logind 17/17, runtime 11/11 and native request 16/16 passed; candidate not handed off. Compiler/runtime slots released pending root short regrant.
