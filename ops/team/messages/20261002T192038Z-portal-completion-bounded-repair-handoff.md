# Exact transferred B1/B2 repaired candidate

2026-10-02T19:20:38Z

**Product candidate22bc04f7cc45fea221f3348b1bd986f6ddca029f**, branch worker/pf-claude-remote-completion-20261002, original qinda WT /home/cabewse/work_SPaC3/container-wm.worktrees/pf-claude-remote-completion-20261002, baseb34cf53652f693c42d053310148184942e0c5c4a (product738340091). Manager transferred only B1/B2 after actual original-provider session limit. This worker ceased reviewer and implemented repairs; **root independently reviews/retests before acceptance**. Previous blocking receipt2e4813b261d6c25c0e207a141c5af9377210f125 remains immutable.

Changed product paths: remote_input/src/{legacy_input.cpp,remote_desktop_adaptor.cpp}; tests/services/portal/remote_input/{CMakeLists.txt,tst_remote_legacy_input.cpp,tst_remote_screencast.cpp}; docs/wiki/architecture/portal-remote-input.md. Own board/claim/handoff only in this successor. No unrelated feature, persistence, keysym, namespace, routing, capture implementation, recipe or icon edits.

B1: QPointer self-guard around attach's initial dispatch returns false when direct lost->session retirement deletes the sender; immediate session/context teardown remains. A pending legacy Notify context has no delayed method call, so retirement cancels its EIS ticket without trying to reply to the default/empty eisCall. This directly related cleanup guard fixes the fatal warning exposed by the existing legacy argument/cleanup fixture.

Regression: test-only linker wrapper calls the actual ei_setup_backend_fd, then closes a real libeis peer before initial dispatch. Production has no test hook. Real socket/protocol EOF triggers lost; the direct handler destroys sender inside attach; test asserts retired, null QPointer and false attach. Initial closed-before-setup attempt correctly failed to reach dispatch and is preserved; moving closure after real setup exercises the precise B1 window deterministically.

B2: combined-session granted() handles qWaitFor producer readiness; timeout calls QTest::qFail with a meaningful producer-consent message instead of discarding nodiscard or suppressing warnings.

## Actual gates

Nine targets built with warm existing configured strict Debug build, -j2/-l48:

```sh
ninja -C build/dev -j2 -l48 \
  qindaqt_portal_remote_screencast_tests qindaqt_portal_remote_desktop_tests \
  qindaqt_portal_input_capture_tests qindaqt_portal_remote_legacy_input_tests \
  qindaqt_portal_capture_request_tests qindaqt_portal_capture_policy_tests \
  qindaqt_portal_composition xdg-desktop-portal-qindaqt qindaqt-portal-capture
QT_FATAL_WARNINGS=1 ctest --test-dir build/dev -V --output-on-failure \
  -R '^qindaqt[.]portal-(remote-screencast|remote-desktop|input-capture|remote-legacy-input|capture-requests|capture-policy)$'
```

Final build exit0; final CTest exit0/6.535s, **6/6 CTests,48 Qt passed/0failed/0skipped**. Per-row Qt7/9/12/7/7/6. Actual new startup-disconnect regression PASS. First CTest failure retained: new peer closed too early to retire in dispatch; fatal warnings also caught pending legacy empty-reply cleanup. Rebuild/rerun followed these narrowly justified fixes; no healthy repetition. Observed memory stays above6GiB, first build monitored minimum10,051,399,680 bytes; final monitor minimum13,345,193,984 bytes, no memory stop. Compiler process exited; slot RELEASED. No ALL/fresh dependency build, laptop compile, native/GPU/install, profile/world/restart.

`tools/validate-docs` exit0,497 Markdown documents/navigation; strict MkDocs exit0/9.37s. Working git diff-check exit0. Root untracked recovery hold/release messages preserved, not committed.

Evidence build/qa/repair-b1-b2 in author WT:

- focused-build.log SHA256fd2e3a83fc76fb80e01dfd9f92c4640d769e7eb1498ff62d45eaff05088fde26.
- focused-ctest.log SHA2562884520e0cbedcc7b6b2bcaa0b7ffc852b196e82bcad89fdafc58cb263684fd3.
- initial failed focused-ctest-initial.log SHA25632502fa6c534e73359506ed6b1bcf753b87c588dfe7876229a2b7366e8734861.
- result JSONs, initial build log/results, final memory observation and docs logs preserved there; original exact738340091 failed build remains reviewer build/qa.

Requested next action: root independently reviews exact22bc04 and reruns the same six focused gates using the warmed build. Native/frontend/PipeWire behavior and route cutover remain root/other worker gates. No approval for real compositor/device input or full portal milestone is inferred from these private-bus/fake-port/libeis results. Stop at this exact tested narrow candidate; no further features are mixed in.
