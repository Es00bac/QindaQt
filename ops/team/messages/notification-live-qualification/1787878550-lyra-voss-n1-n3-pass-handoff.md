# Lyra Voss N1/N3 closure rereview handoff to Soren Pike

- Worker: Lyra Voss (Anthropic, `claude-opus-5`, reasoning xhigh)
- To: Soren Pike, lead/keeper, Notification Live qualification
- Timestamp: 2026-08-27T18:56-06:00
- Triage: `1787878183-soren-pike-lyra-rereview-triage.md`
- Reviewed state: `worker/notification-live`, HEAD
  `c4982697858c083828bd406f1aa56c4e942bcc10`, uncommitted, still 70 paths
- Scope: only your two test-only closures. F1-F5/F8 remain closed per
  `1787878072-lyra-voss-rereview-pass-handoff.md`; I did not re-derive them.

## Verdict: PASS for both N1 and N3

Both repairs do what they claim. One non-blocking residual is noted under N1.

I also confirmed the change is exactly test-only: since my previous handoff at
18:47, only `tests/session/test_notification_live_unit.py` and
`tests/session_supervisor/session_token_child_helper.cpp` have changed, and the
path count is unchanged at 70. No product, docs, or build file moved.

## N1 - CLOSED. The mismatch branch is reached deterministically

Anchors: test `tests/session/test_notification_live_unit.py:151-169`; guard
`tests/session/notification_live_process.py:46-63`; imports at `:10` and `:20`
of the test, so neither `subprocess` nor the guard symbol is missing.

Branch reachability, argued against the guard's own control flow:

- The child PID is greater than 1, so the early bound at
  `notification_live_process.py:48` does not fire.
- The child is alive and unreaped (it sleeps 30 s and the `Popen` object holds
  it), so `os.getsid` succeeds and the `OSError` path at `:53-56` is not taken.
- The comparison at `:57` is a guaranteed mismatch, not a probabilistic one. A
  `setsid` child's session id equals its own PID, and a freshly allocated PID
  cannot equal the session id of any live session leader, so the child's sid
  can never coincide with the runner's. `assertRaisesRegex` searches the
  message built at `:58-62`, and "outside private session" is a literal
  substring of it.

The counterexample I specifically hunted was a fork/`setsid` ordering race: if
`Popen` returned before the child executed `setsid()`, the guard would observe
the parent's session id, no exception would be raised, and the test would fail
spuriously - or, worse, a future reader would think the branch was proved when
it was not. That race does not exist here, and I checked it against the
interpreter in this environment rather than from memory
(`/home/linuxbrew/.linuxbrew/Cellar/python@3.14/3.14.2_1/lib/python3.14/subprocess.py`,
Python 3.14.2):

- `start_new_session=True` is excluded from the `posix_spawn` fast path by the
  `and not start_new_session` term at `subprocess.py:1868`, so the
  `_posixsubprocess.fork_exec` path is taken.
- On that path the parent blocks at `subprocess.py:1938-1945` reading the
  child's error pipe until the child either reports failure or closes it
  through a successful `exec` (the pipe is close-on-exec).

`setsid()` is made in the child before that `exec` - that part is the
documented contract of `start_new_session` and lives in compiled
`_posixsubprocess`, which I cannot read here, so I am relying on the documented
contract for that one link rather than on source I inspected. Combined with the
error-pipe synchronization above, `Popen(...)` cannot return until the child is
already its own session leader.

The failure direction is also safe. If the child were somehow gone before the
call, the guard would raise "could not authenticate private process PID", which
does not match the asserted regex, so the test would fail loudly instead of
passing for the wrong reason.

Cleanup is bounded and unconditional: the `finally` at `:163-169` runs even
when the assertion fails, sends SIGTERM, waits 5 s, escalates to SIGKILL, waits
5 s, and the `Popen` object reaps the child. A sleeping CPython has the default
SIGTERM disposition, so the first signal is sufficient. If even the post-KILL
wait expired, `TimeoutExpired` propagates and fails the test rather than
silently leaking.

Residual, bounded and non-blocking: this child is deliberately outside the
test's session and process group, so no group teardown covers it. If the test
process itself were hard-killed between `:156` and `:163`, the child would
survive until its own `time.sleep(30)` ended. Shortening that sleep to about 5
seconds would bound the orphan window; it is strictly optional and I am not
asking for it.

## N3 - CLOSED. The race is removed, product policy is untouched

Anchor: `tests/session_supervisor/session_token_child_helper.cpp:58-60`. The
only change in this pass is the ordinary branch value, from 5'000 to 30'000.
The three sites I flagged still budget 5-second `QTRY` waits
(`tests/session_supervisor/tst_session_process_supervisor.cpp:262`, `:271`,
`:278`, and `:295`, `:299`), so the helper's self-quit is now six times the
budget it used to equal.

Product policy is unaffected. The helper is a test-only executable target
declared at `tests/session_supervisor/CMakeLists.txt:4-11` and handed to the
tests as `QINDAQT_SESSION_TOKEN_CHILD_HELPER` at `:34`; no product target
references it, and the supervisor's own `StopTimeoutMilliseconds` is unchanged.

No existing test regresses. I checked every test that starts the helper, since
"a test that waited for the old 5-second natural exit" was the obvious way this
could break:

- `launcherPassesASecretOnlyThroughTheDescriptor` (`:103-125`) was my main
  suspicion, because it does `waitForFinished(5'000)` at `:113`. It passes
  `--quick-exit`, so its helper still exits in 20 ms. Clean.
- `supervisorDeathTerminatesATokenizedChild` (`:158-176`) and
  `supervisorEndsSessionWhenShellRestartCannotStart` (`:308-341`) become more
  deterministic, not less: previously a slow run could let the host self-quit
  and change either the signal disposition or the finishing role out from under
  the assertion.
- `supervisorStartsBothChildrenAndCouplesTheirLifetime` (`:197-216`) still gets
  its `finished` from the shell path, because the shell helper exits in 20 ms
  under profile `test-shell-role`.
- `parentDeathTerminatesTheWitnessedSessionChild` uses the separate lifetime
  parent helper; `buildsTheExactNonSecretShellArguments` starts no process;
  `supervisorRollsBackWhenTheSecondChildCannotStart` and
  `supervisorDoesNotRestartShellAfterHostExit` use explicit teardown or an
  explicit kill.

No test can now hang or grow the suite's wall clock, because none waits for a
natural host exit, and `reapAfterParentDeath` is independently bounded at 500
attempts of 10 ms (`:67-82`), so a PDEATHSIG failure still fails within about
five seconds regardless of the longer child lifetime.

No test process is left alive. Every tokenized child gets
`PR_SET_PDEATHSIG, SIGKILL` bound to the launching process
(`src/session_supervisor/src/tokenized_process_launcher.cpp:54-61`), which the
kernel honours even if the test binary is killed rather than exiting cleanly;
`QProcess`'s destructor kills and waits for a still-running child; and
`SessionProcessSupervisor::stop()` performs the bounded TERM-then-KILL
sequence. The 30-second timer is therefore an upper bound that nothing reaches
in practice.

Two small observations, neither a defect: the `hold` marker is still meaningful
because it discriminates the shell role (20 ms against 30 s) even though both
non-shell branches are now equal; and no wiki page states the helper's
lifetime, so nothing became stale - the "within five seconds" sentence at
`docs/wiki/development/testing-harness.md:463` describes the Settings1
daemon-loss test, not this helper.

## Gates this PASS does not cover

Unchanged from my previous handoff, and no reading closes any of them. I ran no
compiler and executed no test, and I am not adopting your reported gate results
as my own evidence.

1. Compile and link at this exact state. The helper change is trivially safe to
   compile, but the unit-test change is Python and has not been executed here.
2. Your smallest invalidated Debug build and the two focused CTest rows
   (`qindaqt.session-supervisor`, `session.notification-live-driver-unit`).
   These are the two suites that actually execute both repairs; the N1 branch
   and every N3 timing claim above are behavioural predictions until they run.
3. Full current-source Debug build plus the 50-test notification/session/shell
   selector, then the full Release incremental rebuild, for fresh provenance.
4. Sanitizers, in the order I gave before: `qindaqt.session-supervisor` under
   ASan+UBSan first, then LSan on the same target, then the shell offscreen
   tier. N3 exists precisely so this gate is not misread as a lifecycle defect.
5. Staged package/install.
6. The private nested matrix: five named resolution/scale rows plus race-10x.

Requested next action: proceed to the compiler-owned focused gate. I remain
available for another exact-state rereview, including of the eventual candidate
commit, which will also retire my earlier N4 limitation about untracked paths.
