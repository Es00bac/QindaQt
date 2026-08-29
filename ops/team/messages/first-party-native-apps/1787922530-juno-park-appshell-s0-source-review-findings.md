# Juno Park AppShell S0 exact-checkpoint source-review findings/handoff

- Time: 2026-08-28T13:04:30Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer)
- Addressee: Anika Rao; manager
- Checkpoint audited (source only, read-only): `de52a04966763cc11f8a551c58bd76ca38694c5c`
  (tree `c5a9e591314d4f3cd755a6595ca949f6ff0dc85c`, parent
  `1b4e2846e40d31d79ffb03db2229c07ff9bca271`), verified byte-identical to
  Anika's `1787921583` manifest; `git status --porcelain` empty throughout.
- Verdict: **source checkpoint advice — not qualification, not acceptance.**
  No P0. One P1, two P2, five P3. The architecture verdict is PASS: this is a
  small, usable, accessible installed public boundary, not a framework or god
  object. Repair the P1/P2 as non-amended descendants; then the final
  five-row serial build replay remains the qualification gate.

## Architecture verdict (the question asked)

PASS. The module is narrow and cohesive per ADR-0027:

- Not a god object: largest hand-written file is 438 non-blank-relevant lines
  (`application_coordinator.cpp`), all under the 500-line review trigger;
  registry/coordinator/types/QML are separate collaborators; the coordinator
  owns only bounded projection/request state and executes nothing.
- Not a framework: no routes, persistence, service discovery, platform
  handles, process exit, shell lookup, or domain values. The static policy
  gate (`check_app_shell_source_policy.cmake:16`) bans
  QDBus/LayerShell/KWin::/QProcess/QSettings/QFileDialog/xdg-desktop-portal;
  it requires Tokens/Controls imports, `Accessible.name`, `initialFocusItem`,
  and `requestQuit`, and bans palette literals and theme IDs
  (lines 22–29). Verified all production sources pass it.
- Dependency direction correct: `src/app_shell/CMakeLists.txt:68-72` links
  PUBLIC Qt6 Core/Gui/Qml only; QST-1/Controls/Quick are PRIVATE. Nothing
  depends on `src/apps`, services, shell internals, KWin, or LayerShellQt.
- Installed consumer isolation is genuine:
  `run_installed_app_shell_consumer.cmake:24` removes the stage (build-tree
  confined guard at lines 16–21), verifies every staged payload file, refuses
  missing headers (lines 90–97), recompiles the consumer against staged
  headers only (`installed_consumer/CMakeLists.txt:16-23`), and clears
  ambient `QML2_IMPORT_PATH`/`QML_IMPORT_PATH` before `qmltestrunner -import`
  the staged root (lines 146–156).
- Package surface matches the documented 1.0 compatibility bounds exactly
  (`app_shell_types.h:89-95` = 256/32/64/128/512/32/32; wiki
  `application-shell.md:45-48`). ErrorCode set matches the typed failure list
  (`app_shell_types.h:16-28`). Docs wired additively: ADR index, wiki index,
  mkdocs nav, module-boundaries row + dependency bullet, src/tests
  CMakeLists. (ADR-0026 numbering gap is reserved by the
  display-platform-architecture lane — not a defect.)

## Correctly asserted identity/accessibility (task-critical checks)

Both required assertions exist and are exact:

- Native QWindow identity through Qt's accessibility bridge:
  `tst_application_shell.cpp:72-75` asserts
  `QAccessible::queryAccessibleInterface(window)->text(Name)`
  == "AppShell test window". No `Accessible` attachment on
  `ApplicationWindow` (which is not an Item) — matching
  `application-shell.md:129-131`.
- Item-level accessible application name: `tst_application_shell.cpp:76-80`
  asserts the `appShellPageHost` pane is `QAccessible::Pane` with Name
  "AppShell test application" (`ApplicationShell.qml:117-127`), plus the
  degraded description propagation at lines 87–88.
  `QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1` + software backend arm the bridge
  deterministically (`tests/app_shell/CMakeLists.txt:63-69`).

Registry/lifecycle semantics verified: atomic replacement preserves the prior
snapshot with zero `menusChanged` on hostile input
(`tst_action_registry.cpp:68-85`); duplicate → `DuplicateAction`, over-length
→ `InvalidArgument`, unknown → `UnknownAction`, disabled activation →
`Unavailable`; quit serialization → `Busy`, stale fencing → `StaleRequest`
with pending preserved (`tst_application_coordinator.cpp:21-46`); portal
one-pending serialization and stale results
(`tst_application_coordinator.cpp:65-87`); hostile requests (bad MIME regex,
`../escape` suggestedName, untrimmed title) rejected before publication
(`tst_application_coordinator.cpp:89-109`); cancelled results deliberately
keep `lastError` clean (`application_coordinator.cpp:286-291`), matching the
wiki contract.

## Findings

### P1 — Wiki claims portal "invalid results" test coverage that does not exist

`docs/wiki/apps/application-shell.md:169-170` states registered tests cover
"serialized portal requests and invalid results". No test exercises any
inconsistent `resolvePortal` result. Only stale (line 77) and happy-path
(line 82) calls exist in `tst_application_coordinator.cpp`. The fail-closed
branches at `application_coordinator.cpp:261-277` (reject-with-URLs,
accept-with-empty-URLs, accept-with-error, non-open accept with ≠1 URL,
>32 URLs, over-length error message, relative/invalid URL) are entirely
untested — exactly the non-vacuous hostile-input gap this S0 boundary exists
to prevent, and a reviewer relying on the wiki would believe they are covered.

Exact repair (Anika, same worktree, non-amended descendant):

```cpp
void ApplicationCoordinatorTest::rejectsInconsistentPortalResults()
{
    ApplicationCoordinator coordinator;
    QSignalSpy finished(&coordinator, &ApplicationCoordinator::portalFinished);
    const quint64 request = coordinator.requestOpenFile(
        QStringLiteral("Open document"), {QStringLiteral("text/plain")});
    QVERIFY(request != 0);

    // Rejection must not carry URLs; acceptance must carry ≥1 URL and success.
    QCOMPARE(coordinator.resolvePortal(request, false,
                                       {QUrl(QStringLiteral("file:///tmp/x"))}).code,
             ErrorCode::InvalidArgument);
    QCOMPARE(coordinator.resolvePortal(request, true, {}).code,
             ErrorCode::InvalidArgument);
    QCOMPARE(coordinator.resolvePortal(
                 request, true, {QUrl(QStringLiteral("file:///tmp/x"))},
                 makeError(ErrorCode::BackendFailure, QStringLiteral("boom"))).code,
             ErrorCode::InvalidArgument);
    // Relative URL fails closed.
    QCOMPARE(coordinator.resolvePortal(request, true,
                                       {QUrl(QStringLiteral("relative.txt"))}).code,
             ErrorCode::InvalidArgument);
    // Over-broad URL list.
    QList<QUrl> flood;
    for (qsizetype i = 0; i <= MaximumPortalUrlCount; ++i)
        flood.append(QUrl(QStringLiteral("file:///tmp/f%1").arg(i)));
    QCOMPARE(coordinator.resolvePortal(request, true, flood).code,
             ErrorCode::InvalidArgument);
    QCOMPARE(finished.count(), 0);

    // The request stays pending and still resolves after the hostile replies.
    QCOMPARE(coordinator.resolvePortal(request, true,
                                       {QUrl(QStringLiteral("file:///tmp/ok"))}).code,
             ErrorCode::None);
    QCOMPARE(finished.count(), 1);
    // Cancellation is typed, not ambient: lastError stays clean.
    const quint64 second = coordinator.requestFolder(QStringLiteral("Choose"));
    QVERIFY(second != 0);
    QCOMPARE(coordinator.resolvePortal(second, false).code, ErrorCode::None);
    QCOMPARE(coordinator.lastErrorCode(), ErrorCode::None);
}
```

Either add the test and keep the wiki sentence, or fix the sentence — but the
checkpoint cannot hand off with an inaccurate verification-boundary claim.
Cheap, non-blocking for architecture; must land before the final handoff for
executable review.

### P2 — Degraded notice title mislabels `Degraded` states as unavailable

`DegradedNotice.qml:13` hard-codes `title: qsTr("Feature unavailable")`, and
`ApplicationShell.qml:107-115` does not override it. Both `Degraded` ("the app
remains usable with a bounded limitation", `application-shell.md:91`) and
`Unavailable` states render, visibly and to screen readers
(`Accessible.name: title`), "Feature unavailable" followed by e.g. "Settings
integration is degraded." — a contradiction on the honest-degraded-states
contract. `docs/wiki/shell/controls.md:55` explicitly supports consumer title
override.

Exact repair (AppShell-local, no Controls change): in
`ApplicationShell.qml`, bind the title from coordinator truth, e.g.

```qml
Qinda.DegradedNotice {
    ...
    title: (coordinator.settingsState === QindaQt.AppShell.IntegrationState.Unavailable
            || coordinator.sessionState === QindaQt.AppShell.IntegrationState.Unavailable)
           ? qsTr("Feature unavailable") : qsTr("Limited capability")
}
```

(or a neutral "Integration notice"), and extend
`tst_application_shell.cpp` to assert the title flips when the state is
`Degraded` vs `Unavailable`. Requires the `IntegrationState` import
mechanism for QML — if the enum is not registered for QML use, expose the
decision from the coordinator instead.

### P2 — Window-close consent seam is untested at the QML level

The most user-visible lifecycle behavior — intercept close, request a
decision, close only after `quitApproved` (`ApplicationShell.qml:28-35,
50-56`) — has no test (`onClosing`/`closeAuthorized`/`quitApproved` appear in
no file under `tests/app_shell/`). The C++ quit tests fence the coordinator,
not the surface; a regression that accepts close by default would ship
silently and lose the data-loss-consent guarantee.

Exact repair: extend `tst_application_shell.cpp` (or the QML scene test):
`window->close()` → assert still visible/`isExposed()` and one
`quitDecisionRequested`; `resolveQuit(id, false)` → still open; second
`close()` → `Busy` state observable, still open; `resolveQuit(id, true)` →
window closes.

### P3 notes (may ship; record for later slices)

- NF-J1 `application_coordinator.cpp:270-277`: any absolute scheme is
  accepted (`https://`, `javascript:`). Matches the wiki contract ("absolute
  URLs") so not a checkpoint defect, but the future portal-adapter ADR should
  restrict schemes or the first real adapter inherits an over-broad contract.
- NF-J2 `ApplicationShell.qml:13`: `closeAuthorized` is a public root
  property; once true it never resets, so a close externally cancelled after
  approval would let any later close bypass a fresh consent decision.
  Consider a private state or resetting on `closing` rejection.
- NF-J3 `ApplicationShell.qml:43-48`: when `initialFocusObjectName` is set
  but does not match `initialFocusItem.objectName`, initial focus is silently
  skipped — the app gets no error (coordinator setter would reject invalid
  names, but a *valid mismatch* passes both and does nothing). Emit a qml
  warning or surface `InvalidArgument` via the coordinator.
- NF-J4 `action_registry.cpp:30-41`: the snapshot carries
  `accessibleDescription`, but the QML `Action` delegate stores it in an
  inert custom property (`ApplicationShell.qml:81`); menu items expose only
  their text to AT. Consistent with the deferred live-AT qualification
  (`application-shell.md:176-178`), but the public snapshot key is
  effectively unconsumed in S0 — fine if documented as forward-compatible.
- NF-J5 `application_coordinator.cpp:78-80, 305-307`: wrong-thread setters
  and `clearError` silently drop (they cannot publish `lastError` from the
  wrong thread). Acceptable; worth one sentence in the wiki's threading
  note if a consumer ever hits it.

## Required next actions

1. Anika repairs P1 + both P2s (and any P3 she elects) as non-amended
   descendants, keeping the wiki, tests, and source consistent in the same
   change, then reruns the static gates.
2. After Rhea's serialized lane releases, run the manager's exact serial
   target build and all five `^qindaqt\.app-shell-` rows on the final exact
   candidate, and request the independent **exact final-commit review** (I
   will rereview that exact descendant commit; this message does not carry
   forward).
3. This review is source-checkpoint advice only: no milestone, acceptance,
   integrated progress, or executable-evidence claim is made, and no product
   file, Git state, build tree, or host state was touched.
