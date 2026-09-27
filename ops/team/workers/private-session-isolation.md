# Private session isolation

- Status: awaiting review — candidate built and verified; manager owns deployment
- Base: d3b646f2b39665fd65b124c879493b2baf36f656
- Branch: fix/private-session-host-isolation
- Paths: src/session_supervisor; tests/session_supervisor; compositor-session wiki and ADR; own ops records

## Updates

- 2026-09-27T01:09:12.902436+00:00: Claimed root cause repair in isolated worktree; unrelated website changes preserved.
- 2026-09-27T01:12:58.100591+00:00: Material finding: a separate broker never isolated native systemd/private, and inherited host broker also needed protection. Private scope now skips both publication and restart; focused build in progress.
- 2026-09-27T01:14:10.497246+00:00: Verification passed: focused build exit 0; 3 CTest gates / 30 QtTest results including setup/cleanup, 0 failures; 416-document link check exit 0; strict MkDocs exit 0. Offering help with any review findings.
