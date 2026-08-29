---
author: Liskov Rowan
date: 2026-08-28T16:35:00-06:00
topic: shell-clipboard-applet
type: handoff
parent_commit: 5e48b5cf4603cb3622237fb4d7d1ec197dcdd988
candidate_commit: 69b3edc066739856424cdc7b99164693152697ff
candidate_tree: 041666d53949eabc1d5eff5fe7c273800772ef55
branch: worker/clipboard-applet-c1-orion
worktree: /mnt/d/QindaQt/worktrees/clipboard-applet-c1-orion
build_root: /mnt/d/QindaQt/builds/clipboard-applet-c1-liskov
review_request: Hopper the 3rd exact rereview
status: handoff
---

# Clipboard applet C1 exact-review repair handoff

One non-amended repair descendant of the rejected parent, Orion Vale's
history preserved. Candidate commit
`69b3edc066739856424cdc7b99164693152697ff`, tree
`041666d53949eabc1d5eff5fe7c273800772ef55`, sole parent
`5e48b5cf4603cb3622237fb4d7d1ec197dcdd988`, working tree clean.

## Repairs (one per compiled blocker from midpoint 1787952516)

1. **Lock purge + generation fence.** `setLocked(true)` now denies model
   privacy before the lock signal is observable: content purged, generation
   +1, unlock restores only the authority the lock itself removed (an
   independent host denial survives unlock). The controller destroys its
   presentation copy, pending intents, feedback, and full search state on
   the same signal. Pre-lock ids never resolve again; unlock cannot
   redisclose.
2. **Pinned-first partition.** Projection stable-partitions pinned then
   unpinned, each class in the snapshot's most-recent-first order; the same
   partition governs search-result rows; cap applies after the partition.
3. **Query-generation fencing.** Controller-internal monotonically
   increasing query generation maps request ids; replies for superseded,
   abandoned, or replayed requests are dropped regardless of numeric ids
   (seam promises uniqueness only). A dispatch window attributes the first
   synchronous reply inside `requestSearch()` to the issuing generation.
4. **Boundary poison.** Patterns now refuse QtGui/QClipboard,
   QGuiApplication::clipboard, GTK/X11/external-helper access; four poison
   cases each staged alone must each be independently rejected.
5. **Install component.** New relocatable `ClipboardApplet` component
   packages the shared backing library, QML plugin, qmldir, qmltypes, QML
   files, public headers (applet + consumed `clipboard_types.h`), and the
   manifest. `qindaqt.clipboard-applet-installed-consumer` runs
   `cmake --install --component ClipboardApplet` into a fresh stage,
   asserts every artifact, links/runs a C++ consumer against only staged
   files, and imports the staged QML module offscreen.
6. **Explicit QML prerequisites + truthful counts.** All three QML rows
   execute through `run_clipboard_applet_qml_test.cmake`, which first
   builds `qindaqt_clipboard_applet_qml_test_prerequisites` (applet +
   Controls + Tokens plugins and the two manifest binaries). Verified on a
   fresh build containing only the declared C++ test targets: the QML rows
   built their prerequisites and passed. Registered truth is **10
   applet-related rows** (280–287 plus shared 120/121), not the previously
   claimed 11 suites.

## Verification (exit 0 unless stated)

- Debug and Release (`-DQINDAQT_BUILD_KWIN_PLUGIN=ON`):
  `ctest -R "clipboard-applet|applet-manifest|applet-catalog"` — **10/10
  passed each profile**; `ctest -R clipboard` including the four C0 model
  rows — **12/12 each profile**.
- Mutation sensitivity: reverting each product repair independently fails
  its regression — lock purge (seam+controller rows 0/2), partition
  (model row failed), id-ordering (controller row failed), poison patterns
  (boundary row failed). Install component is validated by construction:
  the probe asserts staged artifacts produced only by the real install
  rules.
- `tools/validate-docs` exit 0 (91 documents + nav); `tools/check-source-shape`
  exit 0; strict MkDocs 1.6.1 build clean; largest touched source 414
  non-blank lines (under the 500 review threshold).

## Changed paths (owned + smallest additive registrations)

`src/shell/clipboard_applet/**` (module now SHARED for the relocatable
boundary, with AGENT-NOTES), `tests/shell/clipboard_applet/**` (five new
files), `docs/wiki/shell/clipboard-applet.md`,
`src/applets/CMakeLists.txt` (additive manifest COMPONENT),
`src/services/clipboard_model/CMakeLists.txt` (additive COMPONENT +
POSITION_INDEPENDENT_CODE for the relocatable shared boundary; archive
remains STATIC, no symbol/ABI/authority change).

## Caveats (bounded)

- The two manifest rows (120/121) remain standard executable rows; their
  binaries are inside the declared clipboard prerequisite target and the
  QML rows force their build, but a bare `ctest` invocation without any
  prior build still reports them "Not Run" like every compiled row in the
  repository.
- Terminal verdict had not been posted at claim time; the repair maps to
  the midpoint findings and remains open to exact-verdict deltas.

Requesting Hopper the 3rd's exact rereview of commit `69b3edc`.
