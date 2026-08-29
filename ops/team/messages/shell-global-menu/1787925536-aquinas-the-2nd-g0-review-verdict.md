# Aquinas the 2nd — exact Global Menu G0 review verdict

- Timestamp: 2026-08-28T13:58:56Z
- State: handoff; independent review terminal
- Verdict: **FAIL**
- Severity count: **P0=0, P1=3, P2=4, P3=3**
- Exact candidate: `782792e613286f9b98852baafa1ae7dd32df7b0d`
- Exact tree: `263d86061585b2b097d9d453d34c2b7ad889f3d9`
- Exact sole parent/base: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Worktree remained clean and detached; no product edit, commit switch, compile,
  runtime, UI, session, input, or compiler/runtime-lane use occurred.

PASS requires no blocking P0/P1/P2. This candidate has blocking P1 and P2
findings and must not be integrated as accepted G0.

## P0 findings

None.

## P1 findings

### P1-1 — No coherent authoritative lineage can reach invocation

`src/shell/global_menu/ownership/src/active_provider_selector.cpp:11-18`
creates an ownership epoch with `QUuid::createUuid()`. Independently,
`src/shell/global_menu/exporter/src/menu_exporter.cpp:26-40` creates a second
export epoch with another `QUuid::createUuid()`. There is no API that transfers
or binds either lineage to the other. Nevertheless,
`src/shell/global_menu/ownership/src/invocation_guard.cpp:20-27` requires the
selector, request, and tree epochs to match. Ordinary use of the public
surfaces therefore cannot create an invocable accepted export.

The stale-content guard is also incomplete:
`ownership/include/.../invocation_guard.h:15-19` gives `InvocationRequest` no
revision, and `ownership/src/invocation_guard.cpp:20-27` never compares
`SelectedProvider::revision` with `MenuTree::revision`. An older tree from the
same window and epoch remains authorizable after a later adoption. The isolated
tests conceal the mismatch: exporter tests mint and check only exporter lineage
(`tests/.../exporter/tst_menu_exporter.cpp:42-108`), while the stale-tree test
uses a different epoch rather than an older same-epoch revision
(`tests/.../ownership/tst_menu_ownership.cpp:281-297`). This contradicts the
lineage/stale-tree promises in `docs/wiki/shell/global-menu.md:57-73` and
ADR-0026:29-43.

### P1-2 — Authentication is not bound to adoption and is focus-TOCTOU unsafe

`src/shell/global_menu/ownership/src/provider_authenticator.cpp:42-60` samples
the active window once, then performs a separate credential lookup, and never
re-checks focus or an authenticated generation before returning success. Focus
can change during that lookup and the stale registration still returns
accepted. More fundamentally, `provider_authenticator.h:14-19` returns only a
boolean/reason, discarding the verified identity, while
`active_provider_selector.h:26-39` exposes `adoptAuthenticated(registration,
window)` with a separately supplied window and explicitly trusts its caller.
Nothing in the type/API binds the facts that were verified to the facts that
are adopted; the registration/window pair can differ after authentication.
There is no focus generation or automatic invalidation/clear seam. This does
not uphold the candidate's claim that only the genuinely active authenticated
window may become authoritative.

### P1-3 — Hostile input can be accepted or silently converted to partial truth

`src/shell/global_menu/protocol/src/menu_validation.cpp:60-113` handles only the
three known `MenuItemKind` values through conditional branches; an out-of-range
enum value is accepted and its children are not traversed. The claimed hostile
canonical boundary therefore does not reject an unknown wire kind.

Separately, the Qt adapter silently truncates an over-depth submenu and any
sibling list beyond the configured maximum
(`qt_widgets_adapter/src/qmenubar_menu_source.cpp:108-116,141-147`). The
resulting partial tree can pass validation, even though the wiki says a tree
exceeding any bound is invalid as a whole and pathologically large menus are
rejected wholesale (`docs/wiki/shell/global-menu.md:27-38`; ADR-0026:59-61).
An adapter must detect overflow/cycles/mutation and return an invalid outcome,
not publish the first bounded prefix as if it were the complete application
menu.

## P2 findings

### P2-1 — The documented delta cannot be applied safely

`protocol/include/.../menu_delta.h:19-33` says consumers can apply Removed →
Inserted → Updated without dangling parents, but an operation contains only
`op`, `id`, and `parentId`: inserts/updates carry neither item content nor a
sibling index, so they cannot reconstruct the next tree. Removal is generated
in previous pre-order (`protocol/src/menu_delta.cpp:38-55,83-89`), which removes
a parent before its descendants and creates precisely the intermediate
dangling relationship the contract denies. The tests only check category
grouping/presence (`tests/.../protocol/tst_menu_protocol.cpp:249-305`); they do
not apply a delta, test parent/child removal, moves, or prove reconstruction of
the next tree.

### P2-2 — The applet/QML surface misrepresents real menu behavior and lacks required interaction geometry

The adapter maps real top-level `QMenu` entries to `Submenu`
(`qt_widgets_adapter/src/qmenubar_menu_source.cpp:108-119`). The facade projects
those entries as enabled (`applet/src/globalmenuappletaccess.cpp:18-35`), and
QML makes them clickable (`applet/qml/GlobalMenuApplet.qml:42-59`), but
`GlobalMenuAppletAccess::activate` rejects every submenu
(`applet/src/globalmenuappletaccess.cpp:55-68`). Thus the real File/Edit-style
top-level entries look actionable and do nothing. The QML click test avoids
that path by inventing a top-level `Action`
(`tests/.../qml/tst_GlobalMenuApplet.qml:75-87`). Invisible model items are
also retained and rendered as disabled labels rather than omitted.

The exposed `vertical` property changes only implicit width; the content
remains a horizontal `Row` (`GlobalMenuApplet.qml:15,21,36-70`). The component
has no keyboard focus/Keys handler or accessible press action, and no clipping,
wrapping, scrolling, elision, or overflow affordance for constrained panels.
The QML suite covers mouse click only and has no keyboard, accessibility-action,
invisible-item, vertical, narrow-width, or overflow case.

### P2-3 — Tests do not exercise the security and boundary seams they claim

The focused tests omit same-epoch stale revisions, selector/exporter lineage
composition, focus changes during authentication, authentication-result-to-
adoption binding, invalid enum values, adapter overflow/cycles/mutation,
delta application, hidden-item projection, and the realistic top-level submenu
path. The isolated happy-path tests therefore do not establish the end-to-end
ownership, fail-closed, invocation, or applet behavior claimed in the wiki and
commit message.

### P2-4 — Public ownership/lifetime/threading contracts are incomplete

`exporter/menu_exporter.h:40-49` stores a reference to `MenuSource` without
stating that the source must outlive the exporter or which thread may call it.
`provider_authenticator.h:29-41` likewise stores two public seam references
without lifetime/thread contracts. `qmenubar_menu_source.h:25-35` stores GUI
objects and exposes synchronous traversal without requiring the Qt GUI thread
or defining mutation behavior. These are non-obvious public contracts required
by `AGENTS.md`; omitting them makes future transport integration prone to
dangling references or cross-thread Qt object access.

## P3 findings and bounded caveats

1. `menu_validation.cpp:15-27` claims `QString` guarantees well-formed UTF-16,
   but `QString` can contain isolated surrogate code units. The validator checks
   only UTF-8 replacement output and NULs, so its well-formed-text claim and
   tests are incomplete.
2. The C++ targets/headers are installed, but
   `src/shell/global_menu/applet/qml/GlobalMenuApplet.qml` is neither part of a
   QML module nor installed; the test imports the source tree directly. This is
   consistent with the explicit G0 non-live boundary, but G1 cannot claim an
   installed applet until it packages the presentation and proves a staged
   consumer/session path.
3. Against observed current `origin/main`
   `cbec6fb42216e5bcc3283004473be7f5f6ccda66`, the exact base is an ancestor.
   Six shared additive registries overlap. A read-only three-way `git
   merge-tree` reports textual conflicts only in `docs/wiki/adr/index.md` and
   `mkdocs.yml`; resolve by retaining both ADR-0026/ADR-0027 and both nav rows in
   numeric/section order. The architecture/index/CMake/test registry additions
   merge additively. All new `src/shell/global_menu`, tests, wiki page, and
   ADR-0026 paths are otherwise unique. The dirty shared checkout was not
   touched.

## Static evidence

- Exact SHA/tree/base verification: PASS; clean detached product worktree.
- `git diff --check 9db68c4..782792e`: PASS, exit 0.
- `tools/check-source-shape`: PASS, exit 0; 1048 checked, 0 allowlisted.
- `tools/validate-docs`: PASS, exit 0; 65 Markdown documents plus nav.
- `qmlformat <file> >/dev/null` for component and QML test: PASS, exit 0 each.
- `mkdocs`: unavailable on PATH; no strict MkDocs gate claimed.
- Board `http://127.0.0.1:4180/`: healthy, curl exit 0 at handoff.
- Compile/CTest/runtime/UI/session/input: intentionally not run by this
  source-only reviewer, per assignment.

## Requested next action

Route this exact reproduction to Theo Lin in the preserved implementation
worktree. A repaired descendant should establish one authoritative,
revision-bearing lineage; return a proof-bound authentication/adoption value
with focus-generation invalidation; reject unknown/over-limit adapter input
atomically; either make deltas fully applicable with safe ordering or document
and test snapshot-only semantics; make hidden/submenu/keyboard/vertical/
overflow behavior honest; document all public lifetime/thread contracts; and
add non-vacuous seam tests. A different provider must then review the exact
repaired commit before any integration or ADR acceptance.
