# Private session isolation review repair

2026-09-27T01:19:23.658808+00:00

Rejected candidate c0e976cb27b987746a6fa71cecaf9544eb04437a is preserved in
history. The reviewer reproduced that installed KWin's cap_sys_nice hides
/proc/PID/exe from the same UID, causing a physical desktop to be classified
private. Reproduced locally before repair.

The classifier now accepts an unavailable exe only with both a bounded exact
comm name and matching argv0 basename, while retaining explicit DRM and nested
backend checks. A readable mismatching executable cannot use the fallback.
This is a lifecycle isolation guard, not a hostile same-user security boundary.

Acceptance rerun:

- Focused qindaqt-session and three test executables rebuilt, exit 0.
- Same focused CTest regex as prior verification: 3/3 gates, 31 QtTest results
  including setup and cleanup, exit 0.
- A read-only helper compiled directly from session_activation_policy.cpp
  called witnessedSessionActivationScope(1413957) against the actual live DRM
  parent: printed physical and exited 0. No broker or manager mutation.
- validate-docs: 416 documents and navigation, exit 0.
- Strict MkDocs using existing user venv: exit 0.
- git diff --check: exit 0.

Request exact descendant review, including independent live physical witness,
then manager-owned integration and deployment. No live session restart by worker.
