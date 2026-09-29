# Lane PK (Sonnet): native QindaQt polkit agent (plan slice PF15)

- Worker name `claude-polkit-agent`, voice seat `claude-helper-three`, speech name "polkit agent".
  Speak only when you start and at handoff.
- Worktree: `cd ~/work_SPaC3/container-wm && git fetch hub && git worktree add ~/work_SPaC3/container-wm.worktrees/polkit-agent -b feature/qindaqt-polkit-agent hub/main`.
  Push the branch to `hub`.
- ADR number reserved: **0290**. The plan is `docs/plans/2026-09-28-plasma-free-qindaqt.md` §3.4 on hub
  branch `plan/plasma-free`; read §3.4 and its "Guiding principle".

**Live defect this fixes.** Two agents register at every login:
- the supervisor starts the KDE agent (`src/session_supervisor/src/polkit_agent_selection.cpp`);
- XDG autostart runs polkit-gnome (`NotShowIn=MATE;KDE`).

polkitd shows a different winner each session.

**Owner principle.** This is a native QindaQt component designed for QindaQt, not a port of the KDE
agent. It uses QindaTK controls and QindaQt tokens, so it follows the theme, including dark mode. It
lives in the QindaQt session, which supervises it.

## Build

1. **Agent (`src/apps/polkit_agent`, installed as `${CMAKE_INSTALL_FULL_LIBEXECDIR}/qindaqt-polkit-agent`).**
   - Use polkit-qt6: `PolkitQt1::Agent::Listener` and `Session`, from `find_package(PolkitQt6-1)`. It is
     installed on qinda as `sys-auth/polkit-qt-0.201.1`; check the exact CMake package name under
     `/usr/lib64/cmake`.
   - Register for `PolkitQt1::UnixSessionSubject(getpid())` at object path `/org/qindaqt/PolkitAgent`.
   - Registration failure (another agent already registered) is a clear stderr line and exit code 2,
     never a crash loop. The supervisor's restart policy must not spin on it; check how it treats the
     exit code and document it.
   - One request at a time. A second `initiateAuthentication` while one is open is queued, not dropped.
   - `cancelAuthentication` from polkitd closes the dialog and completes the request.
2. **Session logic (pure C++, unit-tested, no polkit types in its interface).**
   - Identity choice: prefer the current user when polkit lists them, else the first identity. Show
     "Full Name (login)" from passwd and "Administrator (root)" for uid 0.
   - Attempts: a wrong password shows "That password didn't work. Try again.", clears and refocuses
     the field, and starts a new Session. Cancel completes with `gainedAuthorization=false`.
   - PAM `request` prompts that are not echo-off (for example an OTP), `showInfo` (for example
     "Place your finger on the reader") and `showError` appear in the dialog.
   - Requester display: polkit's `details` carry the subject; resolve its pid through
     `/proc/<pid>/exe` and `comm`. Take the app name and icon from a matching `.desktop` file by
     `Exec` basename where cheap, else show the program path.
3. **Dialog (QML, `import QindaQt.Controls 1.0 as QQ`, `import QindaQt.Tokens 1.0`).** Follow
   `src/apps/welcome/main.cpp` for import paths and the application appearance controller.
   - Present it as a **layer-shell overlay** on the output under the pointer, with a token-coloured
     scrim over that output and `KeyboardInteractivityExclusive`, so keystrokes cannot land in
     another window. See `src/shell_surface/include/qindaqt/shell_surface/layer_shell_surface_backend.h`
     and the LayerShellQt use in `src/shell_surface/src/layer_shell_notification_surface.cpp`. Link
     that module through its public header only.
   - Contents:
     - title "Authentication required";
     - polkit's own `message`;
     - the requesting app's icon and name;
     - the identity chooser, shown only when there is more than one identity;
     - the password field, with an accessible name and focused on open;
     - an info/error line;
     - a "Details" disclosure with the action id, vendor and program path;
     - Cancel and Authenticate buttons.
   - Enter authenticates and Escape cancels. The layout is touch-sized, and the OSK appears through
     the normal text-input path.
   - Guard the password: it is never logged, never put in a model that outlives the attempt, and
     cleared from the field after every attempt.
4. **Single-agent rule.**
   - `defaultPolkitAgentCandidates()` becomes only the QindaQt agent's install path, with no KDE
     fallback (owner decision). Keep `--polkit-agent` and `--no-polkit-agent`.
   - `src/session_autostart` (ADR-0247) skips XDG autostart entries that are polkit agents while
     the session runs its own agent. Match the `Exec`/`TryExec` basename against a small documented
     table: polkit-gnome-authentication-agent-1, polkit-kde-authentication-agent-1,
     lxqt-policykit-agent, polkit-mate-authentication-agent-1, xfce-polkit, lxpolkit.
   - Log one line per skipped entry. Do not change any other entry's behaviour.
5. **Docs.**
   - ADR-0290: native agent, overlay presentation, single-agent rule. Amend ADR-0100's
     polkit-agent paragraph and ADR-0247 with a dated section each.
   - Add an app page under `docs/wiki/`, following how other apps' pages sit in `mkdocs.yml`.
   - Run `./tools/validate-docs` and `mkdocs build --strict`.
   - **Do not edit ebuilds or the overlay.** The manager handles packaging: the RDEPEND swap to
     `sys-auth/polkit-qt`, dropping `polkit-kde-agent`, and profile USE. Put the exact packaging lines
     you recommend in your report.

## Tests (targeted only; no full suite)

- Unit:
  - identity choice;
  - attempt, retry, cancel and cancel-from-polkit transitions;
  - request queueing;
  - requester resolution against a fake `/proc` root and applications directory;
  - candidate selection (default, configured, disabled);
  - the autostart skip table, where a polkit-gnome entry with `NotShowIn=MATE;KDE` is skipped and an
    unrelated entry still runs.
- An offscreen QML test of the dialog:
  - accessible names;
  - Enter and Escape;
  - the error line;
  - the identity chooser hidden for one identity;
  - the password cleared after a failed attempt.
- Build only your targets on qinda with `-j8 -l24`. Run
  `ctest -R "polkit|session-autostart|session-supervisor"`, adjusting the pattern to the real test
  names.
- **Never** touch the live session: no registering against the real polkitd, and no service
  restarts. List the live check for the manager (`pkexec true` in a QindaQt session, then a wrong
  password, then the right one).

## Report

- Commits, files and test counts, each with its exit status.
- The recommended packaging lines.
- Anything the owner must decide.

Keep token use low. Read only what you need.
