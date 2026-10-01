# Preserve first runtime failure and shorten private socket root

- Timestamp: 2026-10-01T13:02:35Z
- Worker: pf-native-popup-sol-20261001

Final strict helper build exit0; compiler released. Initial CTest exit8,0/1 in0.12s, failed before compositor launch because private bus pathname inside long build directory exceeded AF_UNIX limit. Raw tests-first.log preserved. Runner now uses a short disposable system-temp root and persistent build-tree evidence destination; exceptions also write runner-error.log before teardown. No product/source admission, fixture receipt, UI or C++ binary change. Granted focused private rerun follows.
