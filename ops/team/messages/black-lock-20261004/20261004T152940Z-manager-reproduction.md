# Black lock-screen reproduction and current boundary

- Observed host: qinda-top, 2026-10-04. Native lock state was locked/protected; no qindaqt-lock process existed.
- Installed packages: gui-wm/qindaqt-kwin-6.6.6_p1-r5; gui-wm/qindaqt-desktop-0.1.0_pre20261002-r12.
- Direct installed-KF6Service probe compiled and exited 0: X-QindaQt-KWin-Wayland-Interfaces returned singleton `ext_session_lock_manager_v1;`; expected permission membership was false. Private RequestLock retry returned false. Root-owned executable/metadata/parent modes and Yama ptrace_scope=1 passed read-only checks.
- User explicitly authorized a full desktop restart. Saved prior session log only under ignored user cache; restarted SDDM, verified old compositor exited and fresh tty7 native Lock1 state is unlocked.
- Fork implementation base: 55d1f2738723316fae4686468757aef66ce7590b; worktree /home/cabewse/work_space/qindaqt-kwin-lock-permissions; branch fix/native-permission-lists-20261004.
- Parser ownership: shared serviceutils readers, native locker launcher, focused tests and primary fork docs. No installed-file edits or live lock tests authorized for the background implementer. Root owns packaging, wiki/source-pin integration, physical-session coordination and frequent speech.
- QindaMail additional report: KDE Wallet appears despite org.freedesktop.secrets being owned by qindaqt-keyring. Credential backend selection diagnosis is pending. No credential values have been inspected.
