# Nettie Stevens — independent first-party exact-candidate review

- Persona: **Nettie Stevens**, independent first-party reviewer
- Provider/model: Moonshot Kimi `kimi-code/k3`
- Exact candidate SHA: `bfe6009950187901e4f42902aa211ab3378419aa`
- Tree SHA: `d991e598b48375caeadd518d221ba1364b838ed5`
- Parent SHA: `e01fcd16a07fd6a07251cb111b3afe4e55bf41bc`
- Base SHA: `86ad7c3854a5e35efb0a3b3e65444c432448fa0e` (integrated on main)
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/first-party-menu-export-k3-review`
- Assigned build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-k3`

`git rev-parse HEAD` equaled the exact candidate and `git status --porcelain`
was empty before review and after all work. No product path was edited; all
scratch reproductions live under the assigned build root.

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

The candidate repair was reviewed line by line
(`src/app_shell/menu_export/src/application_menu_export.cpp`, 478 non-blank
lines): `classifyRegistrationCompletion` admits only a `ReplyMessage` with an
exactly empty signature as confirmation, treats bus transport/lifecycle errors
(`ServiceUnknown`, `NoReply`, `BadAddress`, `NoServer`, `Timeout`, `NoNetwork`,
`Disconnected`, `TimedOut`, `NameHasNoOwner`) and malformed/invalid messages as
uncertain, and treats every other method-level error (`AccessDenied`,
`UnknownMethod`, `InvalidArgs`, ...) as an authoritative refusal that clears
the debt. Uncertain attempts keep `pendingRegistration` until exactly-once
compensation in `retirePublishedIdentity`, which targets the recorded exact
owner of the attempt rather than the current owner, and serial fencing makes
superseded replies inert. This matches the lifecycle contract added to
`docs/wiki/shell/global-menu.md:306-338` row by row.

## Review question 1 — prior reproductions rerun against the candidate

The three prior verdicts' reproductions under
`/home/cabewse/work_SPaC3/builds/qindaqt/review-menuexport-codex/repros/` were
copied verbatim into `<ROOT>/repros/` with only `QINDAQT_SOURCE`/`QINDAQT_BUILD`
repointed to this review's worktree and Debug build, then configured, built,
and run under `dbus-run-session` with host display/bus variables unset,
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`, `QT_QPA_PLATFORM=offscreen`,
`QT_FATAL_WARNINGS=1`:

- `window-recreation` (r1 surface-recreation defect): predicate exit 1,
  `NOT REPRODUCED`; observed `surfaceCreated=2 surfaceDestroyed=1
  publishCount=2 withdrawCount=1 oldId71=absent newId72=registered`.
- `inflight-registration` (r2 in-flight defect): predicate exit 1,
  `NOT REPRODUCED`; observed `registerCalls=71,72 unregisterCalls=71
  publishCount=2 withdrawCount=1 registeredWindowId=72 status=3` — withdraw 71
  exactly once before 72, late reply inert.
- `timeout-registration` (r3 uncertain-timeout defect): predicate exit 1,
  `NOT REPRODUCED`; observed `register71=1 unregister71AfterWithdrawal=1
  unregister71AfterLateReply=1 registrarStillHolds71=false publishCount=1
  withdrawCount=1`. The predicate's `timedOutBeforeReply=false` sub-check
  measures the *old* failure code `registrar-registration-failed`, which the
  repair correctly no longer emits; the contract-relevant values show the
  uncertain attempt compensated exactly once at withdrawal.
- `malformed-registration-reply` (r3 malformed-success defect): predicate exit
  1, `NOT REPRODUCED`; observed `attempts=1 registrarRecorded=false status=1
  published=false registeredWindowId=0
  failureCode=registrar-registration-uncertain` — malformed success stays
  unpublished and uncertain.

## Review question 2 — lifecycle table attacks and negative controls

Every transition row in the new lifecycle table names an executable row that
exists and passes (`ApplicationMenuExportInFlightTest` ×5,
`ApplicationMenuExportSurfaceTest` ×3, `ApplicationMenuExportTest::
retriesOnRegistrarReplacementAndTearsDownOnClose`,
`rejectedCloseKeepsLiveMenuPublished`,
`TerminalMenuExportTest::shellFencesRealTerminalIdentity(matching-pid-and-window)`).

Negative controls I executed myself: the exact candidate test source
`tests/app_shell/tst_application_menu_export_inflight.cpp` was compiled and
linked against the pre-repair libraries in scratch projects under
`<ROOT>/negative-controls/` (old sources taken read-only from the exact
`9becfb1e` review worktree and the exact `ba88f0b1` tree):

- Against exact `9becfb1e` (reply-classification repair absent): exit 2,
  **5 passed / 2 failed** — exactly
  `acceptedButNeverRepliedTimeoutIsCompensatedExactlyOnce` and
  `malformedSuccessPayloadRemainsUncertainUntilWithdrawal` fail, so the two
  repair rows are non-vacuous; the other three pass.
- Against exact `ba88f0b1` (in-flight compensation absent): exit 4,
  **3 passed / 4 failed** — `surfaceDestructionCompensatesInFlightRegistrationExactlyOnce`,
  both classification rows, and
  `ownerChangeMidRegistrationCompensatesSupersededAttempt` all fail, so the
  owner-change row's compensation mechanism is also proven non-vacuous.

Independent hostile attacks I wrote (`<ROOT>/attacks/hostile-registrar/`,
private bus, offscreen, `QT_FATAL_WARNINGS=1`), exit 0, `ALL HOSTILE ATTACKS
HELD`:

- **A — UnknownMethod refusal**: registrar answers `RegisterWindow` with
  `org.freedesktop.DBus.Error.UnknownMethod`; observed `registerCalls=1
  unregisterCalls=0 withdrawCount=1`, export waits with
  `registrar-registration-failed`, never publishes, and neither surface
  destruction nor `stop()` sends any compensation — explicit refusals are not
  compensated.
- **B — owner loss to no replacement mid-flight**: a NeverReply registrar
  accepts 71 and releases the name unanswered; the old exact owner observes
  `unregister:71` exactly once, the export falls back to
  `registrar-unavailable`, a later replacement owner receives a fresh
  registration (72), and `stop()` compensates the new owner exactly once.
- **C — late hostile error after recreation**: id 71's reply is a delayed
  error arriving only after surface destruction compensated 71 and the
  recreation confirmed 72; observed `registerCalls=2 unregisterCalls=2`, the
  export remains published with id 72, no double compensation, and `stop()`
  compensates 72 once.

No transition in the table could be broken; every repair-sensitive row has a
demonstrated failing negative control.

## Review question 3 — first-party composition and Terminal teardown

Terminal (`src/apps/terminal/main.cpp:328`) and Text Editor
(`src/apps/text_editor/main.cpp:210-212`) both compose the shared
`QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport` with their own
coordinator, platform `QWindow`, and session bus; File Manager now routes
through the same shared entry. The 91-test selector includes the real-process
exact-identity rows for all three applications, the mismatched PID/window
variants, the registrar-absent and hostile-registrar rebinding rows, the
identity-variant source-policy rows, and the full Terminal PTY/session/
process-group teardown suite — all green in Debug and Release (below).

## Review question 4 — builds, selectors, static gates

Configure (exact lane recipe, system KWin 6.6.6 / Qt 6.11.1 / KF6 cache):
Debug exit 0, Release exit 0; only the expected Qt GuiPrivate exact-version
warning accepted by ADR-0068.

Focused builds (`--parallel 3`, target list from the handoff plus
`tests/app_shell/all` needed for three selector rows): Debug exit 0, Release
exit 0.

Selector, both configurations, with `DBUS_SESSION_BUS_ADDRESS`, `DISPLAY`,
`WAYLAND_DISPLAY`, `WAYLAND_SOCKET` unset and
`DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

```sh
ctest --test-dir <cfg> -N -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)'
ctest --test-dir <cfg> -R '^qindaqt\.(terminal|editor|text-editor|app-shell|global-menu-|file-manager)' \
  --output-on-failure --no-tests=error
```

- Debug discovery: exit 0, 91 tests. Execution: exit 0, **91/91 passed**, 0
  failed, 46.12 s. (A first run reported 88/91 passed with three `Not Run`
  rows — `app-shell-action-registry`, `app-shell-coordinator`,
  `app-shell-surface-offscreen` — because my initial target list omitted
  `tests/app_shell/all`; after building it, the rerun above passed all 91.)
- Release discovery: exit 0, 91 tests (identical name set). Execution: exit 0,
  **91/91 passed**, 0 failed.

Direct candidate rows under `dbus-run-session` isolation:

- Debug `qindaqt_app_shell_menu_export_inflight_tests`: exit 0, 7 passed /
  0 failed / 0 skipped.
- Release same binary: exit 0, 7 passed / 0 failed / 0 skipped.

Static gates from the worktree root:

- `./tools/validate-docs`: exit 0 — 144 Markdown documents plus `mkdocs.yml`.
- `mkdocs build --strict --site-dir <ROOT>/site` (docs venv): exit 0, 1.91 s.
- `./tools/check-source-shape`: exit 0 — no changed path flagged; only
  pre-existing decomposition warnings on untouched paths.
- `git diff --check`: exit 0.
- No JSON changed in `86ad7c38..HEAD`, so no `python3 -m json.tool` applied.

## Safety boundary

No nested-compositor/`tests/session` row, host D-Bus system/session service,
display server, hardware, uinput, or network was touched. All runtime evidence
used offscreen Qt and private `dbus-run-session` buses. Scratch sources,
builds, and outputs exist only under the assigned build root; no product file
was edited, and the worktree remained exactly at the candidate with empty
porcelain throughout.

## Verdict

ACCEPT: the fail-closed reply classification repairs both r3 manifestations
without regressing the r1/r2 lifecycle repairs; all prior reproductions are
resolved, every lifecycle-table transition holds under hostile attack with a
demonstrated failing negative control, and the full Debug/Release selector and
static gates pass.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
