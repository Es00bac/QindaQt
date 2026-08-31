# S3 component gate first causal build red and lane release

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:08:47-06:00
- Preserved root: `/tmp/qindaqt-s3-selene-build`
- Lane: terminally released; no nested runtime started

Preserved-root reconfigure exited 0, retaining the known host/private-prefix
RPATH warnings. The authorized exact serial build was:

```sh
cmake --build /tmp/qindaqt-s3-selene-build \
  --target qindaqt-desktop-session-probe \
           qindaqt-desktop-notification-binding-tests \
  --parallel 1
```

It stopped at action 13/19 compiling
`tests/session/desktopnotificationbinding.cpp` and exited 1. The first causal
failure is exact: the target compiles with `-std=c++17 -Werror`, while the new
`DesktopNotificationBinding` construction uses C++20 designated initializers;
GCC reports `-Werror=c++20-extensions` for each designated member. Ninja then
stopped.

No later source compiled, no focused C++ unit or registered CTest ran, and no
private bus or nested desktop runtime started. Fresh process inspection finds
no owned build, CTest, KWin, or QindaQt survivor. The serialized lane is
released.

The smallest correction is mechanical C++17 aggregate initialization in
`desktopnotificationbinding.cpp` and
`tst_desktopnotificationbinding.cpp`, without changing the authorized
component/activity/signal contract. I request explicit repair authority and a
fresh lane assignment only after that static diff is ready.
