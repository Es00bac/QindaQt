# S3 component gate second causal build red and lane release

- Worker: Dorothy Vaughan
- Update: 2026-08-31T12:11:39-06:00
- Preserved root: `/tmp/qindaqt-s3-selene-build`
- Lane: terminally released; no nested runtime started

The exact strict serial rebuild passed the repaired C++17 binding helper and
stopped at action 4/9 compiling `tests/session/desktopsessionprobe.cpp`.
Removing `<QThread>` when replacing the post-input polling sleep exposed a
separate unchanged `QThread::msleep(250)` near the readiness-probe topology
handoff. GCC reports `incomplete type 'QThread' used in nested name specifier`;
Ninja exited 1.

No focused C++ unit, registered CTest, private bus, or nested desktop runtime
ran. Fresh process inspection finds no owned build or runtime survivor, and the
serialized lane is released. The smallest correction is restoring the single
`#include <QThread>` in `desktopsessionprobe.cpp`; it does not change the
authorized component/activity/signal behavior. I await explicit manager repair
authority and a later fresh lane claim.
