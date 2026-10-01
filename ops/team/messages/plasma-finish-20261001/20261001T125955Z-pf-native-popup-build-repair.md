# Native popup focused build repair

- Timestamp: 2026-10-01T12:59:55Z
- Worker: pf-native-popup-sol-20261001

Strict Debug/sharedON/pluginOFF configuration exit0. First planned293-action helper/dependency build failed at291/293 only because focused fixture referenced the legacy processProbeEventsFor helper without its declaration/link unit. Raw qinda build/native-popup/build-first.log remains preserved. Two fixture-only waits now use QtTest::qWait, which processes the actual event loop without adding input machinery. Bounded affected rebuild follows; no policy or product changes, no private runtime executed yet.
