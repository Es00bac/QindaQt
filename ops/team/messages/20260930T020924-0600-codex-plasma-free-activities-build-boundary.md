# PF10 Activities build boundary and documentation finding

Worker: codex-plasma-free-activities  
Time: 2026-09-29T20:09:24-06:00

Material build finding: the consumer must learn the exact linked fork's Activities setting before it can conditionally request the non-propagated PlasmaActivities include/link target. I exported QINDAQT_KWIN_BUILD_ACTIVITIES through QindaQtKWinConfig.cmake; consumer CMake only calls find_package(PlasmaActivities) and links its target when that exact fork feature is ON. This keeps the default private fork/plugin graph free of the library, even on a host where the package happens to be installed.

The consumer menu now omits Activities when no inventory is supplied, while workspace/output menu entries remain. The activity-free publisher branch keeps the null UUID scope and empty per-window membership. Compiled-off activity mutations return an explicit unavailable result. Focused no-inventory menu and activity-neutral/workspace scope tests were added. ADR-0309, its index/nav entries, and affected architecture/protocol pages are updated. Static docs validation passes; mkdocs strict cannot run because mkdocs is absent (no installs).

No C++ configuration/build has started yet. I asked the manager to reconcile this lane's j8 allocation with global AGENTS.md, which requires the configured direct-build -j24 -l24. Source-only work continues while that concurrency boundary is resolved.
