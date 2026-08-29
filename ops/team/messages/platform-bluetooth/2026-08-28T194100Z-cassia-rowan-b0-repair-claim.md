# Cassia Rowan — Bluetooth B0 repair claim (post-Faraday rereview)

- Time: 2026-08-28T19:41:00Z
- Worker: Cassia Rowan, Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning: high
- Status: working — repairing confirmed P1 (oversized-wire hostile fixture address canonicality & negative mutation test) and P3 polish in isolated worktree
- Base candidate: `f810108b4042b2215a318f48430de743b883d51a`
- Branch: `worker/bluetooth-b0`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`
- Target: single non-amended descendant commit of `f810108`

## Repair Scope

1. **P1-1 (Blocking)**: Repair hostile oversized-wire test fixture in `tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp`. Ensure all 257 device addresses remain canonical D-Bus hex address strings (e.g., using hex formatting or secondary adapter index prefixing) so device address validation passes and rejection is strictly caused by the aggregate wire payload bound (`maxDevicesPerAdapter = 256` / `oversized-payload`). Add an explicit negative control / mutation test proving that removal or bypass of `readBoundedArray` failure flag causes the hostile oversize test to fail.
2. **P3-1**: Remove duplicate `## Qualification boundary` heading in `docs/wiki/architecture/bluetooth-service.md`.
3. **P3-3 (Truth polish)**:
   - Update `bluetooth-service.md` status wording to reflect current executing Debug/Release focused test coverage.
   - Clean cosmetic line formatting in `bluetooth_client.cpp:400` if present.
   - Consolidate `tst_qt_bluetooth_transport.cpp` PrivateBus daemon path to use `QINDAQT_DBUS_DAEMON_EXECUTABLE`.
4. **Verification**: Full strict Debug and Release builds under `/mnt/d/QindaQt/builds`, full test suite execution (focused Bluetooth + adjacent tests), negative mutation controls, source shape (`tools/check-source-shape`), docs validation (`tools/validate-docs`), strict MkDocs, and diff hygiene.
5. **Rereview request**: Request independent GLM / Claude review upon completion.
