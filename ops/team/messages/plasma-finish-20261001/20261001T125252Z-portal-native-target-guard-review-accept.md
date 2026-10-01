# ACCEPT: native portal frontend build-target guard

- Exact candidate: `37347fe4cde7a91c8bd640bc57116701cdc8730a`.
- Reviewer: pf-portal-native-target-guard-review-sol-20261001, OpenAI Codex gpt-6.1-sol/high.
- Source: clean detached qinda isolated review worktree at exact candidate throughout configure gates; laptop records branch merges that candidate without implementation edits. Base `faaaf746`; production-source delta zero. Relevant changed paths: tests/services/portal/{CMakeLists.txt,foundation/CMakeLists.txt} and docs/wiki/development/testing-harness.md, plus implementer records.

The repair resolves the preserved exact139090f9 blocker. Both guarded fixture paths use the same actual public SessionSupervisorSupport target. The parent stage registration intentionally tests that already-declared support target because the frontend executable is declared later. Stage properties and appended dependencies test actual test existence. Existing consent/exporter/input/mail/relay declarations remain available. Full staged command and arguments are identical to the original after whitespace normalization; staged runners and the original dependency-contract script are unchanged. No fallback, poison assertion or runner behavior is relaxed.

## Independent evidence

Binary/log root on qinda: isolated review worktree build/review. No build or private desktop resources used.

1. `cmake -DQINDAQT_SOURCE_DIR=<exact-review-worktree> -DQINDAQT_TEST_BINARY_ROOT=<review>/dependency-contract -DQINDAQT_TEST_GENERATOR=Ninja -DQINDAQT_TEST_AUDIO_LIVE_RUNTIME=OFF -DQINDAQT_TEST_CMAKE_PREFIX_PATH=<qualified-fork690>/stage/usr -P tests/compositor/verify_kwin_plugin_dependency.cmake`: exit0, all four unchanged expected configure branches, log dependency-contract-rerun.log. Required plugin without KWin correctly fails; explicit pluginOFF/no-tests, pluginOFF/testsON and standalone bridge correctly configure. First outer wrapper failed because zsh reserves `status` after the script; its empty script log/generated branches are preserved in dependency-contract.log. Direct-command rerun changed only the wrapper and provides unambiguous exit0.
2. Fresh independent production configure: `cmake --fresh -S <exact-review-worktree> -B <review>/full-production -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_VIEWER=ON -DCMAKE_PREFIX_PATH=<qualified-fork690>/stage/usr`: exit0; full-production.log.
3. Generated CTest JSON plus actual build.ninja order-dependency assertions: exit0, presence.log. Bridge registers1008 tests, production1316; these are registration counts, not executed tests. Consent and positive/negative boundary rows each exactly1 in both; native frontend and composite stage each exactly0 bridge/exactly1 production. All3 required helper executables occur in actual consent order-dependency rows both and frontend production: consent input, mail draft, URI relay.
4. `python3 tools/validate-docs`: exit0,483 documents/navigation; `mkdocs build --strict`: exit0,9.19s. `git diff --check faaaf746 37347fe4`: exit0; unchanged original contract/staged runner source checks exit0.

No runtime/build execution is required or claimed for this configure-only fixture correction. Native runtime assertions themselves remain unchanged and root owns integrated execution. Original139 rejection remains durable history; this verdict applies only to the repaired exact descendant.

Requested next action: root integrate exact37347fe4 and rerun its unchanged combined dependency boundary. Configure-only resource is released. No compiler/private-runtime resource was claimed. Concrete compatible help: resume the preserved PF19 capture implementation source-only, including native helper/dialog tests and honest first-slice documentation; no unrelated expansion.
