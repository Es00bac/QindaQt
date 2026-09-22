# Panel republication leaks threads and RSS — in Qt/Mesa, not QindaQt

**Date:** 2026-09-22 · **Settles:** OPEN-DEFECTS.md item 5

## The question

The shell republishes its entire panel surface set on every output-generation
change, destroying and recreating each panel `QQuickWindow`. A live session
showed threads climbing 78 → 114 and RSS 289 MB → 490 MB across roughly an hour
while `outputGeneration` went 9 → 15. Thread growth came in threes, which is one
Mesa `util_queue` set per GL context.

What was not established: whether the leak is in QindaQt — a window or
scene-graph resource outliving republication — or in Qt/Mesa, which would mean
contexts are not released when a `QQuickWindow` is destroyed.

## The reproducer

`tools/diagnostics/panel_republication_leak_probe.cpp` contains **no QindaQt
code at all**. It creates N plain `QQuickWindow`s from an inline QML `Window`,
shows them, destroys the whole set, and repeats, printing `/proc/self/task`
count and `VmRSS` per generation. The output count is held fixed, which is the
confound that made the original in-session measurements ambiguous.

```console
$ g++ -O1 -fPIC -o leak tools/diagnostics/panel_republication_leak_probe.cpp \
    $(pkg-config --cflags --libs Qt6Quick Qt6Qml Qt6Gui Qt6Core)
$ XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=<session> QT_QPA_PLATFORM=wayland \
    LEAK_GENERATIONS=30 LEAK_PANELS=3 ./leak
```

## The result

Qt 6.11.1, Mesa, against the live session compositor, 3 windows per generation:

| generation | threads | RSS (kB) |
| --- | --- | --- |
| baseline | 4 | 38 088 |
| 1 | 12 | 106 120 |
| 5 | 13 | 118 356 |
| 10 | 14 | 130 380 |
| 15 | 15 | 140 984 |
| 20 | 16 | 146 236 |
| 25 | 18 | 160 228 |
| 30 | 20 | 162 968 |

Monotonic, with no plateau: roughly **one thread per 3.6 generations and ~2 MB
RSS per generation**, sustained over 30 generations.

## Conclusion

**The leak is in Qt/Mesa.** Destroying a `QQuickWindow` on the Wayland platform
does not release everything it took, and no QindaQt code participates in this
reproducer. The shell inherits it once per panel per output-generation change,
which is exactly the shape the original session showed.

This also explains a reading that looked contradictory. A later sample of the
running shell showed threads *falling* 97 → 50 and RSS 504 MB → 386 MB — but
that sample spanned an output-generation change that **removed** a display, so
the live panel count halved. Both things are true at once: resources scale with
the number of live panels, and a residue leaks per republication. Any future
measurement has to hold the output count fixed or it measures the first effect
and misses the second.

## What this does not say

It does not say the shell has no leak of its own; it says there is a Qt/Mesa one
underneath, large enough to account for the observed growth. Separating any
QindaQt contribution needs the same probe shape run against the real panel
factory.

## Mitigation worth considering

Republishing every panel window on every output-generation change is what
multiplies this. Reusing panel windows whose surface configuration did not
change across a republication would avoid most of the churn, and is worth
weighing independently of the upstream bug.
