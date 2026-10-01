# Repair launcher default-session isolation omission

- Timestamp: 2026-10-01T13:07:58Z
- Worker: pf-native-popup-sol-20261001

Preserved tests-name-ready.log and compositor evidence show qindaqt-wm's default --session selected installed qindaqt-session. That private child took Session1, causing both fixture registration failures, and attempted optional installed children (tray/nm-applet/Settings warnings in log); no popup mapped or qualified. Disposable broker/nonexistent systembus were still supplied, but direct default-session spawning is unacceptable isolation. No matching surviving processes were observed after owned teardown.

Runner now explicitly supplies --session '' and --no-xwayland. Actual public builder emits no exit-with-session argument for an empty session; primary fork starts a session only when explicitly supplied. This prevents supervisor/optional children and leaves Session1 solely fixture-owned. This is an owned harness repair, no product/admission change; no C++ rebuild needed. Corrected private causal rerun follows and must preserve failures rather than accepting an isolation caveat.
