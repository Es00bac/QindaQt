# Aquinas the 2nd — exact Global Menu G0 final-repair rereview verdict

- **Timestamp:** 2026-08-28T15:45:09Z
- **Verdict:** **FAIL**
- **Severity:** **P0=0, P1=0, P2=2, P3=0**
- **Exact candidate:** `87cef246a690f5bdc2c860238a1feb37e10957de`
- **Tree:** `48756678c022970f4e2604fa8dfb1efe0baa7b66`
- **Sole parent:** exact reviewed FAIL
  `bdb27348cb2d899cec1f04d5a3fe2ffeed827630`
- **Public base:** `9db68c4023257b49421101fa1b13c73bbc2cfa85`

PASS requires no P0/P1/P2. This exact repair is not ready for combined-tree
compile/integration. The dedicated review worktree remains clean, detached, and
exact. I made no candidate/product/Git mutation and used no compiler, CTest,
QML runtime, GUI/session/bus/input/config, or host interaction.

## Former findings closed

- D-Bus unique-name validation now admits hyphen, uses the exact 255-byte
  maximum, accepts `:1.worker-2` and 255 bytes, and rejects 256 bytes
  (`menu_limits.h:24-27`; `provider_authenticator.cpp:20-54`;
  `tst_menu_ownership.cpp:247-297`; `global-menu.md:82-87`).
- Disabled submenu delegates now correctly refuse active focus while enabled
  actions focus and activate (`tst_GlobalMenuApplet.qml:178-205`).
- The dedicated accessibility suite emits the real attached
  `Accessible.pressAction()` signal, rejects submenu/disabled activation, and
  uses Space on checked and unchecked provider-owned fixtures without local
  inversion (`tst_GlobalMenuAppletAccessibility.qml:80-153`).
- The module boundary explicitly separates protocol/policy, adapter, and applet
  presentation targets; the wiki lists all ten registered focused gates
  (`module-boundaries.md:42`; `global-menu.md:188-205`).
- Previously closed proof provenance, focus double-read, revision/content,
  destroyed-source, bounded hostile parsing, exporter/fallback, adapter
  lifetime, and action lineage sources are blob-identical to their reviewed
  `bdb27348` forms. This descendant does not regress those repairs.

## P0

None.

## P1

None.

## P2

### P2-1 — measured-fit accounting still paints outside its own limit

`horizontalLimitFor` subtracts only `indicatorWidth` before fitting entries
(`GlobalMenuApplet.qml:68-84`), but the painted indicator begins after
`row.right` with a 12 px `root.spacing` left margin (`:225-243`). The test's own
invariant correctly counts that margin (`tst_GlobalMenuAppletOverflow.qml:85-96`).
At the calculated equality boundary of one measured entry plus the measured
indicator, the loop admits the entry and then paints row + 12 + indicator into
the clipped root, exceeding the budget it used.

Vertically, `verticalLimitFor` reserves `measuredIndicatorHeight() + 4`
(`GlobalMenuApplet.qml:87-103`), but `indicatorFits` requires only the measured
height (`:110-113`). At equality it admits the indicator at
`verticalLayout.bottom + 4` (`:241-243`), so its bottom crosses the clipped
host boundary. The registered suite samples widths 200/360/8 and heights 48/12
(`tst_GlobalMenuAppletOverflow.qml:155-238`), not either calculated equality
boundary, despite its fit helper counting both omitted margins.

Repair the horizontal budget to reserve spacing + indicator width whenever an
indicator will be shown, and make vertical fit require the full indicator block
including its top margin. Add calculated-boundary regressions which directly
assert indicator right/bottom geometry. While repairing, measure the actual
localized `qsTr("+%1")` indicator text and replace or isolate the shared mutable
`TextMetrics.text` probe (`GlobalMenuApplet.qml:38-62`); the current fit
calculation measures the untranslated `"+" + total-count` string while painting
localized overflow-count text, so it is not the wiki's strict upper bound for
all presentation states (`global-menu.md:164-172`).

### P2-2 — accessible focusability contradicts actual focusability

Every `MenuEntry` declares `Accessible.focusable: true` even when the same
delegate is `enabled: false` because it is a submenu or disabled action
(`GlobalMenuApplet.qml:137-153`). The repaired keyboard test proves those
objects refuse active focus (`tst_GlobalMenuApplet.qml:178-205`). Assistive
technology is therefore told an item is focusable when the actual Qt item state
cannot focus it.

Bind the accessible focusable state to the effective focusability truth
(`entry.enabled`, or an explicitly documented equivalent). Extend the
registered accessibility test to assert an enabled action is focusable while a
submenu and disabled action are not. Preserve the already-correct real press
signal and provider-owned checked-state regressions.

## P3

None in the exact candidate. Theo's handoff prose says nine gates, while ten
focused gates are registered and correctly documented; this is coordination
prose only, not a product-candidate defect.

## Exact static evidence

- SHA/tree/sole-parent/detached-clean: **4/4 PASS**.
- `python3 tools/check-source-shape`: exit 0; **1051 checked, 0 skipped**.
- `python3 tools/validate-docs`: exit 0; **65 Markdown documents plus nav**.
- `qmlformat -n` parse on production, behavior, overflow, and accessibility
  QML: **4/4 exit 0**.
- `git diff --check` from public base and exact parent: **2/2 exit 0**.
- Exact focused registration surface: **10** gates — protocol, ownership,
  ownership-lineage, exporter, Qt Widgets adapter, applet access, composition,
  behavior QML, overflow QML, accessibility QML.
- Current committed `main` remains `c4982697`; six shared additive registry/doc
  paths overlap. Read-only `merge-tree` has no conflict markers, but the manager
  checkout has existing dirty documentation/board artifacts, so later
  integration must preserve them.
- Team board `http://127.0.0.1:4180/`: HTTP **200** at handoff.
- `mkdocs build --strict`: unavailable on PATH; not claimed.
- Compiler/CTest/QML runtime: prohibited for this exact source review and not
  used. Static parse is not runtime acceptance.

## Requested next action

Theo should produce one non-amended descendant fixing P2-1 and P2-2 with the
calculated geometry and accessible-state regressions above, then route its exact
SHA/tree/parent back to Aquinas. Aquinas remains available for the exact
rereview. Only after a PASS should the manager allocate combined-tree compile,
focused CTest/QML runtime, and integration.
