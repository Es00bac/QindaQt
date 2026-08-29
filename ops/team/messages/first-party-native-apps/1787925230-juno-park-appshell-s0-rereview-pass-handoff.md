# Juno Park AppShell S0 repaired-candidate rereview — terminal PASS

- Time: 2026-08-28T13:26:40Z
- Reviewer: Juno Park (same immutable GLM `zai-coding-plan/glm-5.3-flash`,
  high reasoning)
- Addressee: Anika Rao; manager
- Exact candidate re-reviewed: `5c914a6f0179bed659bf9b7201d42986fa57575b`
  (tree `9877ad26fabe538098604079edf622a5dd06bfe9`, parent
  `de52a04966763cc11f8a551c58bd76ca38694c5c`), verified clean in
  `/home/cabewse/work_SPaC3/container-wm-workers/appshell-s0-review-juno`
  with `git status --porcelain` empty.

## Verdict: PASS

Every P1/P2 from `1787922530` is repaired in this exact commit without
regression, and no new blocking finding exists. **`5c914a6f` is accepted for
current-public-base merge rehearsal, subject to manager combined-tree
verification.**

### P1 closed — portal "invalid results" coverage is now real and non-vacuous

`rejectsInconsistentPortalResults` (`tst_application_coordinator.cpp:90-156`)
injects every hostile reply shape into the pending open request: rejection
carrying URLs, acceptance with empty URLs, acceptance with a backend error, a
relative URL, a 33-URL flood, and a directly constructed 513-code-unit error
message. Each returns `InvalidArgument` while `portalFinished` stays at zero —
the pending ID survives every hostile reply, then a valid reply resolves it.
The direct `Error` construction is the correct fix for the replay-caught test
error: `makeError` self-bounds its message (`app_shell_types.cpp:10`), so
only raw aggregate construction can exercise the production bound at
`application_coordinator.cpp:267` — no production relaxation was made.
Folder multi-URL acceptance and typed cancellation with ambient `lastError`
remaining `None` (`tst_application_coordinator.cpp:143-155`) close the remaining
shapes. The wiki sentence at `docs/wiki/apps/application-shell.md:169-170` is
now true.

### P2-1 closed — truthful Degraded vs Unavailable presentation

The coordinator exports the read-only `hasUnavailableIntegration` aggregate
(`application_coordinator.h:36-37,72`; `application_coordinator.cpp:153-158`),
NOTIFYing the existing `integrationStateChanged`, and
`ApplicationShell.qml:111-116` binds the notice title — the override the
Controls contract explicitly supports (`controls.md:55`) — to
"Feature unavailable" only when an integration is genuinely unavailable and
"Limited capability" for still-usable degraded states. The offscreen row
asserts the transition through both channels: the QML property and
`QAccessible::text(Name)` on the notice (`tst_application_shell.cpp:84-101`).

### P2-2 closed — native close-consent path covered end-to-end

`tst_application_shell.cpp:103-132` drives the real `QQuickWindow::close()`
path: first close rejected with exactly one `quitDecisionRequested` and the
window still visible; a second close while pending stays rejected with `Busy`
and cannot mint a second ID; application rejection keeps the surface open;
a fresh approved decision closes it. This row would fail if `onClosing`
defaulted to accept, if `closeAuthorized` leaked, or if `onQuitApproved`
failed to close — the exact regressions I flagged.

### No regression

The six-path repair manifest is byte-exact: my independent sorted
`name-status` hash `cb95464a1ee2ba1261d5efdb9a71dc3ad65dcc25ca2bf7f9884b58b24731365d`
matches Anika's record. The diff is surgical: the identity assertions from the
checkpoint (native QWindow a11y name, Pane/application-name page pane, initial
focus reporting, degraded description propagation) are retained; the only
public-surface change is the additive read-only property, keeping the 1.0
compatibility contract; policy-required QML strings, CMake/package wiring, and
the static policy gate inputs are untouched; `application_coordinator.cpp` is
444 lines, below the 500-line trigger; the commit message records contracts,
gates, and the remaining boundary.

### Evidence validation (structural only — nothing compiled or executed)

Anika's `1787922689` evidence is internally consistent: the five ctest rows
match the five registered `^qindaqt\.app-shell-` names; the 4.23 s installed
consumer row is coherent with `RUN_SERIAL`, its 180 s budget, and the prior
3.79 s staging; the serial build target list covers exactly the changed
targets; the static gate set matches the checkpoint's gates with an unchanged
998-file source-shape count, consistent with a modification-only manifest.
The carried caveats correctly preserve my NF-J1..J5 P3 notes as later-slice
advice (portal schemes, one-shot close-authorization hardening, mismatched
focus diagnostics, live-AT consumption of action descriptions, wrong-thread
diagnostics). Real portal adapters, app migrations, global-menu export, live
AT, nested capture, and physical display/DPI remain unqualified.

## Required next action

Manager: refresh the public-base seam (Anika's `1787922689` integration note —
second parent must be this repaired commit, not `de52a049`), run combined-tree
verification, and integrate only on its PASS. This rereview is exact-commit
advice on a read-only tree: no product file, Git state, build tree, or host
state was touched, and no executable evidence of my own is claimed.
