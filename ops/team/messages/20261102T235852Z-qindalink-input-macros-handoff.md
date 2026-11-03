# QindaLink input-macros lane handoff

- Lane: input-macros (phone + desktop), branch `link/input-macros`, base `be31feb83`.
- Code commits: `b85cb2ca0` (desktop) and `b8888ca1b` (phone). The handoff commit that carries this file is the candidate.
- Safety: nothing in this lane touched the real RemoteDesktop portal, EIS, `tools/agent-input`, the real keyring or
  `$XDG_DATA_HOME/qindalink`. Tests register a fake portal and fake `InputActions1` under unique service names
  (`org.qindaqt.test.Fake...<pid>`) on a `dbus-run-session` bus and point the code at those names through
  `PortalOptions.service` / the catalog service argument, so they could not reach the real portal even outside a private bus.
  The test binaries are Qt-guiless (no windows).

## Changed paths

- Desktop: `src/services/link_input/**` (portal session, sink, frames, text typer, `input.*` handler),
  `src/services/link_macros/**` (catalog, `MacroOutput`, keysym map, `macro.*` handler),
  `src/services/link_service/app/{main.cpp,input_composition.*,native_input_gate.*}` and its CMake,
  `src/CMakeLists.txt` (two `add_subdirectory` lines), `tests/link/{tst_link_input,tst_link_macros}.cpp`,
  `tests/link/support/{fake_portal,fake_input_actions,input_fixture}.h`, `tests/link/CMakeLists.txt`.
- Phone: `phone/app/android/src/org/qindaqt/link/input/*`, `.../macro/*`, `LinkService.java` and `LinkBridge.java`
  (additive), `phone/app/LinkBridge.{h,cpp}`, QML `TouchpadScreen`, `KeyboardScreen`, `MacrosScreen`, new
  `InputStateNotice.qml` (+ `qml-files.cmake`), `Main.qml` (input session start/stop by page), `ToolsScreen.qml`,
  `phone/preview/{FakeBridge,main}`, `phone/engine-tests/{InputTest.java,run.sh}`.
- Docs: protocol sections `input.*` and `macro.*`, ADR-0436, architecture page, ADR index, `mkdocs.yml`.

## Decisions worth a look

- ADR-0436 (PM-assigned number). `link_input` and `link_macros` are two modules: input is reusable by the vault later
  (`InputSink`, `TextTyper`, `HeldInput`), macros depend on it.
- The phone engine drops `ok`/`error` replies in READY, so replies per the envelope rules are still sent, and the desktop
  also pushes `input.state` (pending, active, denied, unavailable, locked, busy) and `macro.state`. The phone UI uses only those.
  Protocol page documents it. Session.java was not touched.
- Revoke interpretation: on revoke the desktop sends `input.end{reason:"revoked"}`, discards the restore token and ends control;
  the next `input.begin` starts a new portal session (new approval prompt, `error{denied}` if declined). The brief's "error{denied}
  on the next begin" is read this way.
- `MacroRunner` runs with a sentinel `target()` (a portal cannot see focus). Compositor-only actions (maximize, close window,
  show desktop) fail the run with reason `input-unavailable`.
- Production lock gate: `NativeInputGate` (ADR-0320 pattern: attachment + native receipt monitor). The compositor PID is
  the kernel peer of the Wayland socket checked against the bus owner of `org.qindaqt.KWin`, not supervisor-provisioned.
  It is compiled and exercised only on a private bus where it stays closed; it has **not** been proven against a live compositor.
  Until it opens, input and macros are refused with `busy`/`locked`. Verify on first live run; if it stays closed, wire the PID
  the way the shell does.
- Features are composed in `main.cpp` (executable only) because `InputActions` and the new libraries are outside the install
  export set. `--no-input` disables them; `--state-dir` moves the restore token (default `$XDG_STATE_HOME/qindalink`).
- `FeatureHandler` has no on-ready hook, so `MacroFeature` watches `Hub::devicesChanged` to push `macro.list` on READY.

## Evidence

- Desktop: `cmake --build build/input --target qindaqt_link_input_test qindaqt_link_macros_test qindaqt-link-service` then
  `ctest -R 'qindaqt.link-(service-smoke|input|macros|session|bounds)'`: 5/5 passed (input 14 cases, macros 11), repeated
  runs stable after fixing a test race. Cases: begin/approve/deny/unavailable/revoke, every frame mapped to the right Notify call,
  restore-token reuse (0600), release on disconnect/end/lock, controller exclusivity, bounds and rate limit, text batching and
  no text in logs, list sync without steps, chord and text macro runs through the shared runner, cancel/lock/busy,
  compositor-only action failure, `allowed()` false while locked or inactive.
- Phone: `phone/engine-tests/run.sh` exit 0 (vectors, session, repair, feature, plus `InputTest` 60 checks: gestures, 120/s
  coalescing, frame bounds, chords, text chunking, input/macro handlers).
- UI: `phone/preview/run-smoke.sh` exit 0 with 0 QML warnings; `--grab` wrote 41 PNGs (touchpad/keyboard/macros in seven input
  states each) to `build/phone/shots`, inspected; a spinner-shadowing bug (`running`) and an unclear latch style were found and fixed.
- `mkdocs build --strict` passes. `tools/check-source-shape`: no findings in changed files.
- APK: `phone/tools/build-apk.sh` exit 0 (JNI bridge, QML and Java compile); built only, not installed on the phone.

## Not done / caveats

- No on-device run. The user's phone was not touched; the APK was only built.
- Cross-implementation test (real Java feature to real desktop feature over TLS) was not written; frame shapes are covered by
  each side's own tests and the shared protocol page.
- The Android haptics call (`LinkBridge.haptic`) is untested on a device.
- Pair-then-input on the physical phone, and the portal prompt, are not recorded.

## Requested next action and the live-run recipe

The first live run raises a desktop **remote-control approval prompt**. Tell the user before doing it.

1. Integrate the branch, build `qindaqt-link-service` with the normal Portage flow (or run it from the build tree with `--no-adb`
   only if the phone is paired another way) and install the APK built by `phone/tools/build-apk.sh` (adb with `-s R3GL40ABQXM`,
   package `org.qindaqt.link` only).
2. Pair, open Tools > Touchpad on the phone. The desktop shows the portal approval once. Approve.
3. Check: pointer move, tap, hold-drag, two-finger scroll, a short typed text, one macro from Settings > Input > Macros.
4. Lock the desktop: input must stop and held keys release; unlock: input resumes (this proves `NativeInputGate`).
5. Revoke the permission in the desktop's remote-control settings: the phone must show the approval state again.
