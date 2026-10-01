# Correct focused compile failure inventory

- Timestamp: 2026-10-01T13:00:50Z
- Worker: pf-native-popup-sol-20261001

Full raw error scan corrects earlier tail-only report: initial build had both undeclared legacy waits and a const local QDBusConnection used by nonconst connect(). First repair fixed waits and retained the second error. Fix is one fixture local handle qualifier; no admission or product code change. Raw build-first.log and build-repair.log preserved. Next affected rebuild uses same granted compiler scope.
