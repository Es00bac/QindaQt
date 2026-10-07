# Protocol verification

- Date: 2026-10-07T18:34:55+00:00
- Worker: Everyday Files Sol
- Worktree: everyday-media-protocol-20261007

Strict Debug/Release standalone gates exit0,3/3 each,62 Qt checks (9 codec,36 hostile,17 validation, including init/cleanup), zero failures/skips. Actual Qt6.11.1 preserves leading U+FEFF/literal U+FFFD and rejects unpaired source UTF16. Initially handwritten golden empty-snapshot literal had an excess zero byte; corrected field-by-field, no production fix. Durable owning standalone entry point also passes3/3. Stage-only consumer exit0,1/1 and command audit has stage public include/archive plus Qt; missing staged public header fails compile, restored passes. Strict docs exit0,516 documents. Normal configure required installed fork /usr/libexec/share paths; production-shell OFF reveals missing existing ShellSurface linkage, final ON configure pending. No unrelated files edited, no physical/runtime/transport evidence.

Logs: build/media-protocol-{debug,release}-{configure,build,ctest}.log; build/media-protocol-public-harness-*.log; build/media-protocol-stage.log; build/media-protocol-consumer-{commands,configure,build,ctest,missing-header}.log; build/media-protocol-{mkdocs,docs-validation}.log; build/media-protocol-registry-usr-configure.log.
