# ACCEPT — f1d7b89b4fe33a274ebb2fbd934dac7a69080eac

- Time: 2026-10-01T11:21:02+00:00
- Exact candidate: f1d7b89b4fe33a274ebb2fbd934dac7a69080eac
- Base: 7a7c9fbbb9071395cbbcbf13a68c45ef17299580
- Changed product paths: tests/services/keyring/tst_lock_policy.cpp; docs/wiki/development/testing-harness.md.
- Independent qinda source: .cache/pf-keyring-idle-review-20261001, branch review/pf-keyring-idle-fixture-exec-20261001; HEAD verified exact candidate, git status clean.
- Review: refresh discards prior idle and publishes change; revoke clears availability plus idle and publishes change. Consistent with public generation/revocation contract. No existing assertions changed or weakened. Production policy remains untouched. Documentation accurately states fixture behavior.

## Executed evidence

All commands ran on independent source/build roots; no existing mutable build or borrowed libraries were used.

- cmake -S . -B build/review -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON — exit 0.
- cmake --build build/review --target tst_keyring_lock_policy qindaqt_wayland_idle_observation_tests -- -j24 -l24 — exit 0.
- ctest --test-dir build/review -R '^(keyring_lock_policy|qindaqt[.]wayland-idle-observation)$' --output-on-failure -V — exit 0; 2/2 CTests pass. keyring_lock_policy Qt totals 5 passed, 0 failed/skipped/blacklisted (three behavior cases plus init/cleanup); public idle Qt totals 13 passed, 0 failed/skipped/blacklisted (eleven behavior/data rows plus init/cleanup). Output identifies Qt 6.11.1 and GCC 15.3.0.
- mkdocs build --strict --site-dir build/review-docs — laptop independent candidate source, exit 0.
- python3 tools/validate-docs — laptop independent candidate source, exit 0; 480 Markdown documents and navigation validated.
- Post-gate CMakeCache confirms Debug, BUILD_TESTING ON, plugin OFF, strict warnings ON. qinda MAKEOPTS observed unchanged -j24 -l24.

Logs retained on qinda in independent worktree build/review/configure.log, build.log, test.log and CTest Testing/Temporary/LastTest.log.

## Bounded caveats and next action

Configure emitted unrelated installed QML missing-file/plugin diagnostics; configuration succeeded and both compiled focused gates passed. This review qualifies the six-line fixture repair, not broad product, installed, native runtime or hardware behavior. Integrate exact accepted candidate and rerun affected combined-tree gates. Compiler/private-runtime slots released immediately after execution.
