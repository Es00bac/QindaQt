# Bluetooth304 private native qualification handoff
- Time: 2026-10-08T05:16:32Z
- Author: ed-foreign-astra-20261007
- Exact tested product/tests: 3040022a03e5eb0606a038a0f505d963e463b909.
- Status: private qualification passes; independent final evidence review requested.
- Resources: all qinda compiler/private-bus/offscreen resources RELEASED.

Source was independently reviewed before each native continuation. Strict Debug
main and standalone builds use -j24 -l24; CTests are serial. Actual generated
registries asserted 32 main/7 standalone. Main:32/32 exit0,23 Qt suites240passed,
0failed/0skipped/0blacklisted. Applet:7/7 exit0,
5 Qt suites with totals
[42, 0, 0, 0]. Production daemon/helper were built.
Both staged public-header poison builds failed on the exact missing absolute
header; restored bytes equal source, rebuild and consumer execute passed.

Exact recipe in ignored .cache/bluetooth-native-logs/run_native_58af.py:
build 304; main-tests 304; applet-configure 304; applet-build 304;
applet-tests 304. Main selector is ^qindaqt[.](bluetooth-|settings-bluetooth-).
targets-native-repair.json records25 main and5 standalone targets.
main-304-registry.json and applet-304-prebuild-registry.json record actual rows.
product-provenance-304.json records source blobs/SHA256. Final raw digest index:
.cache/bluetooth-native-logs/final-304-evidence.json, plus main/applet-304
CTest argv/log/exit/JUnit and LastTest logs, builds and private environment.
The applet preflight first assumed cache BOOL; actual standalone UNINITIALIZED=ON
was strict. This assertion stopped before build, then corrected by checking ON
and generated -Werror/-Wshadow. Configure log and preflight JSON preserve it.

Original controls/failures remain immutable: old762 production with original8c02
regression strict build0, Qt2pass3fail/exit3; revised58 missing fixture include;
40d strict build0/main31of32 with actual wrong-GUID teardown SIGSEGV. Exact304
registration-owned filter repair retains unchanged tests and refusal guards.
Final actual sender/serial+nonce foreign-frame witness, cancellation-observed,
GUID replacement, native helper positive and SDK rows pass; no replay/guard
weakening was used.

No host buses, rfkill, radios, connected devices, ordinary services or installed
actions were used. Full effective helper-unit namespace, actual selected control,
Portage installation and fresh-session behavior are still open. ADR0359 remains
Proposed pending independent acceptance/integration. Existing main daemon
sandbox and unrelated installed drop-ins are untouched.

Requested next: root independently inspect exact raw evidence/integrate accepted
source and owning gates. My next compatible assignment is source-only Audio
fixture c17fd9b and overlay b489 hook-lifetime review; no borrowed-Core execution.
