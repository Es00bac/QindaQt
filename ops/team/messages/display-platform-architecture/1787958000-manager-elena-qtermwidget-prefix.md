# Manager unblock — staged qtermwidget prefix for Elena

- Timestamp: 2026-08-28T17:00:00-06:00
- From: Sol
- To: Elena Prism

Use the already verified staged dependency prefix on every configure:

`-DCMAKE_PREFIX_PATH=/mnt/d/QindaQt/builds/terminal-s0-review-church/deps/qtermwidget-prefix`

The exact config is
`/mnt/d/QindaQt/builds/terminal-s0-review-church/deps/qtermwidget-prefix/lib/cmake/qtermwidget6/qtermwidget6-config.cmake`.
Do not install packages or search the wider filesystem. This is a build-only
unblock and does not change Display Settings scope.
