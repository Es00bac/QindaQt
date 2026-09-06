# Handoff: Global Menu flash/inconsistency repair (finish-menu-kimi)

- **From:** finish-menu-kimi (Moonshot Kimi CLI)
- **Time:** 2026-09-06T15:57:32Z
- **Candidate:** `a02e4856fce9f6aae9abcd8042b46a08041d92ae` on `fix/finish-menu`, exact base `8958c38f0a9cca5c7906ad62e2fa54afb486a05f`
- **Requested next action:** independent exact-commit review, then manager integration.

## Changed paths (all lane-owned)

- `src/shell/global_menu/applet/include/qindaqt/shell/global_menu/applet/globalmenuappletaccess.h`
- `src/shell/global_menu/applet/src/globalmenuappletaccess.cpp` — new `beginTransition()`/`endTransition()`
- `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml`
- `src/shell/global_menu/composition/include/qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h`
- `src/shell/global_menu/composition/src/global_menu_transport_coordinator.cpp`
- `tests/shell/global_menu/applet/tst_globalmenuappletaccess.cpp` — 4 new transition rows
- `tests/shell/global_menu/transport_composition/tst_global_menu_transport_composition.cpp` — 3 new regression rows, 2 updated expectations
- `tests/shell/global_menu/runtime_composition/tst_file_manager_menu_export.cpp`,
  `tests/apps/terminal/tst_terminal_menu_export.cpp`, `tests/apps/text_editor/tst_editor_menu_export.cpp` — app-exit rows await the bounded presentation grace
- `docs/wiki/shell/global-menu.md` — authority-vs-presentation contract

## Root cause (verified in code and by gate behavior)

Every compositor `ActiveWindowIdentityChanged` — fired for any visibility-relevant change, including pure geometry changes of any visible window, because the identity canonical state carries the visibility `actionRevision` — made the shell client withdraw its snapshot and re-emit `identityChanged`. The coordinator answered the withdrawal with a full `clearAuthority()` (client teardown + `publishUnavailable`) and the reread with a full rebind including a fresh `GetLayout` round trip; `bindRegistration()` additionally published unavailable before the async tree; the applet QML sized to zero while unavailable, reflowing the panel; and content-identical exports were republished, bumping the facade generation and rebuilding every delegate. Matches finish-ux-glm finding 3.

## Fix contract

Invocation authority is still revoked synchronously and fail-closed on every withdrawal (selector cleared; facade `available == false` fences pointer/keyboard/AT activation; the invocation guard can never match). Presentation is decoupled: the last accepted projection is retained, dimmed and inert, for a bounded 500 ms grace so transient invalidation/reread cycles and same-window generation moves never collapse the panel or rebuild delegates. A reread re-proving the same window+endpoint resumes in place (`renewBoundProvider`, no client teardown, epoch preserved); a different window binds through a transition (old menu never actionable, replaced atomically by the first new tree); genuine loss clears at grace expiry or immediately on hard failures (auth/PID mismatch, export rejection, client owner loss). Hosted acknowledgments per ADR-0077 are untouched.

## Gates (all on the candidate, Debug, isolated lane build root `build/dev`, `ninja -j3`)

- `ctest -R 'global-menu|app-shell-menu-export'`: **35/35 pass** (25.7 s) — protocol, ownership, ownership-lineage, exporter, qt-widgets-adapter, applet-access (incl. 4 new rows), composition, registrar/dbusmenu private-bus, dbusmenu decoder/server, transport-composition private-bus (6/6 incl. 3 new regression rows), runtime-composition private-bus, transport/runtime boundary poisons, all six QML offscreen rows under `QT_FATAL_WARNINGS=1`, terminal/editor/file-manager private-bus rows incl. hostile-registrar and registrar-absent variants, installed-package.
- `python3 tools/docs_validation.py` — exit 0 (176 documents).
- `tools/check-source-shape` — exit 0.

## Caveats / limitations

- **Release profile not run** in this lane (Debug only); manager integration gates should cover it.
- **mkdocs is not installed** in this environment (`mkdocs build --strict` could not run); `docs_validation.py` navigation/link validation passed instead.
- Source-shape reports `global_menu_transport_coordinator.cpp` at 503 non-blank lines (threshold 500, hard cap 600). Decomposition review outcome: the file is one cohesive authority/presentation state machine; the added ~90 lines are the transition lifecycle and splitting would scatter one invariant set. Revisit if it grows further.
- Two hosted-ack count expectations now accept the dual-observer withdrawal on provider disconnect (registrar owner loss + client owner loss each emit; the registrar deduplicates the D-Bus signal, so applications see one). Pre-existing behavior on the announced path, previously masked by immediate teardown.
- The compositor identity invalidation cadence itself is unchanged (owned by other lanes); the coordinator now absorbs it. No shared-path edits were needed.
- Behavior verified through the private-bus/offscreen gates only; no live desktop was touched.

Worker record: `ops/team/workers/finish-menu-kimi.md`.
