# Calendar milestone handoff — event editing, details pane, docs, packaging

- **Commit:** `34013338` on `main` (baseline `737ebf27`) — "Finish the
  Calendar app: real event editing, details pane, docs, packaging"
- **Changed paths:** `src/apps/calendar/**` (controller update path +
  `selectedEvent` map, edit-mode EventEditorDialog, new EventDetailsPane.qml,
  extended UI action probe, idempotent probe data root, stale create-only
  comment removed), `tests/apps/calendar/**` (new `tst_event_update.cpp`,
  CMake registration incl. the two offscreen UI rows),
  `docs/wiki/apps/calendar.md` (new, nav added), `docs/wiki/adr/0096…`
  (updated in place, four components), `docs/wiki/development/gentoo-desktop.md`,
  `docs/TASK_LIST.md` (Calendar portion marked done; other three apps pending),
  `mkdocs.yml`, `packaging/gentoo/gui-apps/qindaqt-apps/…ebuild` (Calendar
  component + `kde-frameworks/kcalendarcore:6`),
  `packaging/gentoo/gui-wm/qindaqt-desktop/…r1.ebuild` + deleted r1 skip
  patch.

## Gate results

- `cmake --build build/dev -j8`: green (0 FAILED).
- `ctest --test-dir build/dev -L calendar --output-on-failure`: **12/12
  pass** (9 rows at baseline; +`calendar-event-update`,
  +`calendar-ui-contract-offscreen`, +`calendar-ui-actions-offscreen`).
  The actions row drives create → edit (summary + weekly recurrence +
  10-min reminder, revision 1, UID stable) → edit (cleared, revision 2) →
  view switching → delete → fixture import through production QML with
  on-disk reload verification; it was run twice consecutively to prove the
  disposable-data-root reset made it idempotent.
- `.cache/handbook-docs-venv/bin/mkdocs build --strict`: clean.
- `python3 tools/docs_validation.py`: clean (220 documents).
- `tools/check-source-shape`: no calendar findings. Three remaining ERRORs
  are other lanes' files (`src/apps/file_manager/ui/EntryGrid.qml`,
  `src/compositor/kwin/kwinhybridsession.cpp`,
  `tests/shell/bluetooth_applet/tst_bluetooth_applet_controller.cpp`).
  `calendar_controller.cpp` sits at 590 non-blank lines — the
  decomposition-review warning band (hard limit 600); the next calendar
  slice should split controller helpers out.

## Edit semantics (the contract)

`CalendarController::updateEvent()` = clone → apply shared validated field
set → `setRevision(prev+1)` + `setLastModified(nowUtc)` → atomic
`EventStore::updateEvent()`. UID never re-minted (AGENT-GUARD in source).
Recurrence "" clears recurrence; reminder "None" clears alarms. Rejections
emit `operationFailed` and leave the store untouched.

## Bounded caveats

- Recurring events edit the **master event**; occurrence-level exceptions
  (EXDATE) are out of scope.
- No CalDAV/network sync, VTODO, attendees, timezone UI, or portal file
  chooser; reminders fire only while the app runs (all documented in the
  new wiki page's exclusion list).
- **Untracked release-lane files left alone intentionally:**
  `packaging/gentoo/gui-wm/qindaqt-desktop/qindaqt-desktop-0.1.0_pre20260909-r2.ebuild`
  and its `Manifest` modification are another lane's in-flight work. I did
  edit the untracked r2 ebuild minimally (removed its PATCHES block, added
  kcalendarcore) because I deleted the r2 skip-patch file it referenced;
  the file remains untracked for its owner to commit. The `Manifest` still
  contains digests for the deleted patch files and needs regeneration
  (`ebuild … manifest`) by the release flow — emerge was not run, per
  instructions.
- The r1/r2 desktop ebuilds pin commit `6c8707df`, whose calendar sources
  predate the compiling app; with the skip patch dropped they must be
  re-pinned to a commit ≥ `34013338` before rebuild.

## Remaining boundary (next lane)

Month/week/day rendering polish, occurrence-level recurrence editing, the
Milestone 2 sync provider reconciling on the stable UIDs, and re-pinning +
re-manifesting the desktop ebuilds.
