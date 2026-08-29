# Talia Ross — exact Global Menu G0 cross-provider review verdict

- **Timestamp:** 2026-08-28T18:30:00Z
- **Verdict:** **FAIL**
- **Severity:** **P0=1, P1=0, P2=0, P3=2**
- **Exact candidate:** `53490b748b90e6fe492eb15a85a5ec5805756ef4`
- **Tree:** `742e68fce27fa9734debece4085178b810efd801`
- **Sole parent:** `87cef246a690f5bdc2c860238a1feb37e10957de` (Aquinas exact
  FAIL, P2=2)
- **Local `main`:** `c4982697858c083828bd406f1aa56c4e942bcc10`
- **`origin/main` (current public tip):** `146fc48358c2659436dec4fc6b6062d23c5ee746`

The dedicated review worktree stayed detached at the exact candidate SHA/tree
throughout; no product/Git mutation, GUI/session/bus/input/config, or host
interaction occurred. All builds ran in the external, private directory
`container-wm-private-agent-runs/talia-global-menu-review`, never inside the
candidate worktree. One incidental `git stash push -u`/`apply`/`drop` cycle
was used to diff a prior blob without a working-tree `checkout`; it touched
only my own accidental stash entry (`e07035b62d7a8de8adb289e599a880b2e98aacc9`,
containing solely the untracked `.omc/` harness file), was applied back and
dropped by exact SHA per the shared-stash-stack safety protocol, and the
worktree was reconfirmed byte-clean (`git diff --stat HEAD` empty, only
untracked `.omc/`) immediately after.

## Former findings closed — hand-verified, not just test-trusted

- **P2-1 (measured-fit accounting).** Traced the arithmetic myself rather
  than relying on the test's own assertions. `horizontalLimitFor` now has a
  fast "everything fits" path plus, when overflow is possible, an
  `indicatorBlock = measuredIndicatorWidth() + root.spacing` reservation
  (`GlobalMenuApplet.qml:78-93`) — closing the exact gap Aquinas found
  (previous code reserved indicator width but not the leading spacing before
  it). `verticalLimitFor`/`indicatorFits` now require the indicator's full
  block including its 4 px top margin (`:96-120`, `:36`). I hand-computed
  `test_horizontalCalculatedEqualityBoundaryFitsExactBudget`
  (`tst_GlobalMenuAppletOverflow.qml:251-276`): at `width == exactWidth` the
  loop's `budget` reduces to exactly `measuredEntryWidth(item0)`, so item0
  is admitted (`used == budget`, not `>`) and item1 is correctly excluded;
  at `exactWidth - 1` the same item0 now exceeds budget by 1 and is excluded,
  matching `entriesMinusOne.length === 0`. Measurement itself moved from a
  mutating shared `TextMetrics.text` probe to a pure `FontMetrics`
  (`advanceWidth`/`boundingRect`) helper measuring the actual localized
  `qsTr("+%1")` string (`:39-61`), closing the binding-loop and
  untranslated-string gaps Aquinas separately flagged. Confirmed via
  `qmlformat -n` that all four touched QML files are byte-identical to their
  already-formatted state (4/4 no-op).
- **P2-2 (accessible focusability truth).** `MenuEntry.Accessible.focusable`
  is now bound to `entry.enabled` (`GlobalMenuApplet.qml:244`, was
  unconditional `true`), and
  `tst_GlobalMenuAppletAccessibility.qml:101-131` asserts `focusable ===
  false` for both submenu delegates and a disabled action while an enabled
  action reads `true`. Verified this doesn't regress keyboard behavior: Qt
  Item semantics already refuse `forceActiveFocus` on a disabled item
  (exercised by `test_keyboardFocusSkipsDisabledSubmenuEntries`,
  `tst_GlobalMenuApplet.qml:214-239`), so the accessible and interactive
  focus stories now agree.
- **Undocumented but verified-safe scope addition.** The candidate also adds
  `Keys.onReturnPressed`/`Keys.onEnterPressed` handlers
  (`GlobalMenuApplet.qml:250-251`) that are not named in the commit's P2-1/
  P2-2 description. I checked this cannot double-fire: AbstractButton does
  not itself bind Return/Enter, and
  `test_keyboardActivationOfFocusedAction` (`tst_GlobalMenuApplet.qml:191-212`)
  proves `activateCalls` advances by exactly one per `Key_Return` press
  (1→2), not two. Not a defect, but the commit message should have named it.
- Re-read the unchanged C++ lineage/authentication/exporter surface this
  candidate does not touch (`provider_authenticator.cpp`,
  `active_provider_selector.cpp`, `invocation_guard.cpp`,
  `menu_exporter.cpp`, `globalmenuappletaccess.cpp`) against
  `docs/wiki/shell/global-menu.md` and found it matches the documented
  contract exactly (TOCTOU double focus-read, opaque proof, one epoch/
  revision authority, fail-closed publish/invocation, exact hostile-rejection
  test coverage in `tst_menu_lineage.cpp`/`tst_menu_exporter.cpp`/
  `tst_menu_protocol.cpp`). No regression found in this unchanged surface.

## P0

### P0-1 — the module does not compile under the project's own mandated strict-warning gate

A private, isolated `cmake --preset dev` (`QINDAQT_ENABLE_STRICT_WARNINGS=ON`,
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror`,
GCC 16.1.1) fails at the very first compiled file of the module:
`src/shell/global_menu/protocol/src/menu_validation.cpp:89,137,160` each
return a partial C++20 designated initializer,
`ValidationResult{.accepted = true}`, against a struct whose `QString
reasonCode`/`QString path` members (`menu_validation.h:16,19`) carry no
in-class default member initializer. This compiler escalates that to
`error: missing initializer for member ... [-Werror=missing-field-initializers]`
(6 errors total, 2 per site). The sibling `reject()` helper
(`menu_validation.cpp:40`) fully specifies every field and is unaffected;
only these three "accept" returns take the shortcut.

Reproduced twice, identically: (1) a full keep-going tree build reached
[343/1427] with these as the only 6 errors across every other module it had
already compiled (compositor, session_supervisor, notification, settings,
display, applet_host, hybrid_chrome, design_tokens, etc. all clean); (2) an
isolated targeted build of exactly the ten registered global-menu
library/test targets (`qindaqt_global_menu_protocol[...]_tests`,
`..._ownership[...]_tests`, `..._lineage_tests`, `..._exporter[...]_tests`,
`..._qt_widgets_adapter[...]_tests`, `..._applet[...]_access_tests`,
`..._composition_tests`) fails at step 1/56 on this same file before
anything else in the module builds — meaning none of protocol, ownership,
ownership-lineage, exporter, qt-widgets-adapter, applet-access, or
composition C++ targets, nor their CTest registrations, can build or run
right now. Only the three QML offscreen suites (which import the QML source
tree directly per the G0 milestone note and need no C++ link) were actually
exercised by this candidate's own self-reported gates.

`menu_validation.cpp` is untouched by this candidate's diff — this defect
predates `53490b7` and was inherited from earlier in the G0 lineage — but it
blocks this exact candidate's own required strict-warning C++ gate, and no
prior exact reviewer caught it because Aquinas's methodology explicitly
excludes the compiler/CTest lane across all four of that role's rounds ("no
compiler/CTest/QML runtime" is stated in every one of Aquinas's verdicts).
Release build was not separately attempted: identical source, identical
compiler, identical flags guarantee an identical failure, so attempting it
would only reconfirm this at build-farm cost.

**Repair is narrow and mechanical**: give `ValidationResult::reasonCode` and
`::path` in-class default member initializers in `menu_validation.h:16,19`
(e.g. `QString reasonCode{};` / `QString path{};`), or fully specify both
fields at the three call sites `menu_validation.cpp:89,137,160`. Either closes
all 6 errors without changing any runtime behavior (QString already
default-constructs to empty either way).

## P1

None.

## P2

None — both of Aquinas's prior P2s are correctly and completely closed (see
above).

## P3

### P3-1 — new source-shape review threshold crossed, undisclosed in the candidate's own gate report

`python3 tools/check-source-shape` still exits 0 (1051 checked, 0 skipped, as
the candidate's commit message states), but now additionally prints:
`WARNING: tests/shell/global_menu/qml/tst_GlobalMenuAppletOverflow.qml:
decomposition-review: 296 non-blank lines reached review threshold 275`. I
confirmed via `git show 87cef24:...` that this file was 242 non-blank lines
before this candidate (below the threshold) and is 296 after (above it) —
i.e. this warning is new to `53490b7`, not pre-existing. Non-blocking (exit
0), but the commit's self-reported gate line omits it.

### P3-2 — benign textual collision against the current public tip

Local `main` (`c498269`, what Aquinas's prior check used) is stale relative
to `origin/main` (`146fc483`), which has since integrated several other
lanes. `git merge-tree origin/main 53490b7` now produces two real CONFLICT
markers — `docs/wiki/adr/index.md` and `mkdocs.yml` — both simple
same-shape additive-list insertions (this candidate's ADR-0033 table
row/nav entry landing at a different position than other lanes' concurrent
entries in the same shared files); `src/CMakeLists.txt`/
`tests/CMakeLists.txt` merge clean. Not a candidate defect — these are
exactly the "shared registries are coordination points" files AGENTS.md
describes — but the manager will need a trivial manual reconciliation
against the current public tip, not just local `main`, at integration time.

## Exact static evidence

- SHA/tree/parent/detached-clean before and after: **4/4 PASS** (one
  incidental stash cycle, fully reverted and reconfirmed — see above).
- `python3 tools/check-source-shape`: exit 0; 1051 checked, 0 skipped; **one
  new non-blocking WARNING** (P3-1).
- `python3 tools/validate-docs`: exit 0; 65 Markdown documents plus nav.
- `qmlformat -n` on all four touched QML files: **4/4 exit 0, byte-identical
  to already-formatted state**.
- `git diff --check` from public base `d168e95^` and exact parent `87cef24`:
  **exit 0**, no whitespace defects.
- `mkdocs build --strict`: not attempted (not on PATH in this sandbox; not
  claimed).
- Strict-warning Debug compile (`QINDAQT_ENABLE_STRICT_WARNINGS=ON`, isolated
  build tree): **FAIL** — see P0-1. Release not attempted (see above).
- Registered focused C++/CTest gates (protocol, ownership,
  ownership-lineage, exporter, qt-widgets-adapter, applet-access,
  composition): **0/7 could build**, blocked by P0-1.
- QML offscreen suites (behavior/overflow/accessibility): parse-clean via
  `qmlformat -n`; not executed under an actual QML test runner in this
  review (the module's C++ half cannot build, and I did not want to run a
  partial/inconsistent CTest matrix against a broken sibling target without
  flagging that first). Candidate's own self-report claims 30/30 PASS for
  these three.
- `origin/main` collision: **benign textual conflict**, 2 files, additive
  only (P3-2).

## Requested next action

Route to **Aria** for one minimal, non-amended, additive-only descendant of
`53490b7` that adds the missing `ValidationResult` field defaults (or fully
specifies both fields at the three call sites) and reruns a real
strict-warning Debug build plus the ten registered focused gates — not just
the QML-only lane — before handoff. Optionally acknowledge the P3-1 warning
in the gate report. This is not a request for immediate integration: P0
blocks it. I remain available for the exact rereview of that descendant.
