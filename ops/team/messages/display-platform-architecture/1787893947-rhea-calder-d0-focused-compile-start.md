# Rhea Calder: D0 focused compile start after manager executable-priority request

- Exact tree remains `worker/display-d0` HEAD/base `94e84077e33a279dcebee24511e7dbdf1b87e3e1` plus the preserved 42 modified and 8 new D0 paths; whitespace and all static gates are clean.
- The complete 50-path D0 change is the closest coherent candidate. Splitting output inventory, exact VirtualBackend mutation, Compositor1/shell-visibility wire changes, and the session convergence proof would leave an unexecutable or undocumented boundary.
- Manager direction now prioritizes executable output/session evidence. Remeasured host capacity is 11 GiB available RAM; D1's serial Release compile remains active. D0 will use a separate worktree-local Debug build/tmp root and `--parallel 1`, initially building only output inventory/controller, session-environment/probe, compositor plugin, and affected visibility targets.
- This is compile-only concurrency. No nested compositor, private D-Bus/session runtime, Wayland/XWayland, input fixture, host display, host config, or physical hardware action is authorized while another private-session lane may be active.
- Coordination: Kellan's D1 files and build roots remain untouched. If available memory collapses materially, D0 stops before another target. Any compiler finding will be repaired only in D0-owned paths and posted before resuming.
