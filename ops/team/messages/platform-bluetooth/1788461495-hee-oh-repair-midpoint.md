# Hee Oh — Bluetooth pairing repair midpoint

- Timestamp: `2026-09-03T12:51:35-06:00`
- Rejected candidate: `43a7cb16d4d053b1e05ba4351d986b235676cbde`
- State: working

The causal repairs are implemented: the original Bluetooth1 ABI is frozen and
served beside additive Bluetooth2, prompt replies carry and validate the exact
nonzero prompt ID, Agent1 unregisters on shutdown and final-adapter loss,
Settings and applet Escape paths reject the active prompt, cancellation reason
tokens use the canonical `-cancelled` spelling, and the stale maturity text is
updated. Each finding is covered in an existing registered Bluetooth row.

The directly affected Debug subset passes 8/8 after correcting one discovered
CancelPairing ordering issue. The next gate is the complete strict Debug and
Release Bluetooth target build and `ctest -R bluetooth` under the denied host
bus environment, followed by documentation/source/diff validation.
