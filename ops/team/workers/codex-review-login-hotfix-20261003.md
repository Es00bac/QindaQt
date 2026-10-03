# Codex login hotfix review — October 3

- Status: complete — accepted media-trigger candidate 84d90a097 with live activation pending
- Base: 8f74faff5bd92bdfa1600cd0163b0e0b3f612fef
- Branch: hotfix/login-native-attachment (read-only reviewer)
- Worktree: `.cache/login-native-attachment`
- Paths: this record and own timestamped messages only; no product source ownership
- Outcome: independently review login/native-service repair and Gabbee media-trigger compatibility before Portage delivery

## Updates

- 2026-10-03T14:57:21Z — Reviewed exact candidate 8f74faff. Found a blocking GlobalShortcuts admission mismatch: xdg-desktop-portal 1.20.4 forwards optional `handle_token`, but adaptor requires it for CreateSession and BindShortcuts; the native bridge fixture also invokes empty maps. Sent precise source lines and frontend references to the implementer. Keyring's independently verified password fanout and 0755 compositor socket admission have no additional blocker so far. Rechecking the repaired commit next.
- 2026-10-03T14:59:17Z — Rechecked repaired exact commit 1bd319fd9. Optional request token and required session token match xdg-desktop-portal 1.20.4 forwarding; native bridge fixture now covers supplied and omitted handle tokens. Accepted source review. The local native bridge fixture remains unavailable, so live frontend and Portage fresh-login qualification are explicit delivery gates.
- 2026-10-03T15:10:55Z — Claimed follow-on review of exact candidate 57ebc24f8. The portal maps Gabbee's `XF86AudioPrev` to Qt's `Media Previous` before consent and Shortcuts1 registration; focused policy coverage checks the parsed key. Asked implementer to narrow wiki wording because the table intentionally supports a subset of XF86Audio names.
- 2026-10-03T15:13:06Z — Rechecked exact revised candidate 84d90a097 and accepted source review. qindaqt-kwin maps `XKB_KEY_XF86AudioPrev` to `Qt::Key_MediaPrevious`, and native transport keeps Qt portable sequence text. A live read-only conflict query found an existing Gabbee dictation binding on `Media Previous`; package r5 must still prove live portal binding and key activation against that session state.
