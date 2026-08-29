# Dijkstra the 2nd — Terminal live-repair rereview midpoint

- Time: 2026-08-28T19:51:51Z
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Current counts: P0/P1/P2/P3 `0/0/0/0` (not yet final)
- Status: working

Both independent warnings-as-errors configurations pass configure and compile
for the Terminal support library, production qtermwidget adapter, executable,
and six focused test targets. Configuration uses the pinned qtermwidget 2.4
prefix and `CMAKE_AUTOMOC_PATH_PREFIX=ON`; all generated output is under
`/mnt/d/QindaQt/builds/terminal-s0-colors-rereview-dijkstra`.

With `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR`, and caller
`QT_QPA_PLATFORM` removed, `ctest -R '^qindaqt\.terminal-'` passes **9/9** in
both Debug and Release. Direct appearance execution passes **7/7** in each
configuration; direct production-adapter execution passes **4/4** in each.
The real adapter therefore proves that the `.colorscheme` path renders Qinda
dark's requested `#171a18` center pixel rather than qtermwidget's white
default, and that the pristine widget's structural LF does not publish Copy
availability.

Static source inspection independently confirms `Color0Intense` through
`Color7Intense`, the `.colorscheme` target suffix, and a predicate that ignores
only LF/CR/U+2028/U+2029 while preserving spaces and tabs as meaningful
selection. I am continuing independent parent-behavior negative controls,
package/poison and dependency confinement, source/docs/MkDocs gates, exact
identity, and clean-tree proof. No host desktop, input, session, or cursor has
been touched. PASS still routes this exact commit to Church the 3rd's retained
private-Weston 40-assertion rerun; this midpoint is not integration approval.
