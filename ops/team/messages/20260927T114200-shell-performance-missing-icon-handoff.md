# Remaining workstation icon hotspot repair handoff

- Timestamp: 2026-09-27T11:41:50-06:00
- Exact candidate: `7f5b8431a8111ef43985d1e54caf231440cbbfac`
- Branch: `fix/shell-icon-performance`, pushed to qinda.
- Integration: cherry-pick this product commit; parent preserved prior worker history by merging manager base `83717eaf` at `20a21893`.
- Requested action: independent exact-commit review, manager combined gates, then workstation live A/B and overlay delivery if accepted.

Production change is confined to `src/shell/icons/src/icon_theme_locator.cpp`.
`confinedFile` rejects missing, non-file and unreadable candidates using fresh
QFileInfo metadata before canonicalizing root and candidate. Every existing
candidate still receives fresh canonical containment. No persistent lookup
cache, resolver invalidation or result semantics changed. Existing indexes,
symlink confinement and size/scale/theme priority remain intact.

Focused tests add 64 cycles of absent -> created file -> removed -> escaping
symlink -> directory -> removed on one locator, proving both positive and
negative results remain fresh. Other changed paths are the primary iconography
wiki and worker claim/live board.

## Acceptance evidence

- `cmake --build build/performance --parallel 4 --target qindaqt_shell_icons_locator_tests qindaqt_shell_icons_resolver_tests qindaqt_shell_icons_provider_tests qindaqt_shell_icons_qml_tests`: exit 0.
- `ctest --test-dir build/performance --output-on-failure -R '^qindaqt.shell-icons-(locator|resolver|provider|qml-offscreen)$'`: exit 0, **4/4**, 73 QtTest cases, 0 failures. Includes compiled QML and provider offscreen rows.
- `/home/cabewse/.local/bin/mkdocs build --strict` and `python3 tools/validate-docs`: exit 0, 417 Markdown documents.
- `git diff --check`: exit 0.

Representative benchmark uses the same injected 649-directory hicolor fixture
and 1,000 lookups for the observed missing
`qindaqt-wine-org.kde.xwaylandvideobridge` name at size 18. Parent and fixed locator
sources were compiled with C++20/O2 against the same Qt6Core. Ignored
`build/icon-benchmark/` contains bench.cpp, the parent source, count.c, binaries
and logs. The preloader counts Qt's fortified `__realpath_chk` entry point as
well as realpath; the initial unfortified-only zero count was discarded.

| Implementation | Canonical path calls | Elapsed ms |
| --- | ---: | ---: |
| Parent | 3,900,003 | 35,514 |
| Candidate | 3 | 3,167 |

This removes per-miss canonicalization and is 11.2x faster in this fixture.
Elapsed time is supporting evidence, not a brittle test assertion. Remaining
filesystem metadata queries are intentional to preserve freshness. The actual
workstation CPU after this change is **not measured yet**; manager must qualify
it on the preserved aged session. No live process, session or shared native
build was changed by this worker.
