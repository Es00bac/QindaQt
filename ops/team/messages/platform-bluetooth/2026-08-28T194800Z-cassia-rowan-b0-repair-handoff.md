# Bluetooth B0 Repair Handoff — Faraday Finding Resolution

- **From:** Cassia Rowan (Google Antigravity Vertex ADC `gemini-3.7-flash-high`, reasoning: high)
- **Role:** permanent Bluetooth B0 rescue/finish partner
- **Timestamp:** 2026-08-28T19:48:00Z
- **Branch:** `worker/bluetooth-b0`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`
- **Candidate commit:** `278a5f9520f3cc47e554816961c0c653295fcbc4`
- **Parent commit:** `f810108b4042b2215a318f48430de743b883d51a`
- **Tree hash:** `8c520c26a31a60fa54c3bf5165d3a7ca1fc8ba95`
- **Status:** handoff — requesting non-Gemini exact rereview (GLM or Claude)

---

## Resolved Findings

### 1. P1-1: Hostile Oversized-Wire Fixture Address Canonicality & Poison Verification
- **Root cause:** `HostileSnapshotService::GetSnapshot` generated addresses using `.arg(56 + index, 2, 10, QLatin1Char('0'))`, which for index $\ge 44$ emitted 3-digit decimal strings (e.g., `100`), failing `isCanonicalAddress` before aggregate wire limits were evaluated.
- **Fix:** Formatted all 257 device addresses using two uppercase 2-digit hex octets:
  `QStringLiteral("AA:BB:CC:33:%1:%2").arg((index / 256) & 0xFF, 2, 16, QLatin1Char('0')).arg(index % 256, 2, 16, QLatin1Char('0')).toUpper()`.
  Every single device address from index 0 to 256 is fully canonical, unique, and valid.
- **Negative Control / Poison Test Proof:**
  Bypassing the bounded decode check in `readBoundedArray` (`// wireValid = false; (void)wireValid;`) causes `qindaqt_bluetooth_qt_transport_tests` to fail with exit code 1:
  ```
  FAIL!  : QtBluetoothTransportTests::hostileOversizedWireSnapshotIsRejected() Compared values are not the same
     Actual   (client.state())          : 2 (ClientState::Ready)
     Expected (ClientState::Unavailable): 3 (ClientState::Unavailable)
     Loc: [tst_qt_bluetooth_transport.cpp:273]
  Totals: 3 passed, 1 failed, 0 skipped, 0 blacklisted
  ```
  This proves the test strictly validates the wire bound and rejects only because `wireValid == false`.

### 2. P3-1: Duplicate Heading in Documentation
- Removed duplicate `## Qualification boundary` heading in `docs/wiki/architecture/bluetooth-service.md`.

### 3. P3-3: Truth & Cosmetic Polish
- Updated `docs/wiki/architecture/bluetooth-service.md` Qualification boundary notes to match manager-lane wording and hardware/session boundary disclaimers.
- Consolidated `PrivateBus` daemon executable path in `tests/services/bluetooth_client/tst_qt_bluetooth_transport.cpp` to use `QINDAQT_DBUS_DAEMON_EXECUTABLE`.
- Fixed line break formatting in `src/services/bluetooth_client/src/bluetooth_client.cpp:400`.

---

## Verification Evidence

1. **Lineage & Diff Hygiene:**
   - Single clean non-amended descendant commit of `f810108b4042b2215a318f48430de743b883d51a`.
   - `git diff --check`: 0 errors.
   - Files changed: 5 files (+24 / -15).

2. **Debug Build & CTest (`/mnt/d/QindaQt/builds/bluetooth-b0/dev`):**
   - Ninja build: 0 warnings, 0 errors.
   - CTest 9/9 passed (100%):
     - `qindaqt.bluetooth-protocol` (PASS)
     - `qindaqt.bluetooth-model` (PASS)
     - `qindaqt.bluetooth-deterministic-backend` (PASS)
     - `qindaqt.bluetooth-client` (PASS)
     - `qindaqt.bluetooth-qt-transport` (PASS)
     - `qindaqt.bluetooth-activation` (PASS)
     - `qindaqt.bluetooth-service` (PASS)
     - `qindaqt.bluetooth-lease-owner-loss` (PASS)
     - `qindaqt.bluetooth-staged-install` (PASS)

3. **Release Build & CTest (`/mnt/d/QindaQt/builds/bluetooth-b0/release`):**
   - Ninja build: 0 warnings, 0 errors.
   - CTest 9/9 passed (100%).

4. **Direct Executable Unit Tests:**
   - `qindaqt_bluetooth_protocol_tests`: 16 passed, 0 failed.
   - `qindaqt_bluetooth_model_tests`: 17 passed, 0 failed.
   - `qindaqt_bluetooth_deterministic_backend_tests`: 11 passed, 0 failed.
   - `qindaqt_bluetooth_client_tests`: 12 passed, 0 failed.
   - `qindaqt_bluetooth_qt_transport_tests`: 4 passed, 0 failed.
   - `qindaqt_bluetooth_activation_tests`: 3 passed, 0 failed.
   - `qindaqt_bluetooth_service_tests`: 4 passed, 0 failed.
   - `qindaqt_bluetooth_lease_owner_loss_tests`: 3 passed, 0 failed.
   - Total direct unit checks: 70 passed, 0 failed.

5. **Static Analysis & Repository Policy:**
   - `tools/validate-docs`: 66 Markdown documents and mkdocs.yml navigation validated (0 errors).
   - `tools/check-source-shape`: 1050 source files checked, all within size bounds (0 errors).

---

## Review Request

Requesting exact non-Gemini re-review (GLM or Claude reviewer) on candidate commit `278a5f9520f3cc47e554816961c0c653295fcbc4` in worktree `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-b0`.
