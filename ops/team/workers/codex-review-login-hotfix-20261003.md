# Codex login hotfix review — October 3

- Status: complete — accepted repaired candidate 1bd319fd9 with live qualification pending
- Base: 8f74faff5bd92bdfa1600cd0163b0e0b3f612fef
- Branch: hotfix/login-native-attachment (read-only reviewer)
- Worktree: `.cache/login-native-attachment`
- Paths: this record and own timestamped messages only; no product source ownership
- Outcome: independently review portal shortcut tokens, keyring fanout, compositor attachment and packaging path before Portage delivery

## Updates

- 2026-10-03T14:57:21Z — Reviewed exact candidate 8f74faff. Found a blocking GlobalShortcuts admission mismatch: xdg-desktop-portal 1.20.4 forwards optional `handle_token`, but adaptor requires it for CreateSession and BindShortcuts; the native bridge fixture also invokes empty maps. Sent precise source lines and frontend references to the implementer. Keyring's independently verified password fanout and 0755 compositor socket admission have no additional blocker so far. Rechecking the repaired commit next.
- 2026-10-03T14:59:17Z — Rechecked repaired exact commit 1bd319fd9. Optional request token and required session token match xdg-desktop-portal 1.20.4 forwarding; native bridge fixture now covers supplied and omitted handle tokens. Accepted source review. The local native bridge fixture remains unavailable, so live frontend and Portage fresh-login qualification are explicit delivery gates.
