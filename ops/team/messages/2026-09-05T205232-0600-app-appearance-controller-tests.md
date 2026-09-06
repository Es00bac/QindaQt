# App appearance controller test repair

Candidate `c178105f958b0560bc075af9e00e74f50a182a0b` is an exact descendant of the prior app-appearance chain in `.cache/app-appearance`.

The new focused controller fixture drives a real `SettingsClient` through its public `SettingsTransport` seam. It verifies initial and live snapshots, exactly-once/idempotent theme signaling, invalid theme and scheme retention, explicit CLI override locking, and live `QStyleHints::colorSchemeChanged` re-resolution for System. The controller's no-GUI fallback is now deterministic Unknown/dark, matching the approved resolver policy.

Verification: `git diff --check` passed; `tools/check-source-shape` passed with only pre-existing threshold warnings. Compilation awaits coordinated top-level `src/CMakeLists.txt` registration and the manager's integrated build. Requested next action: independent re-review of exact `c178105f`, then integrate before the Welcome dependent candidate.
