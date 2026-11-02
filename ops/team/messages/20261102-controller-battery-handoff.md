# Controller battery accuracy handoff

- Time: 2026-11-02T17:49:17+00:00
- Exact product candidate: cbde24b855d7b0a683e21676991d93f6874ce862
- Base: 151b02798
- Worktree/branch: container-wm.worktrees/controller-battery-20261102 / codex/controller-battery-20261102
- Outcome: verified the connected DualSense's report path and corrected confirmed coarse-level and unknown-value decoding defects.

## Evidence and limits

On qinda, one connected DualSense (Bluetooth, kernel 6.18.57) was observed.
Five read-only 78-byte reportID 0x31 samples passed CRC and had battery step 1 /
charging code 1. The driver's ten-percent midpoint conversion yields 15% Charging.
Sysfs, UPower 1.90.9 and the live Power1 peripheral snapshot all agreed; the
Power1 receipt was bound to its current unique owner and random request nonce.
No device addresses or full input reports are retained in this handoff.
No output report, controller configuration write or credential access occurred.

This proves faithful transport of the current controller report, not independent
battery calibration, every controller, the shell's actual pixels, or an
intermittent mismatch. The optional question asking for the plug-in's displayed
percentage and the user's comparison source has no supplied answer yet.

The confirmed source bug was independent: UPower uses 0/1/3/4/6/7/8 for
Unknown/None/Low/Critical/Normal/High/Full, while Power1's internal enum is compact.
The old conversion changed Normal into Full, mislabeled Low/Critical and rejected
valid High/Full. Both adapters now use one explicit private converter. Warning-only
2 and 5 and unknown ordinals reject. Explicit Unknown suppresses a placeholder
percentage; absent legacy level metadata still permits a numeric percentage.

## Paths

- Production: src/services/power_service/src/adapters/peripheral_decoder.cpp,
  upower_device_decoder.cpp and upower_device_decoder_p.h.
- Tests: tests/services/power_service/tst_peripheral_service.cpp and
  tst_power_upower_adapter.cpp.
- Documentation: docs/wiki/architecture/power-service.md and
  docs/wiki/shell/power-applet.md, plus task/handoff and own board records.
- Public Power1 schemas, compact enum values and ownership boundaries are unchanged.

## Verification

- Focused strict production/test build: exit 0, targets
  qindaqt_peripheral_service_tests and qindaqt_power_upower_adapter_tests.
- Actual `portageq envvar MAKEOPTS`: -j24 -l24; used unchanged for Ninja.
- `ctest --test-dir build/controller-battery -R '^qindaqt\.(peripheral-service|power-service-upower-adapter)$' --output-on-failure --timeout 30`:
  exit 0, 2/2 groups, 1.33 seconds, 20+15 QtTest passes, zero failures/skips.
- Cases cover every documented coarse level for system and controller batteries,
  invalid warning-only ordinals, ignored percentage placeholders, prior device
  failure isolation, and the sanitized 15% Charging live-report fixture.
- Power-service boundary CMake script: exit 0.
- `mkdocs build --strict`: exit 0; repository checker validates 583 documents,
  exit 0; changed production files remain below 500 nonblank lines.

## Delivery

No Portage package or installed runtime was changed. The live r47 reports the
same numeric 15% value, because this repair does not rescale firmware reports.
Integrate the exact candidate and include the decoder fix in a later Portage
delivery. If the user's reference reports a conflicting charge level, compare
the same controller/transport/time before considering a further hardware or
firmware-specific diagnosis. No delegated model workers were launched.
