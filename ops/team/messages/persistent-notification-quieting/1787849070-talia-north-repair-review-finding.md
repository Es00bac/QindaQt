# P1: repaired controller still presents stale Saving/Conflict and erases failed-save diagnostics

- **Timestamp:** 2026-08-27T10:44:30-06:00
- **Exact candidate:** `55105b2c565f25f0582303e4936bcd288b04ffdb`
- **Severity:** P1 — blocks integration

Fresh inspection of the repaired exact hash confirms two related UI truth and
recovery failures in the production DND controller.

First, `DoNotDisturbController::handleClientState()` returns without consuming
any client transition while its presentation state is `Saving` or `Conflict`
at `src/services/settings_client/src/do_not_disturb_controller.cpp:79-83`.
If the exact owner disappears after an Applied commit reply but before the
required refresh snapshot, `SettingsClient::handleOwnerChanged()` publishes
`Unavailable` at `src/services/settings_client/src/settings_client.cpp:235-258`,
but the write is already no longer in flight, so no later `commitUncertain`
signal repairs the controller. The page remains indefinitely “Saving…” with no
Retry action. An owner loss while Conflict similarly leaves a visible “Apply my
choice” action that returns false while the client is not Ready, with no honest
Unavailable/Retry state. This violates the task's pending/conflict/unavailable
truth and explicit-recovery requirement.

Second, every confirmed non-Applied outcome is set as a Ready-state error at
`do_not_disturb_controller.cpp:127-148`, but `SettingsClient::handleCommit()`
immediately publishes Authenticating and forces an authority refresh at
`settings_client.cpp:351-356`. The controller then transitions through
Unavailable and the successful snapshot clears the diagnostic through
`do_not_disturb_controller.cpp:99-124`. A PersistenceFailed save therefore
ends in ordinary Ready with an empty error, so neither production surface
retains the fact that the user's attempted save failed. The shell surface does
not render `errorText` independently at all; the settings application does, but
the refresh clears it before the stable post-commit state.

Repair should distinguish an accepted commit awaiting refresh from a write
still in flight, always let owner/client loss dominate Saving and Conflict,
offer Retry while authority is unavailable, and preserve a confirmed rejection
diagnostic across the authoritative refresh until a user action/new write (or
another explicit dismissal contract) clears it. Add controller-level races for
owner loss after the commit reply but before snapshot, owner loss while
Conflict, and PersistenceFailed followed by a successful refresh; then project
those stable states through both offscreen settings and shell controls.

The original seven focused private-bus/protocol/client/bridge tests independently
pass at this hash, but none covers these sequences. Review continues for the
recursive Object null/unsigned-integer wire/save/restart concern and the rest of
the repair matrix. No candidate source or live desktop/session input was used.
