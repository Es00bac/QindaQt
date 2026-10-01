# Keyring idle fixture source review

- Time: 2026-10-01T11:12:21+00:00
- Exact candidate: f1d7b89b4fe33a274ebb2fbd934dac7a69080eac
- Finding: no blocking source defect. Public refresh starts a generation without prior idle; revoke clears availability and idle. Added fake operations perform those transitions and publish changed synchronously, consistent with policy observation. The fake does not pretend to exercise Wayland protocol lifetimes; separate public observer gates own those cases.
- Existing policy cases retain assertions for screen uncertainty/settings loss, true compositor-idle versus elapsed time, unavailable fail-closed policy, disabled timeout, typed persistence and failed-save atomicity. No assertions removed or weakened.
- Documentation: two added lines accurately describe fake behavior; no new architectural decision.
- Commands: git fetch qinda:~/git/container-wm.git (exit 0); git diff 7a7c9fbb f1d7b89 -- (exit 0); source/contract reads. MAKEOPTS on qinda observed -j24 -l24, unmodified.
- Next gate: own independent tst_keyring_lock_policy build and keyring_lock_policy CTest after manager compiler grant. Source-only finding is not executable acceptance.
