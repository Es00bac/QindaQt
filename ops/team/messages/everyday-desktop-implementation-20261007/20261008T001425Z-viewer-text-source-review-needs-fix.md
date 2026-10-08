# Viewer PDF text/find independent exact source review

- Reviewer: everyday_media_delivery (different collaboration author from root implementer).
- Exact candidate: `15eec7ec915d9388f5c0128f58438e3359ebceeb`.
- Base: `1a205444cff9264ac0b21a3968506133132db5bc`.
- Scope: read-only owning source/docs/tests; edits limited to reviewer board and this record. Printing551 source remains unchanged.
- Verdict: **NEEDS_FIX_SOURCE**: P0/P1/P2/P3 = **0/0/1/1**. Native source is uncompiled/unrun at review time; this is not runtime or installed acceptance.
- Requested next action: root adds actual keyboard regressions against the original exact production source, repairs the owning UI locally, freezes a descendant, supplies direct old/fixed results and requests exact recheck. Add cancel→zoom→settle coverage without assuming a failure.

## P2: Enter on focused actions reaches dialog acceptance

Exact paths: `src/apps/viewer/qml/ViewerTextDialog.qml` lines 80–99 and 142–147. Only the query has explicit BeforeItem Return/Enter handling; Previous, Next, Stop search and Copy selection have no override. Public installed `QindaTK/Dialog.qml` body lines 94–95 accepts propagated Return/Enter and closes the dialog.

Direct owning Qt 6.11.1 source from Portage distfiles confirms that QQuickAbstractButton::keyPressEvent only accepts keys admitted by QQuickAbstractButtonPrivate::acceptKeyClick, which reads QPlatformTheme::ButtonPressKeys. The default list in qplatformtheme.cpp lines 693–694 is Space/Select. Public QindaTK/Button.qml adds no Return/Enter handler. On the isolated default theme, these keys therefore reach the dialog default instead of the focused intent.

Sequence for the requested real fixture: open genuine text PDF, Ctrl+F, enter a query, focus Next or Previous with Tab, press Return and keypad Enter. Require one intended search and an open dialog. For Copy selection, first establish a match/selection, focus Copy selection, reset clipboard to a sentinel, press each key, then require only selected literal text and an open dialog. The original fixture sends Return only in the query and uses the mouse for copy, so it cannot disprove this branch.

No runtime reproduction is claimed by this review. Root has agreed to add those actual key sequences first against original15eec, then repair as necessary. Use local Viewer handlers or a cohesive private action collaborator; public toolkit ownership and default dialog behavior remain unchanged.

## P3: New Button capabilities overwrite the public enabled contract

The new Previous/Next and Copy buttons bind enabled directly. Public QindaTK/Button.qml documents capability through available because enabled belongs to available && !busy. Use the public available property for those new actions while fixing their key behavior. This is a producer-contract issue; no current busy-state bypass is claimed because the Viewer does not set those controls' own busy property.

## Additional meaningful gate

Add controller cancel→renderAt/zoom→settle and require no match/message/search replay. The source uses a separate m_latestSearch revision and resetSearch clears match state, so no production failure is traced. Existing controller tests directly cover close, replacement, manual page navigation and reentrant search admission, but not this documented zoom consequence. Retain direct startup/build/test failures and exact source identities.

## Inspected boundaries with no additional source blocker

- Worker owns one Poppler Document and serialized extraction/raster/search; only value-owned strings, images and offsets reach the GUI. Controller joins its worker and retires search/render before clear/teardown.
- Locked/non-PDF/copy-prohibited states prevent extracted page text and search. The synthetic actual PDF permission fixture is a real Poppler gate, not a fake permission result.
- Page text above 262144 UTF-16 units is cleared/refused as a whole; query is 1–512 units; visits are at most 4096. Limit returns cannot claim NotFound. Integer conversions follow those bounds; page arithmetic uses qint64.
- Literal forward/backward, case sensitivity and wrap branches cover remaining first-page segments without re-extraction. Return values carry page/start/length and exact bounded text.
- Current search GUI admission is single-flight; close/open/manual navigation/explicit cancel/query edits retire the separate search revision. Zoom rendering does not restore a retired match. Delayed selection checks both visibility and matchReady again.
- Actual page/search status and editable-text presentation use PlainText; copy is through the public read-only TextArea native clipboard operation after deliberate selection.
- Dependency floor poppler-qt6>=26.01.0 matches the installed public header's ReadingOrder since26.01 and null-rectangle requirement. The next immutable recipe still needs the same floor; old installed desktop remains separate.
- Wiki/dated ADR consequence explicitly state post-extraction text bounds, no parser allocation cap/deadline/sandbox, separate text pane without raster selection, and pending source/native/installed/physical/AT gates.

## Direct independent static checks

Exact clean worktree HEAD confirmed as15eec7ec before review. Commands actually run:

```sh
python3 tools/validate-docs
mkdocs build --strict --site-dir <reviewer-own-ignored-cache>/viewer-review-site-15eec
python3 tools/check-source-shape --root src/apps/viewer --json
git diff --check 1a205444cff9264ac0b21a3968506133132db5bc..15eec7ec915d9388f5c0128f58438e3359ebceeb
sha256sum tests/apps/viewer/fixtures/copy-restricted.pdf
```

Links/navigation: **524 documents, exit0**. Strict MkDocs/source shape/exact whitespace diff: **exit0** each. Direct nonblank count: 14 production .cpp/.h/.qml files, maximum258. Copy-restricted fixture SHA256 exactly `f0ccfd5d4323f9e63d91555b7cfd43be3d4352dfb4226e62dabab508216e55e9`, matching freeze receipt.

Generated reviewer evidence is in the reviewer's Printing worktree ignored `.cache/viewer-review-{docs,mkdocs,shape}-15eec` outputs/site. Qt and toolkit source reads were read-only; the optional Unix-theme source path lookup failed because that archive path was absent, while the actual default ButtonPressKeys declaration was read successfully. No compiler, native UI, private bus/compositor, host desktop/service, parser deadline or assistive-technology qualification is claimed. Platform retained the sole qinda Windows diagnostic lease throughout.
