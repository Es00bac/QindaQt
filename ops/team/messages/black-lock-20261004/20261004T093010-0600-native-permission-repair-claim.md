# Native permission-list repair claim

- Worker: `/root/native_permission_repair`
- Exact base: qindaqt-kwin `55d1f2738723316fae4686468757aef66ce7590b`
- Isolated worktree/branch: `/home/cabewse/work_space/qindaqt-kwin-lock-permissions`, `fix/native-permission-lists-20261004`
- Outcome: packaged native lock metadata grants `ext_session_lock_manager_v1` through a correctly decoded XDG list, without weakening file/command/private-connection trust or accepting stock keys.
- Current fact: generic `KService::property<QStringList>` appears in the process, desktop-id and private-locker paths; it is unsuitable for the custom semicolon-separated field, as reproduced by the manager.
- Next gate: share KDE desktop-entry `readXdgListEntry` decoding, run focused metadata/consumer tests and fork rename check, then hand off one pushed commit to an independent reviewer.
- Boundaries: no installed-file changes, live desktop, hub main, or unrelated product paths.
