# ACCEPT — be99a1b5f84fc60a6469c8e76cd5f6240250800b

- Candidate: `be99a1b5f84fc60a6469c8e76cd5f6240250800b` (Gabbee).
- Base: `c6bd2af`.
- Reviewer: post-reboot-radio-review-codex-20261017, different from implementer.
- Blocking defects: none.

The native probe queries Session paths at a captured unique backend owner,
requires the exact provider-PID frontend path at both ends, and rechecks that
backend ownership before success. A missing owner cannot produce success.
Stock KDE retains its existing frontend-only probe. The change does not alter
the idle-only provider restart, recording/delivery deferral, generation/circuit
bounds, PID safeguards, or recovery actions. No private shortcut binding IDs
or dictation content are queried by the new logic.

Independent checks from sibling
`gabbee/.cache/reboot-shortcut-liveness-review-20261017`:

- `PYTHONPATH=src QT_QPA_PLATFORM=offscreen pytest -q tests/test_voice_watchdog.py tests/test_voice_watchdog_shortcut_session.py tests/test_recovery_reports.py tests/test_desktop_recovery.py`: exit 0, 69 passed in 0.56 seconds.
- Execute baseline `c6bd2af` and candidate probe methods in memory against the same orphan frontend/current-native-session-absent fixture: exit 0. Baseline incorrectly reports healthy=True; candidate reports healthy=False.
- `git diff --check c6bd2af be99a1b5f84fc60a6469c8e76cd5f6240250800b`: exit 0.

Bounded caveat: the new documentation's phrase "after suspend" should not be
treated as proof that suspend caused the observed loss; the proven repair
boundary is native Session/authority loss. Physical-key/text-delivery and
installed Portage verification remain next, as the candidate itself states.

Requested next action: preserve/integrate the exact accepted candidate and
proceed with the pinned recipe review/delivery. The reviewer is immediately
reviewing QindaGentoo `8dfc954ccbf29514b2433c3bb54fddfb35e31263` next.
