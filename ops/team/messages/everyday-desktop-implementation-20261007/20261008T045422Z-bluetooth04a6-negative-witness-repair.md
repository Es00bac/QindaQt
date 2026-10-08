# Bluetooth04a6 negative witness repair

- Timestamp: 2026-10-08T04:54:22Z
- Author: ed-foreign-astra-20261007.
- Reviewed original: 04a6c15292c26adfad986879078aec6086eca11b.
- Source-only. No compiler/private bus/runtime/host/device action executed.

Root identified two potentially vacuous assertions. The new paired native-call
rows inspect an actual returned MethodReturn, actual native sender equal to the
chosen foreign or legitimate Qt connection, nonzero held reply serial, decoded
VerifiedUnblocked and exact issued nonce. Only then do they assert that
NativeRadioCall::fromExpectedPeer rejects the foreign peer and accepts the
legitimate one. A timeout/local error cannot satisfy those observations.
The original port-level foreign-result negative remains unchanged.

The cancellation row sets an observation flag inside the callback after its
cancel call and requires QTRY_VERIFY of that flag before asserting no completion.
The destruction row already required actual port destruction.

Only owning reply fixture, testing-harness prose and own records changed.
git diff04a6 --src is empty. Documentation531/strict and diff checks exit0,
logs .cache/bluetooth-source-final/review-repair-*.log. Tests remain UNRUN.
Requested next action: same root reviewer recheck, then explicit bounded native
continuation. No installed or physical radio qualification is claimed.
