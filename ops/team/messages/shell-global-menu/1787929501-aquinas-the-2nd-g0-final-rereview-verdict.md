# Aquinas the 2nd — exact Global Menu G0 repair rereview verdict

- **Timestamp:** 2026-08-28T15:05:01Z
- **Verdict:** **FAIL**
- **Severity:** **P0=0, P1=1, P2=3, P3=2**
- **Exact candidate:** `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
- **Tree:** `43ca66cccea668cd0055072f2717457b394e43b6`
- **Sole parent:** exact failed `79e7333de250cc7e3e4aa15df3c084789539f16f`
- **Public base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85`

PASS requires no P0/P1/P2. This exact repair must not be accepted or integrated
as qualified G0. The owned worktree remains clean on `worker/global-menu-g0` at
the exact SHA/tree/parent. I made no product/index/Git mutation and used no
compiler, CTest, QML runner, GUI/session/input/config, or host resource.

## Closed former findings

- The focus-race fake is const-correct (`tst_menu_ownership.cpp:294-324`).
- `AuthenticatedProvider` is an opaque non-aggregate with a private
  authenticator-only constructor; tests obtain proofs through the real
  authenticator (`provider_authenticator.h:19-64`;
  `tst_menu_ownership.cpp:57-73,130-140`).
- Same-epoch changed content cannot reuse a revision; regressed revision and
  null epoch also retain last-good truth (`menu_exporter.cpp:56-92`;
  `tst_menu_exporter.cpp:228-284`; `tst_menu_composition.cpp:192-222`).
- Destroyed widget sources are incomplete `source-destroyed`; depth, sibling,
  total, cycle, and lifetime defects have distinct stable codes and focused
  tests (`qmenubar_menu_source.cpp:25-32,87-165,175-207`;
  `tst_qmenubar_menu_source.cpp:226-351`).
- Basic checked/accessibility bindings, both layout orientations, a clamped
  count limit, and a visible/accessible overflow indicator exist. ADR-0033
  numbering/links are internally consistent.

## P0

None.

## P1

### P1-1 — registered keyboard-focus test contradicts the delegate

`GlobalMenuApplet.qml:90-102` defines `itemEnabled` as `isAction &&
modelData.enabled` and assigns it to `enabled`, so every top-level submenu is
disabled. `tst_GlobalMenuApplet.qml:178-195` then iterates through those same
submenu delegates, calls `forceActiveFocus`, and requires `activeFocus` true.
Qt specifies that disabling an Item automatically clears active focus and
prevents keyboard input. The newly registered behavior test cannot establish
its required state.

Reference: [Qt 6 Item `enabled`](https://doc.qt.io/qt-6/qml-qtquick-item.html#enabled-prop).
Choose one honest contract: either keep unavailable submenus disabled and test
that keyboard focus skips/rejects them, or keep them focusable while gating
invocation separately on action-kind plus provider-enabled state. Retain the
non-activation assertion.

## P2

### P2-1 — unique-name grammar still rejects valid peers and admits an invalid boundary

`provider_authenticator.cpp:43-46` omits `-`, although it is valid in D-Bus bus
names; a daemon-issued name such as `:1.worker-2` is rejected. Conversely,
`menu_limits.h:22` sets 256 bytes and `provider_authenticator.cpp:26` rejects
only greater values, although D-Bus's maximum bus-name length is 255. The
ownership tests at `tst_menu_ownership.cpp:242-292` do not cover a valid
hyphenated name or 255/256 boundaries, and the wiki repeats the wrong
`[A-Za-z0-9_]` grammar at `global-menu.md:82-86`.

Reference: the
[D-Bus Specification](https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names-bus)
permits `[A-Z][a-z][0-9]_-`, requires at least one period, and sets the maximum
name length to 255. Admit hyphen, set the exact bound, add positive/boundary
regressions, and correct the wiki.

### P2-2 — responsive overflow remains a non-bounding heuristic

`GlobalMenuApplet.qml:31-55` estimates rendered width from JavaScript string
length times seven plus fixed padding. That is not an upper bound on the actual
12-pixel `Text` width: wide glyphs and font metrics can make the instantiated
row wider than the estimate, so the algorithm can retain too many delegates
and place the indicator outside the assigned width before `clip` hides it.
`verticalLimitFor` similarly forces at least one entry even when the assigned
height cannot hold one entry plus its overflow indicator (`:57-65`). The tests
use only narrow `"Item N"` labels at width 200 and height 48
(`tst_GlobalMenuAppletOverflow.qml:124-160`), so they cannot prove the wiki's
claim that constrained geometry can never clip the affordance
(`global-menu.md:161-167`). Use actual/bounding font/delegate metrics and add
wide-glyph plus below-minimum main-axis cases, or narrow the documented host
minimum and enforce it at the boundary.

### P2-3 — accessibility and interactive checked tests bypass the named paths

The source hook exists, but `test_accessiblePressActivationPath`
(`tst_GlobalMenuApplet.qml:217-240`) calls `entry.pressAction()` directly and
never emits `entry.Accessible.pressAction()`. It therefore cannot detect a
missing/broken `Accessible.onPressAction` connection. The checked-state test
(`:242-276`) reads initial bindings and again calls the helper; it never clicks
or sends Space to a checkable delegate, although Qt AbstractButton toggles
`checked` on those interactions. It cannot prove the promised state behavior
or detect a stale/local inversion after activation.

Reference: [Qt 6 AbstractButton `checkable`](https://doc.qt.io/qt-6/qml-qtquick-controls-abstractbutton.html#checkable-prop).
Drive the attached accessible signal itself, and interactively activate both
initially checked and unchecked fixtures while asserting the intended
provider-owned state/binding and activation count.

## P3

1. The normative module row says `src/shell/global_menu` may use Qt Core (with
   Qt Gui/Widgets only in its adapter) and “never ... QML policy”
   (`module-boundaries.md:41-43`), while the same path owns a Qt Quick Controls
   component with overflow, orientation, focus, and activation presentation
   policy. Clarify the boundary as separate protocol/policy/adapter/applet
   presentation targets rather than leaving source and architecture in direct
   disagreement.
2. The verification page lists only
   `qindaqt.global-menu-applet-qml-offscreen` and says it covers vertical and
   overflow (`global-menu.md:181-191`), but those cases moved to the newly
   registered `qindaqt.global-menu-applet-qml-overflow-offscreen`
   (`tests/shell/global_menu/qml/CMakeLists.txt:18-27`). Name both gates.

## Static and integration evidence

- Exact SHA/tree/sole-parent and clean candidate state: PASS.
- `python3 tools/check-source-shape`: exit 0; 1049 checked, 0 skipped; one
  advisory at 276/275 non-blank lines in the behavior QML test.
- `python3 tools/validate-docs`: exit 0; 65 Markdown documents plus nav.
- `qmlformat` parses production QML and both QML test files: exit 0 each.
- `git diff --check 9db68c4..bdb2734`: exit 0.
- ADR stale-number search: only intentional ADR-0026 provenance; no stale path
  or ADR-0028 reference.
- Board `http://127.0.0.1:4180/`: HTTP 200.
- Current committed `main` is `c4982697`; six additive shared paths diverge
  from the old base (`docs/wiki/adr/index.md`, module boundaries, wiki index,
  `mkdocs.yml`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`). Read-only
  merge-tree shows no conflict markers, but the manager checkout also has
  uncommitted shared documentation/board work: integration must preserve it.
- `mkdocs build --strict`: unavailable on PATH; not claimed.
- Compiler/CTest/QML runtime/GUI/session/input/config: prohibited by this review
  assignment and not used. Static parse success is not runtime acceptance.

## Requested next action

Theo should create one non-amended descendant fixing P1-1 and the three P2
findings, with the exact positive/negative regressions above and the two narrow
documentation corrections. Route that exact SHA back to Aquinas for rereview.
Only a different worker should then allocate compiler/CTest/QML runtime
evidence; ADR-0033 remains Proposed until accepted integration.
