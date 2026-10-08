# Bluetooth failed-open filter ownership repair
- Time: 2026-10-08T05:10:05Z
- Author: ed-foreign-astra-20261007
- Runtime source: 40d508894524af578e59c411c94404fe36d3522c.
- Outcome: NEEDS_FIX from actual native evidence, not acceptance.

Strict Debug build exited 0. The complete already-launched main cohort settled
31/32, exit 8 in 30.23 seconds. Only qindaqt.bluetooth-radio-session crashed:
explicitWrongGuidCannotFallBack reached destructor and SIGSEGV at
NativeRadioWire::~NativeRadioWire, filter removal. No applet run began.

Exact cached libdbus 1.16.2 source dbus_connection_remove_filter lines 5690–5740
checks absent filter only under DBUS_DISABLE_CHECKS's inverse. Our open path can
own a connection yet fail authentication/Hello before installing the filter.
Unconditional teardown removal therefore does not own that registration.
The repair records successful registration and removes only that acquired
filter; connection close/unref and every admission/refusal predicate remain.
Existing wrong-GUID and same-path/new-broker tests are unchanged direct
regressions. No test weakening or authority fallback is added.

Raw evidence under ignored .cache/bluetooth-native-logs includes
main-40d-ctest.*, main-40d-LastTest.log, build-40d.* and
main-40d-failure-evidence.json (raw digests and completed-suite totals).
Completed Qt suite aggregate: 22 suites,
[227, 0, 0, 0]; the crashed suite has two PASS lines and no
terminal totals and is excluded from that aggregate. Source bytes40d and old762
failure evidence are immutable. Main authority/reply/native-service and staged
header gates passed within this failed cohort; that is no overall acceptance.

Requested next action: root exact source recheck before native continuation.
No compiler/test is now running. No host bus, radio, device or installed action.
Whole batch and installed qualification remain incomplete.

Verification before freeze: docs validation 531/exit0; MkDocs strict exit0;
diff check exit0; all test bytes unchanged from40d (diff exit0). Native repair
remains unrun pending independent review.
