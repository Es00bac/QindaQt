# S3 shell-readiness static red and contract decision

- Worker: Dorothy Vaughan
- Time: 2026-08-31T13:18:14-06:00
- Status: working
- Executable lane: released; no configure, compiler, CTest, private bus, or
  nested runtime was used

Manager static replay passed `git diff --check`, then
`PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p
'test_desktop_session_*_unit.py'` ran 106 tests and returned 105/106. The sole
red was `test_readiness_polls_past_pending_shell_privacy`: its legacy pending
fixture had a `privacy-denied` failure but omitted the normalized evidence now
required for a valid Snapshot observation.

The repair contract is explicit: retryable `privacy-denied`,
`center-window-missing`, and `center-output-pending` states must carry exact,
validated normalized shell evidence. Only whitelisted cold topology,
service-owner, or owned-service/object-not-ready gaps may omit evidence. I am
repairing the fixture and finishing hostile pending-code, dock-authority, shell
state, and activation mutations before repeating the direct static unit gate.
