# Native sleep modes source/static checkpoint

- Time: 2026-10-01T13:56:40+00:00
- Base: e21c1757d45ee4814734ad646a8c95b2c5b14b00
- State: candidate source for executable qualification, not accepted or integrated

Implemented bounded Suspend/Hibernate/HybridSleep/SuspendThenHibernate with matching zero-input boolean Can methods on versionless additive Sleep1. Actual session-bus caller UID is resolved for hints and actions. The selected supervisor connection remains the actual logind/polkit caller; no client PID impersonation is promised. Mode dispatch repeats exact Can only after current targeted Locked/Protected receipt, interactive=false; challenge/no/na/unknown/missing/malformed refuses. Cancellation retires exact transport request serial before another explicit mode, preserving uncertain/no-replay after dispatch. Protected callback and readonly completion lifetimes are fenced. No Power policy, Settings, composition, shell or compositor edits.

Installed qinda systemd261.2 distfile primary implementation was inspected directly: all four Can signatures ()→s and action signatures (b)→(), caller credential UID and polkit evaluation confirmed. ADR0325 and minimal index/nav approved by manager; proposed until candidate review.

Static commands: tools/validate-docs exit0 (484); mkdocs build --strict --site-dir build/native-sleep-modes-static/site exit0; tools/check-source-shape --root src/session/native_sleep --config tools/source-shape.json --warnings-as-errors exit0 (8 files, largest272); corresponding tests root exit0 (7 files, largest228); git diff --check exit0. Source-only evidence does not establish executable qualification.

Requested strict Debug/sharedON/pluginOFF build: qindaqt_logind_sleep_transport_tests qindaqt_sleep_coordinator_tests qindaqt_sleep_modes_tests qindaqt_native_lock_runtime_tests qindaqt_session_actions_tests qindaqt_qt_native_lock_request_tests qindaqt_session_supervisor_support. Unchanged -j24 -l24, ccache and qualified fork690 prefix; no installs. CTest regex qindaqt\.(logind_sleep_transport|sleep_coordinator|sleep_modes|session-native-lock-runtime|session-actions-client|session-actions-boundary|session-actions-boundary-poison|qt_native_lock_request)$, serialized private buses, unavailable system bus, fatal Qt warnings. Own modified fixtures use explicit empty activation configuration. Root grants compiler/private runtime before any run. All original seven rows plus new mode row required.

Expected focused mode cases cover each mode's exact pairing/protection, unsupported/challenge/no/malformed capabilities, actual denied caller UID, stale same-epoch reply/new request, loss before/after dispatch, timeout/malformed uncertainty, contradictory hint/action capability, delayed protection loss, forged nonce, unknown/Locking, duplicate/pending, descriptor closure and facade stop. Counts/results will come only from actual logs. This prerequisite alone does not complete PF2.
