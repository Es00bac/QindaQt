# L5 — Lock greeter and lock PAM (container-wm) — Sonnet

Base: container-wm hub `main` at the commit named in the manager's assignment. Branch `stability/l5-greeter-20261016`.
Worktree: `~/work_SPaC3/container-wm.worktrees/stab-l5-greeter-20261016`. Voice seat: `claude-helper-five` (create if missing).

## Why
- **Dead prompt after 90 s.** A PAM attempt starts the moment the screen locks and has a 90 s deadline
  (`src/lock_worker_client/worker_process.h:47`, `src/lock_authentication/worker_main.cpp:37`). After that the
  greeter shows "Authentication unavailable", hides and disables the password field, and Enter does nothing
  until the Unlock button is clicked (`src/lock_greeter/Main.qml` 10–21, 53–58, 73;
  `authentication_controller.cpp` 109–126). Keyboard-only unlock looks frozen. Each timeout also counts toward
  pam_faillock (3 in 15 min locks the account for 10 min).
- **Greeter exits leave a black screen.** The greeter process can exit while locked, which leaves the screen
  black (the compositor-side relaunch is lane L4):
  - `qFatal` on a 0×0 configure (`src/lock_protocol/shell_integration.cpp` 160–161);
  - exit 2 on a QML or role failure (`src/lock_greeter/native_windows.cpp` 15–25);
  - exit on a failed `screenAdded`.
- **Keyring stays locked.** The keyring is never unlocked from the lock screen: `pam_qindaqt_keyring` calls
  systemd `GetUnit` with the service name as the interface (`src/services/keyring/pam/system_owner.cpp` 13,
  90, 110), and the system bus policy rejects it. The pam_unix "setuid failed" line is debug noise and needs no
  change.
- Findings items 7 (greeter side), 11 and 23.

## Owned paths
- `src/lock_greeter/**`, `src/lock_authentication/**`, `src/lock_worker_client/**`, `src/lock_protocol/**`,
  `src/authentication_overlay/**` (only if needed)
- `src/services/keyring/pam/**`
- tests and wiki pages for these (`docs/wiki/architecture/native-session-lock.md` is shared with L1; coordinate
  edits through clearly separated sections)

## Outcomes
1. **Authentication starts on input.**
   - Begin a PAM conversation on the first key press, click or touch, not at lock time.
   - When an attempt fails or times out, return to the ready state automatically with the field visible and
     focused, and start a fresh attempt on the next input.
   - Enter always submits, and the field is never left hidden or disabled while locked.
   - No PAM `authenticate` call happens without user input, so idle locks add nothing to faillock.
2. **The greeter never exits on recoverable conditions.**
   - Treat a 0×0 configure as "wait for the next configure".
   - A failed per-output window creation retries or skips that output while the others stay locked.
   - Log every exit reason.
3. **Keyring unlock from the lock screen.** Use interface `org.freedesktop.systemd1.Manager` for `GetUnit`, and
   verify the call is allowed under the system bus policy. A test asserts the exact message (destination, path,
   interface and member).

## Acceptance
- An authentication-controller state-machine test: idle → input → attempt → timeout → ready again with the
  field enabled; Enter submits.
- A test that no attempt starts without input.
- A shell-integration test for the 0×0 configure.
- The PAM keyring message test.
- Focused build and ctest. Update the native-session-lock wiki section and the greeter page, then run strict
  mkdocs and the link checker.

## Reference designs (study, don't depend)
- KDE KScreenLocker greeter: authentication starts on interaction.
- GNOME Shell unlock dialog: a fresh PAM conversation after each failure.
