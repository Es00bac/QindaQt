# Codex login repair — October 3

- Status: working — restore physical login and audit native services on qinda-top
- Base: efc5eb497c5e3592f2207589151c5831de7740a3
- Branch: hotfix/login-native-attachment
- Worktree: `.cache/login-native-attachment`
- Paths: compositor attachment, portal, audio, keyring, power and focused tests/wiki; this record and own messages
- Outcome: native QindaQt login stays running; installed services, keyring, audio and power work after fresh login

## Updates

- 2026-10-03T13:54:24Z — Claimed exact-base isolated repair. qinda-top's SDDM accepts credentials, then `qindaqt-session` exits 2 because the native lock's ordinary compositor attachment fails. A private virtual run of installed `qindaqt-kwin` produces socket mode 0755; the shared attachment rejects any other-user bit. Changed it to reject group/other write while retaining 0700 runtime, owner, actual peer PID/PIDFD and exact bus lineage. Focused fixture now matches real 0755 and rejects a writable socket. Build and package verification active; no physical session mutation yet.
- 2026-10-03T14:39:32Z — Physical session 9 on VT7 is running with private temporary login wrapper. Audio1 responds after runtime TasksMax 128; source unit raised from 16 to 128. Portal backend active with a live environment override; source canonicalizes its theme root. Keyring user service/socket were installed under lib64 outside the manager search path; source path corrected. One-password follow-on unlock for independently authenticated matching collections is under private-fixture build. Installed Power1 crashes; a fresh source build stays alive under the native arguments, so a Portage rebuild is the next qualification gate. Current session remains available, and no fresh-login qualification has yet been claimed.
