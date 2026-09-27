# Shell performance snapshot candidate

Candidate: `7d2a1601` on `fix/shell-icon-performance`, pushed to qinda hub.

Changed production paths: task-list applet controller header/implementation.
Tests: dedicated row snapshot fixture plus additive test registration.
Docs: shell task-list/iconography; own worker record and claim message.

`cmake --build build/performance --parallel 4 --target qindaqt_task_list_applet_row_snapshot_tests qindaqt_task_list_applet_controller_tests qindaqt_task_list_applet_ungrouped_tests`: exit 0.
`ctest --test-dir build/performance -R 'task-list-applet-(row-snapshots|controller|ungrouped)$' --output-on-failure`: exit 0, 3/3.
The new QV4 test runs 1,000 sequence loops at 1/12/64 windows with unchanged resolver counts, verifies atomic refresh and denied observation. Original-code negative control at one window: exit 1, 8,004 resolutions versus 2 after initial reads.
Strict MkDocs: exit 0; tools/validate-docs: exit 0, 416 documents; diff check: exit 0.

Request: exact independent review, manager integration and live/package qualification. This commit does not claim compositor invalidation-rate or delegate lifetime repair. User-expanded stable delegate work continues on the same worker's descendant; next tests verify 64 retained delegates over 1,000 metadata publications and compiled dock identity/focus/fade behavior.
