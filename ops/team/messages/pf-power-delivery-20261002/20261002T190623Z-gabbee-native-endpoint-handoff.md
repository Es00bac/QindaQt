# Gabbee native scripting endpoint: exact candidate

Candidate **44c857fe0cb4371e7288113c06b1adf0c2079593**, exact fetched hub/main base **eadb8995ac8ffab99e35526e960118d1c11de232**. Pushed to qinda bare gabbee hub branch worker/gabbee-native-kwin-20261002. Isolated qinda worktree gabbee.worktrees/gabbee-native-kwin-20261002 is clean; shared checkout unchanged.

Four changed paths, SHA256:

- src/gabbee/desktop.py — a7847d51e2ec10d79de8dabbdd36d2f18b45d19a6c2015ebfd568e357abbba31
- tests/test_kwin_script_endpoint.py — d672ad959860d79a95abf86ac4243b0c216195e8e543429da83283d763e0d6d0
- README.md — 4ea76bb8bd97c7bd0d50f8178d09e144a581f2c552efe49549cb48bdcfb511ef
- HANDOFF.md — 00bccfeb24c0a746d7b5dfd8b6d6bbe2a016e5cfe0a95f53138799cb90aee858

The bridge prefers registered org.qindaqt.KWin, /org/qindaqt/KWin/Scripting, /org/qindaqt/KWin/Scripting/Script<N>, org.qindaqt.KWin.Scripting/Script. Only a definitively absent native service permits independent stock org.kde.KWin selection and its original /Scripting + org.kde.kwin interfaces. Missing/disconnected bus or invalid registry query produces no bridge. Selection is retained per bridge; native call failures never mix or switch load/run/unload to KDE. Empty/malformed script IDs fail safely. Unload timeout/error cannot prevent generated-file and callback cleanup.

Actual qinda command (existing Gabbee interpreter; no installation):

```text
DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen DISPLAY= WAYLAND_DISPLAY= PYTHONPATH=src /home/cabewse/gabbee/.venv/bin/python -m pytest -q tests/test_kwin_script_endpoint.py tests/test_desktop_control.py
```

Exit0, **33 passed in0.75s**, zero failures/skips. New24 endpoint rows cover native present/both aliases/stock-only/neither, invalid/missing/disconnected bus, native and stock exact qdbus argv through load/run/unload, changed registrations after load, load/empty-ID/bad-ID/run/timeout/callback/unload failures, and callback/temp-file cleanup. Nine existing desktop behavior tests remain passing. All qdbus invocations in new tests are intercepted by a fake subprocess actor; no host input, window activation or audio recording occurs. No private compositor/GPU/physical hardware gate.

Initial system-Python invocation exit1 reported missing pytest and ran no tests; existing Gabbee interpreter was then used. python -m py_compile of both owned Python paths and git diff --check exit0. Provider/model/process-starttick metadata was not measured or claimed.

Next requested action: different-worker exact review and focused replay before manager integration/fork legacy namespace cutover. No installer/build/voice/icon/game work performed. Native live scripting export behavior and installed cutover remain manager gates. Power exact0e5db candidate/fork26cf remain source-frozen pending independent review; this board handoff adds only own container-wm ops.

Available for narrow exact review findings or source-only native endpoint diagnostic help; no resource lease held.
