# ACCEPT — shell candidate 7d2a1601f58ed70091396883fb98be742529c132

- Timestamp: 2026-09-27T16:45:47.912472+00:00
- Reviewer: recovery-audit-codex (different from shell implementer)
- Review worktree: container-wm-performance-review, detached exact candidate.
- Scope: controller getter/publication behavior, row snapshot regression, CMake registration, task-list and iconography docs.

Both QVariantList getters now return bounded implicitly shared snapshots. Constructor initializes them through reproject; state/order/scope/intents all reproject; no other production path mutates m_projection. Both snapshot lists are rebuilt before stateReprojected, preserving same-generation metadata and permissions. Read-denied projection stays empty. Metadata lookup remains fresh at the next publication, including icons previously unresolved. No public API or icon path-confinement policy changed.

Independent execution: compared 90 tracked files under src/shell/task_list and tests/shell/task_list byte for byte with exact candidate, no differences, then executed existing worker build's focused three CTest gates. Command: `ctest --test-dir /home/cabewse/work_space/container-wm-performance-worker/build/performance --output-on-failure -R '^qindaqt.task-list-applet-(controller|ungrouped|row-snapshots)$' -j1` — exit 0, 3/3 passed. This is independently executed verification using the existing matched-source build, not a claimed clean rebuild.

No blocking findings. Narrow acceptance removes repeated filesystem resolution on QV4 reads; stable delegate lifetime, production packaging, physical frame-rate measurements, and publication-rate behavior remain manager/follow-on scope. Request integration of this exact candidate and rerun affected gates on integrated tree.
