# Notification Live Lyra rereview triage and next gates

- **From:** Soren Pike (Notification Live lead/keeper)
- **Reviewed state:** 70-path uncommitted candidate on base
  `c4982697858c083828bd406f1aa56c4e942bcc10`
- **Review:** `1787878072-lyra-voss-rereview-pass-handoff.md`
- **Verdict consumed:** PASS; F1-F5/F8 are closed and no source blocker remains

Bounded-note decisions:

- **N1 accepted now.** Add a throwaway Python child created with
  `start_new_session=True`, assert the private-session predicate rejects its
  real PID through the `getsid` mismatch branch, and terminate/wait it in a
  `finally` block. This closes branch coverage without any host process.
- **N2 keep/defer as recommended.** The host-state predicate is harmless
  defence in depth. The existing host-exit test proves the user-visible
  supervisor contract; manufacturing Qt signal-delivery order would make the
  test less representative.
- **N3 accepted now.** Raise the test helper's ordinary host lifetime above the
  sanitizer/QTRY budget. Every test already owns and boundedly tears down the
  helper, so the timer is not product policy and should not create a 5-second
  race.
- **N4 accepted as a review limitation.** The exact candidate commit will make
  all 70 paths reviewable before the mandatory different-worker candidate
  review. No assertion is made from an untracked diff alone.
- The residual pidfd window is accepted exactly as documented: immediate
  re-resolution plus POSIX-session containment prevents signals from escaping
  the disposable tree; expanding this outcome to pidfds is not justified.

After the two test-only notes and source gates, the next compiler-owned command
is the smallest invalidated Debug build:

```sh
cmake --build build/notification-live-debug-current --parallel 1 \
  --target qindaqt_session_supervisor_tests qindaqt-notification-live-probe
ctest --test-dir build/notification-live-debug-current \
  -R '^(qindaqt\.session-supervisor|session\.notification-live-driver-unit)$' \
  --output-on-failure
```

Then the full current-source Debug build plus the previously passing 50-test
notification/session/shell selector, followed by the full Release incremental
rebuild, are required for fresh provenance. Sanitizer, package, and installed
private nested rows remain later gates and will not start without the sole
compiler/runtime lane.
