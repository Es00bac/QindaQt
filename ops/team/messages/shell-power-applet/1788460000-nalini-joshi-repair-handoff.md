# Session actions repair handoff

- Candidate commit: `23d99f5b3ec5df65b4525552197f744c382f3640`
- Tree: `8fde42060eb32d620ea2dfc494fa62d98b90b9c7`
- Exact repair base: `f78695f05218ca1e01df0d0c6d6cbcbd3f1ecaa9`
- Rejected candidate repaired: `437064325e7aa29b14a7dab230382ab947e71253`

## Changed paths

- `docs/wiki/adr/0070-confine-session-actions-behind-authenticated-boundaries.md`
- `src/services/session_actions/CMakeLists.txt`
- `src/services/session_actions/include/qindaqt/services/session_actions/session_actions_client.h`
- `src/services/session_actions/src/session_actions_client.cpp`
- `src/services/session_actions/src/session_actions_state.cpp`
- `tests/services/session_actions/check_boundary.cmake`
- `tests/services/session_actions/check_boundary_negative.cmake`
- `tests/services/session_actions/tst_session_actions_client.cpp`

## Evidence

- New registered regressions against the rejected implementation: `qindaqt.session-actions-client` failed with the owned-but-empty ScreenSaver service advertising Lock and the replaced owner's delayed success returning `Succeeded`; exit 8, 1/3 CTest rows failed. Both cases pass after repair.
- Debug and Release configure with the assigned system-KWin cache and all required strict/test/shell flags: exit 0 each.
- Focused Debug and Release session, Power, Settings Power, and shell-runtime target builds: exit 0 each.
- Exact poisoned-bus selector `env -u DBUS_SESSION_BUS_ADDRESS DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent ctest --test-dir <ROOT>/<config> -R '^qindaqt\.(session|power-|settings-power-|shell-runtime-)' --output-on-failure --no-tests=error`: Debug 42/42 passed, exit 0, 62.74 s; Release 42/42 passed, exit 0, 58.16 s.
- `./tools/validate-docs`: exit 0, 141 Markdown documents and navigation validated.
- Strict MkDocs to the assigned build root: exit 0.
- `./tools/check-source-shape`: exit 0, 2,421 files checked; only existing review-threshold warnings, and the repaired main source is 499 non-blank lines.
- `git diff --check`: exit 0. No JSON changed, so no `json.tool` invocation applied.

## Bounded caveats and next action

This candidate claims private-bus fake coverage and the existing offscreen/package rows only. It does not claim a host bus, live ScreenSaver/login1 action, nested compositor, hardware, uinput, or network evidence. The ADR boundary claim is now scoped to the named session-action consumers; separately owned platform adapters remain governed by their own public boundaries.

Requested next action: Wei Ho performs one independent exact recheck of `23d99f5b3ec5df65b4525552197f744c382f3640`, then the Program Manager integrates it.
