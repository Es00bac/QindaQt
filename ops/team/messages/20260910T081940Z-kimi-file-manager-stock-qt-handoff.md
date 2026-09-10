# File Manager stock-Qt conversion + S3 daily-use features — handoff

**Worker:** kimi (File Manager lane)
**Base:** work committed directly on `main` in the shared checkout per manager instruction
**Commits:**

1. `337edf3a` — "Build the File Manager app on stock Qt Quick Controls" (ADR-0116 presentation conversion; 29 files; landed earlier, verified 28/28 rows at the exact committed tree).
2. `5d54c653` — "Add File Manager clipboard, drag-and-drop, properties, and recursive search" (36 files, +2960/−142).

## What landed in 5d54c653

- **Clipboard** (`model/clipboard_controller.{h,cpp}`): `edit.cut`/`edit.copy`/`edit.paste` (Ctrl+X/C/V, Edit menu, view context menus). Own snapshots carry listing-time identity maps through the identity-checked batch contract; a committed cut paste clears the snapshot, a failed one keeps it. Foreign clipboard content and foreign URL drops are adopted copy-only via `MutationController::copyForeignPathsTo` (fresh stat at dispatch, 4096-path cap). Paste-into-self/parent/descendant refused before dispatch. GNOME `x-special/gnome-copied-files` + KDE cut markers published and parsed.
- **Drag-and-drop** (`ui/EntryDrag.js`, delegates, `PlacesSidebar`): internal drags carry a JSON identity snapshot under a private MIME type; move by default, Ctrl copies; drops land on folder delegates, the view background (current folder), and places/bookmark rows. The Trash place is excluded so a drop cannot bypass the `.trashinfo` record.
- **Properties** (`model/entry_properties.{h,cpp}`, `ui/PropertiesDialog.qml`, `file.properties` Alt+Return): single-entry fields synchronous (kind/MIME/size/modified/symbolic permissions/full path); folder and multi totals on one bounded worker (20000 visited, 32 depth, no symlink descent, cancellable).
- **Recursive search** (`model/search_controller.{h,cpp}`, FilterBar Subfolders toggle): bounded (2000 results / 24 depth / 100000 visited), no symlink descent, case-insensitive name substring, token-fenced `searchReady`; results publish as a `NavigationController` guest listing that any navigation/refresh discards; a committed mutation restarts the in-flight search.
- **Enabled-state binding** (`app_shell/file_manager_transfer_actions.{h,cpp}`): cut/copy track idle + non-empty selection; paste tracks idle + clipboard content.

## Two defects found and fixed during verification (worth reviewer attention)

1. **Offscreen QPA clipboard ownership:** the offscreen platform never reports `ownsClipboard()`, so the foreign-adoption path stomped our own cut snapshot right after publish. `ClipboardController` now keeps the own snapshot when the incoming payload's local paths equal it (see the AGENT-NOTE in `clipboard_controller.cpp`).
2. **Latent use-after-free in the new workers:** Qt 6.11 delivers queued context-bound slot calls even after the *sender* is destroyed (proven empirically: 50/50 delivered after `delete`). The original `finished`-lambda pattern dereferenced a joined-and-deleted `QThread`. Both `SearchController` and `EntryPropertiesController` now report results **by value** in one completion signal; cancel/stop invalidates the token/generation *before* join+delete; late deliveries are fenced and never touch the worker. Hammered 20× (search/properties) and 10× (clipboard) with zero flakes. The same pattern may be worth auditing elsewhere in the tree.

## Changed paths

- `src/apps/file_manager/**` (model/mutation/app_shell/runtime/ui, incl. new `mutation/mutation_controller_foreign.cpp`, `ui/EntryListDelegate.qml`, `ui/PropertiesDialog.qml`, `ui/EntryDrag.js`)
- `tests/apps/file_manager/**` (3 new unit rows + probe/catalog/browsing/visual updates)
- `docs/wiki/apps/file-manager.md`, File Manager entries in `docs/TASK_LIST.md`

## Gates (all at committed tree)

- Full `cmake --build build/dev -j8` green.
- `ctest --test-dir build/dev -R '^qindaqt\.file-manager'`: **31/31** repeatedly (28 existing + new `clipboard-controller`, `recursive-search`, `entry-properties`); QML-driving rows rerun 3× after the delegate extraction.
- `python3 tools/check-source-shape`: **no file-manager ERRORs remain** — the pre-existing EntryGrid qsTr false positive is gone (braced Keys handlers), the probe S3 stage was extracted, the foreign batch split into `mutation_controller_foreign.cpp`, and the Details delegate into `EntryListDelegate.qml`. Only remaining lane note: `Main.qml` keeps its pre-existing decomposition WARNING (303→346 non-blank; threshold 275). Two pre-existing other-lane ERRORs unchanged (`kwinhybridsession.cpp`, `tst_bluetooth_applet_controller.cpp`).
- `.cache/handbook-docs-venv/bin/mkdocs build --strict` and `python3 tools/docs_validation.py` both pass.

## Caveats / bounded limits

- QML DnD handlers (EntryDrag.js guards, DropArea wiring) get construction-level coverage via the UI-contract/UI-action rows; unit coverage sits at the controller seams (`dropUrlsInto` filtering/dedup/cap, paste guards, dispatch kinds).
- The selection UI probe asserts selection via identity **keys**: on this Qt 6.11 build the QJSValue→QVariant seam corrupts nested value objects (keys arrive intact; values can mismatch). Production dispatch is unaffected (pure-QML `tst_selection` passes unchanged).
- Per-volume Trash, mounts, network locations remain open (S4–S5); preview pane, open-with chooser (needs an ADR widening ADR-0029), and optional permanent deletion remain open in S3. Wiki roadmap and TASK_LIST updated to match.

**Requested next action:** integrate/review commits `337edf3a` and `5d54c653` on `main`; no rebase needed (already on main). File Manager lane is clear for the next outcome.
