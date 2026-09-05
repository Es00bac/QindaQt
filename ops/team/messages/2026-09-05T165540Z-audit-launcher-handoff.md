# Launcher persistence candidate handoff

Exact product commit: `7fc7cf02` on `codex/audit-launcher-persistence`, base `49210a4c`.

Changes: shipped schema keys `panels.launcherPinned` and `panels.launcherRecent`, controller key constants, canonical JSON conversion for schema string lists, round-trip storage regression, real private-bus service-owner replacement and fresh shell-client reload test, Settings1/launcher documentation and ADR-0076. The employee record is accurately waiting for review.

Verification: both focused build invocations exited0;18/18 affected Settings/launcher CTests passed; strict MkDocs exited0; repository link checker passed165 documents; source-shape and diff checks exited0. Logs are in `.cache/evidence-launcher-persistence` under this worktree.

Command: `ctest --test-dir build/audit --parallel 2 --output-on-failure -R '^qindaqt\.(settings-(schema|layers|persistence|migration|repository|service-lifecycle|service-process-lifecycle|protocol|protocol-dbus|client|client-commit-validation|dnd-controller|qt-transport|qt-transport-adversarial)|launcher-(settings-contract|persistence|runtime-boundary|contract-text))$'`.

The restart fixture now creates a distinct D-Bus service connection/object and epoch; same-owner epoch regression checks remain unchanged. Pins and recents both survive on disk and load into a fresh shell client. No production host-session or nested desktop interaction is claimed for this persistence-only slice.

Requested next action: independent review of7fc7cf02, then manager integration and affected gates. Worker available for control-wiring review when assigned.
