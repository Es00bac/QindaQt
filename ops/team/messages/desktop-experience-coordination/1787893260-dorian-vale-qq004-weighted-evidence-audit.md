# QQ-004 weighted integrated-evidence recommendation

- Timestamp: 2026-08-28T05:01:00Z
- From: Dorian Vale, KWin API and nested-session evidence auditor — OpenAI
  Codex `gpt-5.6-sol`, reasoning high
- Scope: read-only audit of the provisional nine-step QQ-004 Shell and
  customization ledger
- Evidence identity: public main
  `2c52c985f846b083c2aebb7a08f04aa8318a2912`
- Product/Git/build/runtime mutation: none; only this message and Dorian's own
  worker record changed

## Recommendation

Keep nine stable user-outcome rows and keep the milestone-level state
`EXECUTABLE`, but correct the row boundaries, three stages, and weights below.
Weights describe product value within QQ-004, not code volume, implementation
effort, candidate size, or worker activity. They total exactly 100.

Use the integrated-evidence stages consistently:

- `ABSENT`: no integrated implementation of the named QQ-004 user outcome.
- `MODELLED`: a bounded outcome-specific model/contract and deterministic model
  tests are integrated, but no live authority/consumer path is connected.
- `WIRED`: the required integrated authorities and consumers are composed, but
  no accepted end-to-end executable proof exists for the named outcome.
- `EXECUTABLE`: the accepted integrated stack operates inside its declared
  isolated runtime/package boundary, while named user-surface or broader
  qualification remains.
- `QUALIFIED`: the complete narrowly declared user outcome, including failure,
  keyboard/accessibility, persistence where applicable, and its required matrix,
  is independently accepted and integrated.

The stages are not fractional multipliers. Weighted scope should be reported by
stage; a row becomes completed scope only at `QUALIFIED` unless the manager
adopts a separate explicit conversion policy.

| Weight | Stable user outcome | Integrated stage at `2c52c985` | Exact evidence and caveat |
|---:|---|---|---|
| 15 | **Production LayerShell panel/dock publication and static work-area reservation.** Users get real top/bottom shell surfaces whose declared layer, anchors, size, exclusive zone, work-area reduction, and teardown are truthful. | `QUALIFIED` | Commit `2c52c985` is the integrated stopping point. `docs/wiki/development/testing-harness.md:198-244` records real `qindaqt-shell` roles and exact 1080p/WUXGA/1440p work-area transitions; `docs/wiki/shell/panel-surfaces.md:127-144` states the same bounded proof. This title must stay narrow: the matrix uses one output, two `never`-hidden panels and does not qualify dynamic hiding, partial panels, or heterogeneous multi-output publication. |
| 10 | **Window-aware hiding, reveal/hold, and dynamic reservation.** Panels should hide from coherent compositor window state, reveal predictably, preserve applet state, and change reservations safely. | `WIRED` | The compositor publisher, exact-owner client, policy, orchestration, interaction leases, and in-place controller path are composed (`docs/wiki/shell/panel-visibility.md:62-86`; `src/shell/runtime/shellruntimeapplication.cpp:280-297`). Production edge sensors, pointer/menu/shortcut producers, hide animation, and a live automatic-hide transition remain absent; the qualified surface matrix explicitly excludes this path (`testing-harness.md:206-214,235-244`). The pure editor transaction core does not belong in this row. |
| 8 | **Audited in-process applets: clock and notification-center entry.** Users get real locale-aware clock content and a least-authority notification entry, with failures visibly unresolved rather than impersonating functionality. | `EXECUTABLE` | The exact built-in trust root contains only clock and notification center (`src/applet_runtime/src/builtin_applet_registry.cpp:14-22`), the QML dispatcher has only those two renderers (`src/shell/qml/BuiltinAppletContent.qml:12-48`), and production panels instantiate that path (`docs/wiki/shell/panel-surfaces.md:94-110`). `testing-harness.md:183-196` says resolver and QML-cache evidence is not visual/interaction qualification. Narrow the draft title: sandboxed/process hosting and generic applet-platform authority are not delivered by these two built-ins. |
| 14 | **Bounded notification service and presentation foundation.** Notifications are admitted and retained safely, privately transported, projected into popup/Active/Recent models, persisted through Settings1 DND, and denied unless compositor-bound lock state is conclusively unlocked. | `EXECUTABLE` | The integrated composition is recorded in `implementation-roadmap.md:51-63`, and the completed policy/service slices are named in `docs/TASK_LIST.md:27-33`. Private-D-Bus, real Qt transport, process lifecycle, model/policy, lock authentication, and offscreen QML evidence is recorded in `testing-harness.md:246-346`. This is an executable foundation, not a qualified desktop surface: those checks open no production notification surface or inject input (`testing-harness.md:343-354`). |
| 10 | **Installed live notification interaction.** Users can open the center through the real shortcut, receive focus, navigate and operate it by keyboard, persist DND across Settings1/shell replacement, and retain privacy through real nested lock transitions. | `WIRED` | Production authorities and consumers are composed in `src/shell/runtime/shellruntimeapplication.cpp:201-278,304-330`, but `docs/TASK_LIST.md:10-17` and `docs/HANDOFF.md:50-57` make the required private nested 1080p/WUXGA/1440p workflow the active incomplete outcome. Any candidate branch, worker run, or source-ready claim contributes zero before exact review, manager integration, and integrated verification. |
| 10 | **Global application menu.** The focused application's exported actions and menus appear in a usable, keyboard-accessible shell menu with truthful ownership and fallback behavior. | `ABSENT` | Public main has only profile hints and `data/applets/global-menu.json`. `docs/wiki/shell/applet-runtime.md:51-75` says the manifest resolves `implementation-unavailable`; the compiled registry and QML dispatcher have no global-menu implementation. A generic manifest/schema is a prerequisite, not an outcome-specific menu exporter, provider, focus owner, renderer, or test. Change the provisional `MODELLED` stage to `ABSENT`. |
| 14 | **Launcher, task list, status tray, and remaining system applets.** Users can launch/discover applications, see and control real windows, consume status items, and operate audio/power/network/Bluetooth/clipboard controls through least-authority service boundaries. | `ABSENT` | Launcher, task-list, and status-tray are manifest/catalog identities only and explicitly resolve `implementation-unavailable` (`applet-runtime.md:51-75`). Other profile labels are static or missing-manifest representations; the integrated Audio1 backend is adjacent platform infrastructure and does not create an Audio applet. Generic manifests, profiles, fixtures, and service capability names contribute no stage to these user experiences. Change `MODELLED` to `ABSENT` and give this daily-use cluster more product weight. |
| 11 | **Direct WYSIWYG customization, preview/apply, and reveal affordances.** Users drag or keyboard-place panels/applets from Settings, see valid provisional results in the real shell, undo/redo, commit/cancel, persist derived profiles, and reveal hidden surfaces. | `MODELLED` | Unlike generic manifest identities, `src/shell_customization/**` is a genuine outcome-specific transaction model with manifest-aware preflight, immutable snapshots, preview, rollback, undo/redo, and failure atomicity (`layout-profiles.md:110-163`; `testing-harness.md:167-181`). It remains model-only because it constructs no settings UI or shell surfaces, persists no user profile, and the shell does not subscribe to provisional snapshots (`layout-profiles.md:165-171`). Move the draft's edit-transaction credit here; no WYSIWYG UI is claimed. |
| 8 | **Whole-shell multi-output, DPI, theme, keyboard, and accessibility qualification.** Every stock workflow remains usable and recoverable across required output/topology/theme/input/accessibility matrices. | `WIRED` | Profiles, themes, logical mixed-DPI planning, shell surfaces, and structural keyboard/accessibility paths exist, while only the narrow panel slice has nested proof. `testing-harness.md:690-705` requires single/multi-output, mixed-scale/rotation/hotplug/profile/theme coverage, and `implementation-roadmap.md:97-100` requires failure, keyboard/accessibility, persistence, focused tests, nested display evidence, and docs. Five points materially underweights this cross-cutting user outcome; eight remains conservative. |

Weight check: `15 + 10 + 8 + 14 + 10 + 10 + 14 + 11 + 8 = 100`.
The integrated distribution is `QUALIFIED: 15`, `EXECUTABLE: 22`, `WIRED: 28`,
`MODELLED: 11`, and `ABSENT: 24`. That distribution is not a completion
percentage.

## Exact corrections to the provisional draft

1. QQ-004.01: change weight `18 -> 15`; retain `QUALIFIED`, but narrow the
   title/summary to static single-output publication/work-area proof and add the
   unqualified dynamic-hide, partial-panel, and heterogeneous-output caveat.
2. QQ-004.02: change weight `12 -> 10`, title it around window-aware hiding,
   reveal/hold, and dynamic reservation, and change `EXECUTABLE -> WIRED`.
   Remove preview/rollback/undo-redo credit from this row and move it to .08.
3. QQ-004.03: change weight `10 -> 8`; retain `EXECUTABLE`, but narrow it to the
   two audited in-process built-ins. Do not call generic/sandbox applet hosting
   live.
4. QQ-004.04: change weight `15 -> 14`; retain `EXECUTABLE` only with
   “foundation” and isolated/private-bus/offscreen limits explicit.
5. QQ-004.05: retain weight `10` and `WIRED`. Remove “source-ready worker tree”
   from product stopping-point prose; candidate state is not integrated evidence.
6. QQ-004.06: retain weight `10`, change `MODELLED -> ABSENT`, and replace schema
   evidence with the exact `implementation-unavailable`/no-renderer evidence.
7. QQ-004.07: change weight `10 -> 14` and `MODELLED -> ABSENT`. Generic catalog
   identities and visual fixtures are zero product behavior.
8. QQ-004.08: change weight `10 -> 11`, retain `MODELLED`, and make it the owner
   of the genuine editor transaction-core credit now mixed into .02.
9. QQ-004.09: change weight `5 -> 8`; retain `WIRED`. This is a required product
   acceptance outcome, not incidental test effort.

The milestone stopping point should also stop saying that “panels and applets”
share the qualified three-resolution proof. That matrix qualifies panel roles
and work-area causality, while the two built-ins are only executable. Replace
“source-ready” notification language with the public truth: production
components are integrated, and the installed live interaction matrix remains
incomplete.

## Read-only evidence checks

- `HEAD` and `origin/main` resolve to exact `2c52c985...`; the only manager
  worktree dirt observed was the provisional untracked `ops/team/features.json`
  and `tools/team-board/`, not a product-source mutation.
- Exact-tree enumeration found the global-menu/launcher/task-list/status-tray
  manifests but only two compiled renderer entry points. An exact-tree grep
  found the unavailable applet IDs only in resolver tests, not production
  registry/renderer implementations.
- The public roadmap, task list, handoff, profile/panel/applet/notification
  authority, ADRs 0002/0006-0012, testing matrix, current shell composition,
  built-in registry, and QML dispatcher were inspected. No candidate branch or
  live worker state was used to advance a stage.
- No build, test, UI, compositor, nested session, session bus, input, display,
  host configuration, or Git mutation was performed for this audit.

Manager action: adopt the corrected nine-row ledger on the next evidence-backed
`features.json` reconciliation. Future movement requires an accepted exact
commit, manager integration, verification on the integrated tree, and an exact
stopping-point citation; activity and preserved candidates remain zero.
