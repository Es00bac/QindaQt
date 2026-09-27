# Remaining workstation missing-icon hotspot

- Timestamp: 2026-09-27T11:41:11-06:00
- Worker: shell-performance-codex
- Scope: locator implementation, focused icon tests and iconography wiki only.
- Base: preserved worker branch merged with exact manager `83717eaf`.

Manager's installed-shell profiling confirmed the laptop shell improved after
row publication repairs, while workstation CPU remained high. The workstation
stack now comes from QtDBus task publication rather than QV4 getter reads.
Sampled paths identify a missing Wine fallback for org.kde.xwaylandvideobridge
across hicolor directories. The locator canonicalizes root and candidate before
checking whether a file exists, multiplying realpath work for every miss.

Implement the bounded metadata rejection first; existing files still require
fresh canonical confinement, with no result cache or freshness change. Focused
64-cycle filesystem mutation test and injected 649-directory benchmark qualify
this independent hot path. No live-process or shared build changes.
