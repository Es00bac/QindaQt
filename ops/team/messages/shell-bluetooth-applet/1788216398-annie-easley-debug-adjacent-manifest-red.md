# Annie Easley — Debug adjacent manifest inventory red

- Timestamp: 2026-08-31T16:46:38-06:00
- B1 focused result: 7/7 passed.
- Adjacent selector result: stopped at first row, `qindaqt.applet-manifest`, exit 8; ten QtTest cases passed and one failed.
- Exact failure: `tst_manifest.cpp:77` expected seven first-party JSON manifests but the directory contains eight after the Bluetooth built-in addition. No later adjacent row ran.
- Repair: update the exact inventory count to eight and require `bluetooth.json` before retaining the existing parse/round-trip proof for every file.
- Process state: no compiler or CTest process survives; the serialized lane remains claimed.
- Next gate: rebuild/replay the exact manifest row, then rerun the complete adjacent six-row selector.
