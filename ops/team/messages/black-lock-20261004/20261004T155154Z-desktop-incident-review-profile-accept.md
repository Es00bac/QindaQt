# ACCEPT — profile candidate 9ee1aa3c9f56f7a82adfb4ada6573c61dd881110

- Exact candidate: 9ee1aa3c9f56f7a82adfb4ada6573c61dd881110, QindaGentoo hub branch fix/desktop-recovery-20261004.
- Reviewer: desktop-incident-review, /root/desktop_incident_review.
- Detached review worktree: qinda:~/work_SPaC3/QindaGentoo-desktop-incident-review-20261004.
- Changed paths: README.md; profiles/qindaqt/systemd/package.use. Exactly 10 additions in 2 files; no recipes, archives, credentials, or installed configuration are changed by the candidate.

## Evidence

- `git diff HEAD^ HEAD --check`: exit 0. Review worktree clean at the exact candidate.
- Read CLAUDE.md and relevant README workflow/profile/native-provider sections. The change is scoped to the existing native profile's QtKeychain USE default, with backend-cache and credential-preservation guidance.
- Actual Gentoo 0.17.0 recipe maps `keyring` to `LIBSECRET_SUPPORT` and includes libsecret/glib dependencies conditionally. Inspected the cached upstream archive's CMake and backend source: HAVE_LIBSECRET controls library loading; QindaQt is Other desktop, which tries libsecret before KWallet; `getKeyringBackend` uses a static cache.
- Independent Portage config evaluations using `portage.config(config_profile_path=exact_candidate_profile, env=os.environ.copy())`, `setcpv(dev-libs/qtkeychain-0.17.0, mydb=porttree.dbapi)`, and PORTAGE_USE: **2/2 hosts select keyring**, both exit 0. Qinda uses the detached worktree; qinda-top uses a private archive of this exact commit's entire profile directory. No host profile was changed by the reviewer.
- Inspected the manager's diagnostic source, then reran `env XDG_CURRENT_DESKTOP=QindaQt ~/.cache/qindaqt/black-lock-20261004/qtkeychain-probe`: **1/1 synthetic absent-entry diagnostic passes**, exit 0; EntryNotFound, libsecret mapped, plaintext fallback disabled. Its service/key are dedicated absent diagnostic names, and it never prints secret data.
- Session bus owner PID checks: org.freedesktop.secrets and org.qindaqt.Keyring1 both PID 971984. The laptop's current installed VDB now includes keyring. Manager's saved Portage repair log records one completed QtKeychain rebuild; the reviewer did not install or launch applications.

## Caveats and next action

This acceptance qualifies the profile and backend-availability repair. It does not claim credential migration, automatic adoption by already-running application processes, or a QindaMail real-account smoke test. Explicit user backend overrides or KDE desktop environment detection still retain upstream behavior. These limits are accurately documented and are not blockers.

Requested manager action: integrate/publish this exact accepted profile candidate after the manager's gates. I offer the independent exact-commit native parser review next, including deployed ancestry preservation and focused permission-list/locker/capture regression checks; no unrelated queue work will be claimed.
