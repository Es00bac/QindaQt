# ACCEPT — 2d99ca7a0ca5aa983757cb4f46ffda6253604248

- Candidate: 2d99ca7a0ca5aa983757cb4f46ffda6253604248
- Base: c05721d48388ce36512172ff6886f9ebd0ffea42
- Verdict: ACCEPT for the bounded pointer selection repair; no blocking findings.
- Reviewed paths: the three Gather tile QML handlers, two Gather regression tests, Gather overview and testing wiki changes, plus task-intent/transport and installed KWin activation/layer-close boundaries by read-only source trace.
- Verification: `ctest --test-dir ../expose-selection-20261010/build/fix -R '^qindaqt\.gather-overview-(surface-qml|composition|controller)$' --output-on-failure` exits 0; 3/3 CTests, 46 Qt cases, zero failures/skips/blacklists, 2.27 seconds. Candidate tests and production QML match the owner's build source. `git diff --check` exits 0. No fresh build.

`ReleaseWithinBounds` gives each enabled tile the exclusive grab needed to prevent the background handler from processing the same mouse/touch tap. The surface test now checks both event counts while synchronous dismissal clears its projection, and it verifies the original task/revision. The composition test checks exactly one real controller intent and resolved target for window, minimized, iconified and container rows. Existing background, Escape, fenced source and stale task rejection remain covered.

The existing composition dispatches the authenticated action before clearing/closing the overlay; no authority bypass or retry was added. Installed `gui-wm/qindaqt-kwin-6.6.6_p1-r6` pins `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`; its `Workspace::activateWindow` raises then force-focuses, and `KeyboardInputRedirection::pickFocus` uses the active window. Layer focus changes activate the overlay only when it wants input; closing an overlay that is no longer active cannot undo the new target via `activateNextWindow`. This trace does not identify an admitted activation suppressed by the overlay.

Bounded caveat: the old-QML regression demonstrates double handling, but an emitted activation alone does not establish that the user-visible no-raise report is resolved. The composition fixture records a task intent through a fake port and creates no layer windows. Root must complete the already-owned native selection check showing the target raised and focused after dismissal, and investigate any concrete failure before reporting installed resolution. This is a remaining delivery check, not a demonstrated defect in the reviewed slice.

Requested next action: integrate the exact accepted candidate after root's bounded native proof and owning integrated gates; preserve separate installed-adoption truth. The unrelated external panel geometry plugin failure is outside this slice and was not rerun.
