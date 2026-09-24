# Audio1 session owner refresh independent review

Reviewed exact candidate 99b2fac10e4fe181473912625ae42677e36068ff against base d9cd43f570b670cf29bb0e97a7a3117316ccd71f and manager main 8c639c1e38ca445f7d8a17430f6b2a510ffc6ced.

Verdict: blocked on documentation/ADR compatibility. The implementation path is otherwise sound for its stated best-effort behavior.

Code and test review:
- The production list adds only qindaqt-audio-service.service, first, ahead of desktop consumers. qindaqt-session invokes refreshResidentServices after activation-environment publication and before SessionProcessSupervisor starts consumers.
- RestartUnit is an enqueue request, not a completion guarantee. The code captures the prior org.qindaqt.Audio1 unique owner, requests the unit restart, then polls GetNameOwner until that exact owner is gone or a bounded two-second timer expires. A missing owner needs no retirement wait; malformed/unavailable owner queries fail closed in the helper result.
- Owner-query calls are each capped at 500 ms and clipped to remaining wait time; the wait sleeps 25 ms between polls. RestartUnit keeps its existing two-second call bound.
- The helper logs errors/timeouts and production intentionally proceeds so Audio1 cannot hold desktop startup hostage. The docs accurately state that an unretired owner may still be seen by Settings. This is especially relevant on the private-bus Type=dbus path: the existing ADR-0170 explains that a user manager not connected to the session bus cannot observe BusName acquisition. The candidate calls this a bounded best-effort and does not claim guaranteed retirement on that path.
- Private-bus tests prove delayed retirement after an immediate RestartUnit reply (300 ms), the two-second timeout when an owner persists, and the fixed list. They do not emulate systemd actually killing/restarting a Type=dbus unit; the documented caveat is therefore still operationally unqualified.
- Focused test passed: ctest --test-dir build/dev -R 'qindaqt.session-resident-service-refresh$' --output-on-failure (1/1, 2.63 s). git diff --check passed.

Blocking documentation finding: ADR-0094 remains unchanged although the fixed production list changed. Its Decision at lines 58-68 still enumerates only four units and says a service is added only for direct Wayland use or startup-cached desktop environment/routing. Audio1 is a fifth, D-Bus-only package-upgrade/ABI exception. Consequences at lines 106-115 still say four calls and that adding a fifth requires updating this ADR. The architecture pages describe the new behavior, but the accepted ADR remains directly contradictory. Add a durable ADR extension/superseding decision and update ADR-0094's forward pointer/list accounting without rewriting its historical rationale; include the package-upgrade exception and private-bus limitation. Until then the change is incomplete under the repo documentation policy.

Merge preflight: git merge-tree --write-tree 8c639c1e 99b2fac1 produced tree 7cb887e88806f013ae84b28fa32690bb7ec8a9f4 with no conflict entries. The current main source tree does not yet contain this implementation. Candidate changes only its two architecture docs, not ADR-0094, so the same ADR gap will remain after a clean source merge.

No candidate, main, install, live service, or qinda-top changes were made; this reply is the review board record.
