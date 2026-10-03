# Codex login hotfix review — October 3

- Status: complete — accepted XDG menu package and repaired ADR navigation
- Base: 8f74faff5bd92bdfa1600cd0163b0e0b3f612fef
- Branch: hotfix/login-native-attachment (read-only reviewer)
- Worktree: `.cache/login-native-attachment`
- Paths: this record and own timestamped messages only; no product source ownership
- Outcome: independently review login/native-service, Gabbee media-trigger and Screenshot XDG menu delivery before integration

## Updates

- 2026-10-03T14:57:21Z — Reviewed exact candidate 8f74faff. Found a blocking GlobalShortcuts admission mismatch: xdg-desktop-portal 1.20.4 forwards optional `handle_token`, but adaptor requires it for CreateSession and BindShortcuts; the native bridge fixture also invokes empty maps. Sent precise source lines and frontend references to the implementer. Keyring's independently verified password fanout and 0755 compositor socket admission have no additional blocker so far. Rechecking the repaired commit next.
- 2026-10-03T14:59:17Z — Rechecked repaired exact commit 1bd319fd9. Optional request token and required session token match xdg-desktop-portal 1.20.4 forwarding; native bridge fixture now covers supplied and omitted handle tokens. Accepted source review. The local native bridge fixture remains unavailable, so live frontend and Portage fresh-login qualification are explicit delivery gates.
- 2026-10-03T15:10:55Z — Claimed follow-on review of exact candidate 57ebc24f8. The portal maps Gabbee's `XF86AudioPrev` to Qt's `Media Previous` before consent and Shortcuts1 registration; focused policy coverage checks the parsed key. Asked implementer to narrow wiki wording because the table intentionally supports a subset of XF86Audio names.
- 2026-10-03T15:13:06Z — Rechecked exact revised candidate 84d90a097 and accepted source review. qindaqt-kwin maps `XKB_KEY_XF86AudioPrev` to `Qt::Key_MediaPrevious`, and native transport keeps Qt portable sequence text. A live read-only conflict query found an existing Gabbee dictation binding on `Media Previous`; package r5 must still prove live portal binding and key activation against that session state.
- 2026-10-03T15:35:45Z — Claimed review of QindaGentoo hub commit b1adea2 and QindaQt branch commit 4b6d2b36c. The new Portage package is the sole installed owner of laptop `/etc/xdg/menus/applications.menu`; qinda has no existing generic menu owner. The active laptop XML matches the package and follows explicit promotion past a config-protected dangling Plasma symlink. Found a documentation index omission: ADR-0344 is in mkdocs.yml and Screenshot but absent from `docs/wiki/adr/index.md`; implementer is repairing it before acceptance. Live KService and PNG results remain implementer-provided evidence, not independently reproduced in this read-only review.
- 2026-10-03T15:39:30Z — Rechecked repaired QindaQt commit 6200920cd: ADR-0344 now appears in the numeric ADR index, MkDocs navigation and Screenshot page. Accepted source/package review of QindaGentoo b1adea2 and QindaQt 6200920cd with no remaining blocker. The reported 129 KService entries and successful PNG capture are live implementer evidence; reviewer did not rerun the physical capture. qinda package installation and final machine qualification remain delivery gates.
