# Lyra Voss anchor correction to Soren Pike

- Worker: Lyra Voss (Anthropic, `claude-opus-5`, reasoning xhigh)
- To: Soren Pike, lead/keeper, Notification Live qualification
- Timestamp: 2026-08-27T18:08-06:00
- Corrects: two line ranges in
  `1787875314-lyra-voss-cpp-lifecycle-finding.md`. No finding, severity, or
  repair changes.

Both are in `src/session_supervisor/src/session_process_supervisor.cpp`.

- **F4** - the restart decision is at `:193-205` inside `childFinished`, which
  begins at `:186`. I wrote `:181-196`. The condition to amend is still the
  `role == ChildRole::Shell && m_shellRestartCount < ShellRestartLimit` test at
  `:193-194`; add the `m_host.state() != QProcess::NotRunning` term there.
- **F8** - the `stop()` body spans `:108-121`. The token reset the marker
  describes is at `:115`; the `m_shellRestartCount = 0` the marker mismatches
  is at `:120`, after both `stopChild` calls. I wrote `:110-118`, which omitted
  the line the finding is about.

Every other anchor in that message I have re-verified against the current
working tree, including
`notificationlivesettingsphases.cpp:143`,
`test_notification_live_nested.py:342`,
`notificationwindowcontroller.cpp:79-84`,
`notificationliveworkflow.cpp:204-208`,
`shelldevelopmentevidence.cpp:398-427`, and
`NotificationCard.qml:77-89`.

Separately, for your provenance bookkeeping: the candidate surface is now 70
paths, not 69. `tests/shell/qml/tst_notificationquietingcontrols.qml` became
modified during my read (38 tracked modifications, 32 new files). I did not
touch it or anything else in that worktree; HEAD is still
`c4982697858c083828bd406f1aa56c4e942bcc10`. None of my anchors are in that
file, so no finding depends on it.
