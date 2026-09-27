# Same-reviewer retained-menu fixture repair — ACCEPT

Exact candidate `292cd361adb15f9d3ace5a1cb2f4a3e277e03238`. Findings P0/P1/P2/P3: **0/0/0/0**.

Inspected the full test/docs-only diff over accepted `e37f01f9` in isolated `../container-wm-shell-keyboard-review`. There is no production focus workaround. The test retains all dispatch, revision, pending-disable and reopen assertions. It adds the popup's actual closed transition, the same retained menu object, and an explicit row active-focus assertion before sending Menu. `window.requestActivate()` plus exact `QGuiApplication::focusWindow()` merely establishes the recipient for subsequent synthetic offscreen input after Popup.Window hides; the existing launcher fixture at `tests/shell/launcher/tst_launcher_qml.cpp` already uses that identical compensation. Waiting for closed also prevents input being sent while the real exit transition is still running.

Independently reran `qindaqt.task-list-applet-qml-keyboard-offscreen` in worker build/performance: **1/1 pass**, exit 0. Worker HEAD is a records-only descendant (`4fe3bbe7`); diff to candidate contains only a new handoff Markdown file, with no production or test drift. No manager build or physical session was touched.

This accepts deterministic retained-menu behavior and fixes a test input precondition without weakening production assertions. It does not establish physical/native compositor focus restoration; that remains a separate qualification boundary, as does the manager's installed/staged/relocated consumer gate. Requested next action: integrate this exact test/docs repair, rebuild its focused test and finish native deployment gates. Same-reviewer repair loop complete; available for bounded integration reproduction.
