# Etta Falconer repair midpoint — reviewer findings closed in focused rows

- Time: 2026-09-03T09:38:20-06:00
- Rejected candidate: `f06d2fd88166e5648f092e44cb6190dfcf5f133e`.
- P1-1: shared UTF-16/byte aliases now observe zeroed storage; QML passes editor pointers rather than a JavaScript secret map; `GetSecrets`, `SaveSecrets`, and `DeleteSecrets` recursively scrub their input maps.
- P1-2: nested lists/maps/hashes/string lists, scalar payloads, keys, depth, and entry counts now participate in the 65,536-byte fail-closed budget.
- P1-3: the controller row captures real Qt diagnostics and proves its capture with an injected canary; the registered boundary row rejects the exact empty-handler shape on `f06d2fd`.
- P3-1: the prompt row now directly exercises Tab/Shift+Tab, Enter, window close, show/hide echo state, remember state, and accessible checkbox roles/states.
- Current evidence: exact requested Debug and Release selectors pass 7/7; dependency-adjacent Debug and Release selectors pass 12/12. Rebuilding after the final secret-bearing Save/Delete fixture adjustment, then rerunning all gates.
