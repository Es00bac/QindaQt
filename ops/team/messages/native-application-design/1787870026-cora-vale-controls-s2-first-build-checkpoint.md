# Cora Vale Controls S2 first Debug build checkpoint

- **Timestamp:** 2026-08-27T22:33:46Z
- **Branch/worktree:** `worker/controls-s2` at
  `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Exact HEAD:** `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Compiler state:** stopped; lane returned to manager
- **Evidence log:** `build/controls-debug/.ninja_log` plus worker terminal
  transcript; no CTest log exists because no test started

Manager released exactly one Debug `-j1` lane. Configure passed 1/1 (exit 0):

```sh
cmake -S . -B build/controls-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
```

The focused build command was:

```sh
cmake --build build/controls-debug \
  --target qindaqt_controls_behavior_tests \
           qindaqt_controls_visual_tests \
           qindaqt_controls_bare_memory_probe \
           qindaqt_controls_memory_probe \
           qindaqt_controls_qml_qmllint \
  --parallel 1
```

It is **not passing yet**. The initial pass stopped at 263/274 on two strict
test-source findings: this QtTest exposes no `QWindow` `keyClicks()` overload,
and `QSignalSpy::size()` is `qsizetype`, not `int`. The source now sends each
ASCII key through the supported `QWindow` `keyClick()` overload and preserves
the inferred size type. The first resume then stopped while compiling the
visual test due to an incomplete `QQmlEngine`; its direct include is present.
The second resume stopped at 6/12 on the same direct-include omission in the
controls PSS probe; that source repair is also present.

The QML module itself completed qmlcache generation for all 14 QML files and
linked its backing library and plugin before these test-only stops. That is
build progress, not a passing module claim. Exact tests executed: **0**.
Behavior, policy, PSS, qmllint completion, visual fixtures, installed import,
Release/broad matrices, and docs gates remain unclaimed. No configure/build
process remains; recompilation is held until the manager explicitly releases
the lane after Audio sanitizer/package work.
