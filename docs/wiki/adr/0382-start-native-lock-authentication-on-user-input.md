# ADR-0382: Start native lock authentication on user input

- **Status:** Accepted
- **Date:** 2026-10-16
- **Owners:** Native locker, lock authentication
- **Supersedes:** None
- **Builds on:** [ADR-0299](0299-native-locker-private-launch-and-authentication.md),
  [ADR-0349](0349-package-native-lock-authentication-policy.md)
- **Superseded by:** None

## Context

The 2026-10-16 stability audit and the live journal showed three failures of the
locked screen. See [Lock greeter lifecycle](../architecture/lock-greeter.md).

- **A dead prompt.** The greeter began a PAM attempt the moment the screen
  locked. The worker client killed it at a 90 s deadline, after which the
  greeter hid and disabled the password field and Enter did nothing until the
  Unlock button was clicked. Keyboard-only unlock looked frozen.
- **Phantom failed logins.** The installed `system-auth` runs
  `pam_faillock preauth`, `pam_unix` with `default=bad`, then
  `pam_faillock authfail`. A conversation that fails inside `pam_authenticate`
  therefore counts as a failed login, and three in 15 minutes lock the account
  for 10 minutes. The live journal holds `pam_unix: conversation failed` and
  `auth could not identify password` lines written by `qindaqt-lock-pam` at
  unattended lock times (October 15 at 01:10, 04:39 and 07:54), so an attempt
  nobody typed into did reach the PAM failure path. The cause is a race between
  two nominally equal 90 s deadlines: the worker's own channel deadline starts a
  few milliseconds after the client's coarse Qt timer and is free to win, and
  the cancel path closed the channel before it killed the child.
- **A black screen when the locker ends.** The locker exited on a 0x0
  configure (`qFatal`), on a QML or role failure (exit 2) and on a failed
  `screenAdded`. The compositor keeps the session protected black, but nothing
  was left to unlock it. Relaunch is a compositor-side concern (lane L4); the
  locker must at least not end on conditions that pass.

## Decision

1. **No attempt without input.** The screen locks into a ready state with the
   credential field visible, enabled and focused and no PAM conversation. The
   first key press (except Escape), mouse click or touch starts one. Pointer
   motion does not. An explicit submit also starts one. `pam_authenticate` is
   never called without a person's action, so an idle lock adds nothing to
   `pam_faillock`.
2. **The field is never hidden or disabled while locked.** Characters typed
   before the first prompt arrives stay in the field and answer that prompt;
   Enter before the prompt is held and sent at the first secret prompt; Enter
   while a response is being verified keeps the typed-ahead text for the next
   attempt. An empty password is never sent (it cannot authenticate, and would
   only count as a failed login).
3. **Every ending but an authenticated unlock returns to the ready state by
   itself.** A failure, timeout or cancellation sets a status message and leaves
   the field intact; nothing restarts an attempt without input. An attempt that
   ended before any prompt (locked-out account, deny-only policy, worker that
   cannot start) is not retried by plain key presses, so key mashing cannot
   spawn workers; Enter retries it.
4. **Deadline ordering protects `pam_faillock`.** A new outcome,
   `Outcome::TimedOut`, is minted only by the greeter-side worker client when
   its 90 s whole-attempt deadline fires (precise timer). The worker's own
   backstop deadline is 105 s and compile-time checked to stay longer. The
   client always `SIGKILL`s the child before closing its end of the channel, so
   a cancelled, timed-out or abandoned attempt dies while still blocked in the
   conversation and no PAM module observes a failure.
5. **The greeter ends only when the lock ends.** A 0x0 or oversize configure is
   skipped (not acknowledged, not applied). A window that cannot be created is
   retried per output with bounded exponential backoff (250 ms doubling to
   30 s, unlimited) while the other outputs keep theirs. No usable output yet
   means waiting. A lock role handed to a window that dies unshown is released
   so a retry can succeed. A role-free observer accessor lets the locker see
   `finished` even when no window got a role, and a lock already finished at
   start-up is an exit. Every end states its reason in the journal.
6. **Private fixture contract.** `NATIVE_GREETER_UI_READY` means locked with a
   visible, enabled field (no longer a pending prompt); `NATIVE_GREETER_PROMPT`
   still means a prompt is waiting. The fixture gains `input` and `key`
   commands that stand in for a person.

## Consequences

- The nested compositor scenario `native_lock_greeter_test.cpp` in the fork
  currently waits for `NATIVE_GREETER_PROMPT` straight after the lock. It must
  first send `input` (or `key`); the gathered-scene test needs no change.
- A wrong password still counts once, as for any login. Abandoned, cancelled
  and timed-out attempts no longer do.
- The PAM service, worker wire format (digits 0-4) and unlock authority are
  unchanged. `TimedOut` never travels over the worker channel; a frame carrying
  it is rejected.
- Authentication devices that act before a password (fingerprint) begin when the
  person first touches the screen, not at lock time.
- Tests: deterministic controller state-machine and retry tests with fake ports,
  a real-QML behavior test, and a real Qt Wayland client plus the real
  integration plugin against a fake `ext-session-lock` compositor, each shown to
  fail against the previous code.

## Revisit when

- Presence-based factors need a PAM conversation before any input: add a
  presence trigger beside the input trigger rather than starting at lock time.
- PAM moves faillock accounting off conversation failure.
- The compositor-side locker supervision changes what a lingering or exiting
  locker means.
