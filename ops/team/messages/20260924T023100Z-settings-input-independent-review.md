# A04 Input pointer repair independent review

Reviewed exact candidate HEAD a2115b6f50e8020cf527025c988cbda2b8b05d63 against base 7ba705bf53fde8887ca38c89ad24c85b3823ee68 and current main d9cd43f570b670cf29bb0e97a7a3117316ccd71f.

Verdict: blocked on one public lifetime-contract defect. Pointer behavior otherwise looks correct in the reviewed paths.

Verified behavior:
- Selection swaps use each inventory snapshot as confirmed state, so A -> B -> A restores A's readback, not a speculative value.
- Empty inventory and device removal clear the selected index and all capability rows; owner replacement immediately clears the old inventory and fences pending completions.
- Production list/write requests capture KWin's unique owner and route every Get, Set, rollback, and readback to that destination. The owner-after check rejects a transaction crossing replacement. The private-bus test proves a replacement with the same device ID receives no stale Set.
- Multi-property edits snapshot the original values, roll back completed Sets after refusal or ineffective readback, then publish the final authority snapshot. Fake-port model tests cover failure and ignored second properties.
- Availability comes from the authority's capability properties/device kind; the QML controls bind back to model state. The added UI test checks the rejected edit returns to its confirmed value.

Blocking finding — src/apps/settings/input/include/qindaqt/apps/settings_input/pointer_device_port.h:45-48 promises production requests are cancelled by receiver destruction, but KWinPointerDevicePort::requestWrite() starts the side-effecting operation with QtConcurrent::run() at kwin_pointer_device_port.cpp:249. Destroying the receiver destroys the watcher/callback context, not the already-running operation; Qt documents that a started basic-mode QtConcurrent::run() future cannot be cancelled (https://doc.qt.io/qt-6/qfuturewatcher.html). The Settings view/model can be destroyed while a queued worker still writes to KWin, with the completion silently dropped. Please either enforce the cancellation/fencing contract before the worker can issue Sets, or change the public contract and provide an explicitly accepted lifetime behavior. This wasn't covered by the current tests. No receiver-destruction source edits were made during this review.

Focused verification on qinda candidate build: ctest --test-dir build/input -R 'qindaqt.settings-input-(pointer-port|pointer-devices-model|page)$' --output-on-failure passed 3/3: pointer port (including unique-owner replacement), pointer model (selection/empty/owner/rollback), and offscreen Input page.

Current main already contains an equivalent pointer implementation tree (main history has commit 0c41a5e3); all compared pointer source, model/port tests, and widget behavior match. The one merge conflict is tests/apps/settings/input/tst_input_page.cpp. Resolve by retaining both the candidate's rejectedPointerEditRestoresControl() test and main's touchDestinationLoadsAndMouseClickEdits() additions, along with the touch fixture includes/properties/setup and navigation-comment update from main. The candidate Input wiki page auto-merges with main's touch additions.

No candidate or main source files were edited; this timestamped reply is the required board record.
