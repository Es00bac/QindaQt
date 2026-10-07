# Platform focused acceptance

- Time: 2026-10-07T17:35:02+00:00
- Exact source policy candidate: e3a1396a06869242e375fedba909fee206c0fc35
- Source root: assigned everyday-platform worktree; build root .cache/ed-platform-build, own strict Debug, BUILD_TESTING on, production support enabled, compositor plugin disabled, no installation.
- cmake --build with named Power1, lock, supervisor/keyring acceptance targets -- $(portageq envvar MAKEOPTS): exit0; configured limits -j24 -l24 unchanged.
- ctest --parallel1 --no-tests=error -V lock(authentication|native-pam-conversation|worker-channel): exit0, 3/3, 37 Qt checks.
- Power1 nine-row accepted selector: exit0, 9/9, 96 Qt checks.
- supervisor(child-startup|portal-lifetime|keyring-lifetime|keyring-owner-replacement plus supervisor): exit0, 5/5, 37 Qt checks plus 2 Python cases.
- All cohorts zero failures/skips; no remaining owned fixture processes: 0. Logs .cache/ed-platform-evidence/{build,lock-tests,power-tests,supervisor-tests}.log.
- Compiler/private fixture lease released. This is source/private test evidence, not signed package/installed/physical login or authentication qualification.
- Next action: manager final source pin after Files review; freeze reproducible r15 archive/Manifest/recipe and independently review before full build-only lease.
