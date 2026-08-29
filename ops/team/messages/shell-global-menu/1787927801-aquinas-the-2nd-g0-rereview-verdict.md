# Aquinas the 2nd — exact Global Menu G0 descendant rereview verdict

- Timestamp: 2026-08-28T14:36:41Z
- Verdict: **FAIL**
- Severity: **P0=0, P1=3, P2=4, P3=3**
- Exact candidate: `79e7333de250cc7e3e4aa15df3c084789539f16f`
- Exact tree: `0161a2ec0ec40ca26a0bd9c6b370aa87268b1b2b`
- Sole parent: `d168e95218d86a96cd803cec35367ccc8d55ac97`
- Rejected ancestor: `782792e613286f9b98852baafa1ae7dd32df7b0d`
- Base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`

PASS requires no blocking P0/P1/P2. This exact descendant still has P1 and P2
findings and must not be accepted or integrated as qualified G0. The dedicated
worktree is clean and detached at the exact SHA/tree. I moved only detached
HEAD as assigned; no product file, index, commit, compiler, test, runtime, GUI,
session, input, or host resource was modified or used.

## P0

None.

## P1

### P1-1 — The new focus-change regression cannot compile

`tests/shell/global_menu/ownership/tst_menu_ownership.cpp:219-229` declares
`int calls = 0` and increments it from `activeWindow() const override`. The
member is not `mutable`, so the new test target is deterministically rejected
by C++ before it can exercise the promised two-read focus race. This is a
source-level finding; I did not invoke a compiler. Make the counter mutable or
move sequencing behind an explicitly mutable fake and retain the two-call
assertion.

### P1-2 — `AuthenticatedProvider` is not authenticator-issued proof

`provider_authenticator.h:17-29` exposes `AuthenticatedProvider` as a public
aggregate with public fields. Any caller can construct one and call
`ActiveProviderSelector::adopt()` (`active_provider_selector.h:37-49`); the
ownership tests do exactly that in their `authenticated()` helper
(`tst_menu_ownership.cpp:55-61`). `adopt()` also accepts default/invalid fields
without validation. The type therefore reduces accidental parameter mixing but
does not prove that `ProviderAuthenticator` checked the adopted facts.

This contradicts the “provably checked”/“API has no way” claims in
`docs/wiki/shell/global-menu.md:76-80`, ADR-0033:50-54, and the public header
itself. Make the capability opaque/non-aggregate with construction restricted
to the authenticator (or give selector an authenticator-owned issuance check),
then test that arbitrary registration/window values cannot reach adoption.

### P1-3 — Changed content can retain the same revision

`MenuTree` promises that revision advances for every accepted content change
(`protocol/menu_tree.h:14-21`), but selector revision changes only when an
external caller re-adopts (`active_provider_selector.cpp:8-18`).
`MenuExporter::refresh()` accepts and stores changed content under any lineage
returned by the injected source without requiring a revision advance
(`menu_exporter.cpp:48-64`). A stale request with the same id and revision can
therefore authorize against semantically changed content.

The composition test manually re-authenticates/re-adopts before its changed
pull (`tst_menu_composition.cpp:149-188`); it proves only the cooperative path,
not the invariant. Reject changed content unless the same-epoch revision
strictly advances (and reject regressed/null lineage), or move revision advance
into the accepted-publication authority. Add a negative changed-content/
unchanged-lineage test and prove the last good tree is retained.

## P2

### P2-1 — A destroyed widget source becomes authoritative empty truth

When `m_menuBar` is null, `QMenuBarMenuSource::snapshot()` returns the default
`MenuSnapshot` with `complete=true` and an empty tree
(`qmenubar_menu_source.cpp:160-167`). The test requires that unsafe result
(`tst_qmenubar_menu_source.cpp:200-208`). An exporter can consequently replace
its last good tree with an authoritative empty menu after source destruction.
This contradicts the incomplete-on-loss guard in
`qmenubar_menu_source.h:26-39`, the wiki at `global-menu.md:121-124`, and Theo's
handoff. Return `complete=false` with a stable source-destroyed defect and test
last-good retention through the exporter.

### P2-2 — Provider “unique name” is not validated as unique

`provider_authenticator.cpp:13-23` accepts any non-empty, byte-bounded string as
`providerUniqueName`. A well-known or malformed name can pass when the
credential seam resolves it, despite every public contract relying on an
immutable D-Bus unique name. A later owner change of a well-known name breaks
the proof's identity meaning. Validate the exact unique-name grammar and
well-formed text/NUL rules, and add hostile well-known/malformed-name cases.

### P2-3 — Overflow is not responsive and vertical overflow is clipped away

`GlobalMenuApplet.qml:25-35` collapses by a caller-controlled item count only
and computes its own implicit width from all retained entries; it never reacts
to assigned narrow width. The existing test (`tst_GlobalMenuApplet.qml:223-241`)
does not constrain width, so it does not prove the original narrow-panel
requirement. Negative `maximumVisibleEntries` also feeds directly to
`slice(0, -1)` and defeats the intended limit.

For vertical panels, root implicit height excludes the overflow indicator
(`GlobalMenuApplet.qml:33-37`), while the indicator is anchored below the
column (`:121-135`) and the root clips. The “+N” marker is therefore outside
the clipped implicit geometry. Clamp the count, make overflow respond to
actual main-axis geometry, include the indicator in vertical size, and test
genuinely narrow horizontal and vertical hosts.

### P2-4 — Accessible/checkable presentation remains incomplete and unproved

The facade projects `checkable` and `checked`
(`globalmenuappletaccess.cpp:29-40`), but `MenuEntry` never binds those values
to its `AbstractButton` or accessible state (`GlobalMenuApplet.qml:43-81`). The
QML fixtures all set them false. `Accessible.onPressAction` now exists, but no
test invokes it, and the overflow indicator has no accessible description.
Add checked/unchecked accessible-state cases and a real accessible-press
activation/rejection case; make collapsed-entry count discoverable.

## P3 and bounded caveats

1. `GlobalMenuAppletAccess::activationRequested` carries only an action id
   (`globalmenuappletaccess.h:39-50`) and the facade has no explicit thread/
   queued-connection or lineage-handoff contract. G1 must preserve the exact
   window/epoch/revision observed at the UI request and run `InvocationGuard`
   before execution; looking up a later “current” tree by id would recreate a
   request/content race.
2. All adapter traversal failures collapse to
   `menu-traversal-exceeded-bounds` (`qmenubar_menu_source.cpp:177-183`), even
   cycles and destruction classes that the public `defectCode` contract treats
   as stable diagnostics. Preserve distinct codes so support/tests can
   distinguish hostile size from object-lifetime defects.
3. ADR-0033 allocation and links are correct: the only 0026 references are the
   intentional provenance note, 0028 is absent, and docs validation passes.
   A read-only merge-tree against current `origin/main` `50742fe` reports
   textual conflicts only in `docs/wiki/adr/index.md` and `mkdocs.yml`; manager
   integration must retain 0026, 0027, and 0033 rows/nav entries. Other shared
   registry/index additions merge additively. QML remains deliberately
   source-only/uninstalled, accurately documented as a G1 boundary.

## Prior-verdict closure ledger

- Original P1-1 lineage: **partially closed**. Initial public composition and
  request revision checks exist; publication does not enforce revision/content
  binding (new P1-3).
- Original P1-2 authentication: **partially closed**. Focus double-read and
  generation invalidation are implemented; the adoptable value remains
  forgeable (new P1-2).
- Original P1-3 hostile parsing: **mostly closed**. Unknown kind, isolated
  surrogate, depth/sibling/total overflow, and cycle paths are present;
  destroyed source loss is still reported complete (new P2-1).
- Original P2-1 delta: **closed** by deleting the unusable contract and keeping
  snapshot-only truth.
- Original P2-2 presentation: **partially closed**. Hidden/submenu honesty,
  keyboard action activation, and a real Column landed; constrained/vertical
  overflow and checked accessibility remain open (new P2-3/P2-4).
- Original P2-3 tests: **not closed** because one new gate cannot compile and
  key negative/accessibility/geometry cases are absent.
- Original P2-4 contracts: **mostly closed** for references/threading, but the
  adapter implementation/test contradict its loss contract and the facade's
  activation lineage/thread boundary remains unspecified.
- Original P3-1 Unicode: **closed** with isolated-surrogate rejection/tests.
- Original P3-2 packaging: **honestly bounded**, still G1.
- Original P3-3 numbering/collision: **ADR rename closed**; two ordinary
  current-main documentation conflicts remain for manager resolution.

## Static evidence

- Exact SHA/tree/sole-parent and detached-clean state: PASS.
- `git diff --check 782792e..79e7333`: PASS, exit 0.
- `python3 tools/check-source-shape`: PASS, 1048 files, 0 allowlisted.
- `python3 tools/validate-docs`: PASS, 65 Markdown documents plus nav.
- `qmlformat` parse of production component and QML test: PASS, exit 0 each.
- ADR-0033/stale-reference audit: PASS as described above.
- Read-only current-origin merge-tree: two bounded documentation conflicts.
- `mkdocs build --strict`: unavailable on PATH; not claimed.
- Compiler/CTest/QML runtime/GUI/session/input/host: intentionally not run by
  assignment. P1-1 is a direct C++ const-correctness finding, not a claimed
  compiler result.
- Board `http://127.0.0.1:4180/`: healthy during review.

## Requested next action

Theo should create one non-amended descendant fixing the three P1s and four
P2s, with targeted negative regressions named above. Route that exact SHA back
to Aquinas for rereview. Only after source review passes should the manager
allocate compiler/CTest evidence and consider integration; ADR-0033 remains
Proposed until then.
