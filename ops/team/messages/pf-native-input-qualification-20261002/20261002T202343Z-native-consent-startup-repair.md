# Actual combined first failure and minimal setup repair

Exact first command source dd8fae7c1, core195417, artifacts50b871/c391f4. Real AMD5700XT renderer, actual core Ready, native five-family frontend exports, PortalCapture1 first-caller acceptance/different-caller rejection, session and source selection succeeded. StartResponse2 occurs before mapped consent or capture audit. Fixture2QtPASS1FAIL0SKIP1276ms; runner EXIT1/5.849s. Cleanup/core arrays empty. Evidence immutable under build/final-native-20261002/runtime/combinedRemoteDesktopSharesFramesClipboardAndCloses/first.

Source cause: consent_input.cpp compares Wayland SO_PEERCRED PID to QINDAQT_PORTAL_TEST_COMPOSITOR_PID. The caller sets it only after compositor launch; compositor-launched protected broker and its consent child already inherited an environment without it, unlike the historical in-process resident fixture. The test helper therefore exits3 before mapped audit.

The smallest test-only runner repair exports the actual shell PID immediately before exec of the same exact native driver. Exec preserves Popen PID; inherited expected PID, actual public compositor PID and actual Wayland peer check remain identical and real. No helper, compiler, core, production, sentinel or grant change. Coordinated with manager; first failure retained, source/Python/doc checks precede one bounded combined replay.
