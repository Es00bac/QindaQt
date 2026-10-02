# Native DPMS actual bounded checkpoint

Exact source `a6964310dbfb3a11df06532a32059f4ab9e92ac2`; no source edits. Both actual attempts preserved in ignored qinda runtime worktree archives `build/native-dpms-a696` and `build/native-dpms-a696-short-tmp`. Exact commands, 20 hashes, log/status SHA and PID/starttick receipts are in adjacent JSON.

First attempt failed before fixture initialization because our archive-nested TMPDIR made the Unix socket path exceed108bytes: exit1/0.6143088s, Qt1pass1fail0skip. Parent authorized exactly one setup-only replay with fresh short0700 `/tmp/pf-dpms-0jqgukly`. Replay exit134/1.8979049s, Qt0pass1fail0skip/6ms: `ASSERT !m_outputBackend`, `src/main.cpp:516`. No11behaviorrows executed.

The framework `WaylandTestApplication` constructor installs VirtualBackend at `kwin_wayland_test.cpp:116`; our fixture then calls the one-shot setter at its initTestCase. This is a fixture ordering defect, with no production DPMS/admission conclusion. The safe minimal repair is construction-time test backend injection (existing default preserved), and this test explicit main. No source repair/retry authorized or performed at this checkpoint.

All20frozen source/cache/artifact hashes unchanged; leader1706701/starttick42680574 and children1706705/42680579,1706706/42680580,1706707/42680580 gone; core0. Earlier1702211 and descendants also gone. Private lease released to root immediately. This gate covers only scoped private software/virtual Workspace authority; no physical DRM/DPMS/GPU-renderer or fullPower qualification. Request narrow fixture repair ownership and tiny reviewed target build/replay after root Shortcuts.
