# High: required shell-reconstruction persistence proof is absent

Candidate `00b3d49ac3d7ba94edcf10272fa5e61185d63b56` does not supply the
shell-restart evidence required by `docs/TASK_LIST.md:31-33` and the accepted
manager interpretation in `1787796417-manager-boundary-decision.md`. The real
transport test commits DND, replaces the resident service, and proves the
existing clients rebaseline from disk
(`tests/services/settings_client/tst_qt_settings_transport.cpp:50-97`). The
bridge test, however, keeps one `SettingsClient`, one controller, and one bridge
alive while injecting owner changes
(`tests/shell/tst_notificationquietingsettingsbridge.cpp:55-90`). It never
destroys and reconstructs the shell-side consumer while a settings service
continues running, nor reconstructs both sides from the persisted file.

The settings-app QML test is projection-only and similarly never performs a
real save/close/reopen. The service lifecycle test only releases/reacquires the
name without a persisted transaction. Therefore the passing focused/full
registry cannot prove the explicit “committed choice survives a complete
settings-service and shell restart” outcome; source plausibility is not a
substitute for the required executable evidence.

Repair should add a safe private-bus/process or composition-harness scenario
that commits through Settings1, tears down and reconstructs the shell-side
client/controller/bridge while the service remains, then reconstructs the
service from the same isolated XDG file and reconstructs the shell consumer
again. It must assert the policy starts fail-quiet, changes only after each
fresh exact-owner baseline, and restores the committed value without replay.
An app save/reopen scenario should exercise the same ordinary-controller path
or the task/wiki must stop claiming it was tested.
