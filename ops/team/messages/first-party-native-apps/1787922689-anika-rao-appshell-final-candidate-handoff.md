# Anika Rao — AppShell S0 repaired final-candidate handoff

- Time: 2026-08-28T13:11:29Z
- Outcome: QQ-006.03 AppShell S0
- Exact candidate: `5c914a6f0179bed659bf9b7201d42986fa57575b`
- Tree: `9877ad26fabe538098604079edf622a5dd06bfe9`
- Parent: `de52a04966763cc11f8a551c58bd76ca38694c5c`
- Original base: `1b4e2846e40d31d79ffb03db2229c07ff9bca271`
- Worktree: clean on `worker/appshell-s0`
- Status: candidate handoff only; not independently accepted or integrated
- Requested next action: Juno Park rereviews this exact commit, then the
  manager refreshes the public-base seam and integrates only on PASS

## Exact repair manifest

The sorted six-path `name-status` manifest hashes to
`cb95464a1ee2ba1261d5efdb9a71dc3ad65dcc25ca2bf7f9884b58b24731365d`:

```text
M docs/wiki/apps/application-shell.md
M src/app_shell/include/qindaqt/app_shell/application_coordinator.h
M src/app_shell/qml/ApplicationShell.qml
M src/app_shell/src/application_coordinator.cpp
M tests/app_shell/tst_application_coordinator.cpp
M tests/app_shell/tst_application_shell.cpp
```

## Review repairs

- P1 closed: coordinator tests now inject every inconsistent portal-result
  shape: rejection carrying URLs, accepted empty result, accepted backend
  error, relative URL, 33-URL flood, overlong raw error, and multi-URL folder.
  Every hostile reply preserves the pending ID and emits no result; a later
  valid reply succeeds. Cancellation emits a typed result while ambient error
  remains `None`. The wiki's invalid-result coverage statement is now true.
- P2 notice truth closed: the coordinator exports one read-only
  `hasUnavailableIntegration` aggregate. AppShell renders and exposes
  `Limited capability` for usable degraded states and `Feature unavailable`
  only when at least one integration is genuinely unavailable. Offscreen
  assertions verify both visible property and accessible name transitions.
- P2 close consent closed: the QML-level native close row proves the first
  close is rejected while one decision is pending, a repeated close remains
  rejected with `Busy` and cannot create another ID, application rejection
  keeps the surface visible, and a fresh approved decision closes it.

The first coordinator replay correctly caught a test-construction error:
`makeError()` bounds its own message and therefore cannot manufacture an
overlong hostile public value. The test now directly constructs the malformed
public `Error`; no production relaxation was made. All evidence below is the
fresh final replay after that correction.

## Exact executable evidence

Configured build root:
`build/appshell-s0-checkpoint-de52-debug` (strict Debug, Ninja, KWin plugin and
both shell targets disabled). Final serial target command:

```sh
cmake --build build/appshell-s0-checkpoint-de52-debug --parallel 1 --target \
  qindaqt_app_shell qindaqt_app_shellplugin \
  qindaqt_app_shell_action_registry_tests \
  qindaqt_app_shell_coordinator_tests qindaqt_app_shell_qml_tests
```

Result: exit 0; final exact-commit replay reports `ninja: no work to do` after
the prior serial repair build linked all changed targets.

```sh
ctest --test-dir build/appshell-s0-checkpoint-de52-debug \
  --output-on-failure -R '^qindaqt\.app-shell-'
```

Result: exit 0, 5/5 in 4.47 seconds:

- action registry: 0.03 s
- coordinator including hostile portal results: 0.03 s
- offscreen surface accessibility, notice truth, and close consent: 0.16 s
- source policy: 0.01 s
- clean focused staged AppShell/Tokens/Controls installed consumer: 4.23 s

No compiler or CTest process remains; the serialized lane is released.

## Static/documentation evidence

- `git diff --check`: PASS before commit; worktree clean after commit.
- `qmlformat FILE >/dev/null` for all three AppShell/test QML files: PASS.
- AppShell source-policy script: PASS.
- `tools/check-source-shape`: PASS, 998 files; coordinator remains below the
  500 non-blank-line decomposition-review trigger.
- `tools/validate-docs`: PASS, 65 Markdown documents and navigation.
- `uvx --from mkdocs mkdocs build --strict`: PASS in 0.48 seconds.

## Bounded caveats and integration seam

Juno's P3 notes remain later-slice advice, not S0 blockers: portal schemes,
one-shot close-authorization hardening if a platform cancels an approved close,
valid-but-mismatched focus diagnostics, live-AT consumption of action
descriptions, and wrong-thread diagnostic publication. Real portal adapters,
concrete app migrations, global-menu export, live AT, nested capture, and
physical display/DPI remain explicitly unqualified.

The prior read-only public-base rehearsal remains valid structurally, but the
second parent is now this repaired commit, not `de52a049`. The manager must
refresh public HEAD before integration. Against rehearsed public `9db68c4`,
the only both-changed paths remain `src/CMakeLists.txt`, `tests/CMakeLists.txt`,
and `docs/wiki/architecture/module-boundaries.md`; this repair changed none of
them. Required merge order remains public first parent, exact accepted
AppShell candidate second parent.
