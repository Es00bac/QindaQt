# QindaLink mirror lane (M1d) — handoff (repair round and live checks appended)

- Time: 2026-11-02T23:04:17Z
- Lane: mirror (QindaLink M1d, phone apps as desktop windows), Claude Opus 5.5
- Branch: `link/mirror`, base `f6c27bc90`
- First candidate: `91fd1f528931d11be16adb99272dc9ea38d53d03`. It was
  reviewed as ACCEPT-WITH-P1s.
- **Current candidate:** `cc1e878145317af82199a7d40c35822324601970`. It
  includes the merge of main `be31feb83` and the repair round. The final
  handoff commit adds only this file's update on top.

## Outcome state

The server and the desktop executable are complete and committed. The live
check is incomplete because the phone (R3GL40ABQXM) dropped off USB at about
22:05Z. It no longer showed in `lsusb`, and it did not return within an hour
of bounded waiting.

- **Live-verified on the phone:** the hidden API probe, creating the virtual
  display, launching the app, H.264 encoding and inventory icons. The client
  was a scripted Python one, and its stream was decoded with ffmpeg.
- **Verified end to end without the phone:** the desktop executable, against
  a scratch fake adb and fake server.
- **Not verified live:** the desktop executable against the phone, a visible
  input change, live resize, and the per-display wake lock.

## Changed paths

- `phone/mirror_server/**`: `Server`, `Session`, `Connection`, `Frames`,
  `HiddenApi`, `VirtualDisplayHost`, `ScreenEncoder`, `InputInjector`,
  `AppLauncher`, `AppInventory`, `DisplayPower`, `Ln`, plus `build-dev-jar.sh`.
- `src/apps/link_mirror/**`: the pure `qindaqt_link_mirror_core` (protocol,
  messages, input mapper, key mapping, decoder selection) and the
  `qindaqt-link-mirror` executable (adb session, connection, FFmpeg decoder,
  decode worker, window, controller, options, evidence runner, main).
- `tests/apps/link_mirror/**`: the framing, input-mapping and
  decoder-selection tests.
- Additive edits to `src/CMakeLists.txt` (option `QINDAQT_BUILD_LINK_MIRROR`,
  default ON), `tests/CMakeLists.txt` and `mkdocs.yml`.
- Docs: new `docs/wiki/reference/qindalink-mirror-v1.md`, and the mirror rows
  plus the "Phone apps on the desktop" section of
  `docs/wiki/architecture/qindalink.md`.

## Verification (exact)

- **Configure.** `cmake -S . -B build/mirror -G Ninja -DBUILD_TESTING=ON
  -DQINDAQT_BUILD_SHELL=OFF -DQINDAQT_BUILD_KWIN_PLUGIN=OFF
  -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=libexec` exited 0. The
  two install-path flags are needed only because
  `src/services/portal/capture` compares install paths with the installed
  fork headers. The brief's plain command fails there.
- **Build.** `cmake --build build/mirror --target qindaqt_link_mirror_core
  qindaqt-link-mirror qindaqt_link_mirror_{framing,input_mapping,decoder_selection}_tests`
  exited 0 with zero warnings under `-Werror -Wconversion -Wsign-conversion
  -Wshadow`. No job flags were passed.
- **Tests.** `ctest -R '^qindaqt\.link-mirror-'`: 3/3 passed in 0.46 s
  (`framing`, `input-mapping`, `decoder-selection`).
- **Docs.** `python3 tools/validate-docs` checked 588 documents and exited 0.
  `mkdocs build --strict -d build/mirror/site` exited 0.
- **Source shape.** `tools/check-source-shape --json` reports 0 issues in
  lane files. Its 173 pre-existing issues are elsewhere. The largest lane file
  is `Session.java` with 292 non-blank lines.
- **Java/C++ cross-check.** On the host JVM, `Frames.read` rejects the same
  malformed headers that `FrameDecoder` rejects, and accepts a valid hello.
- **Live server (SDK 37), scripted client:**
  - `Server probe` printed `hidden API complete` and exited 0.
  - The display flags were `0x17d49`, the full trusted set, on display 6/7.
  - `c2.qti.avc.encoder` produced High profile at 720x1440 with 8 Mb/s: 111
    packets in 4 s.
  - The calculator launched on the display. An ffmpeg-decoded frame showed
    its UI, with no personal content.
  - The inventory returned 1 app with an 8 364-byte base64 PNG.
  - The server exited 0 on `stop`.
- **Desktop against the fake adb and server** (an x264 720x1440 test
  pattern):
  - `--grab … --input tap:216,450 --input scroll:216,450,-2 --input text:hello
    --input key:back --input resize:400x600 --grab-after-input …` exited 0
    with `decoder=vaapi`.
  - The fake server saw a matching token, then `start 432x900 dpi=160`, then
    a touch down and up at display (360,720), then `scroll v=-2`, then 5 text
    frames, then BACK down and up, then `resize 400x600`, then `stop`. The
    forward was removed.
  - The grab showed correct colours and letterboxing.
  - `--probe` measured 30.5 fps on both vaapi and software. `--list` exited
    0. SIGTERM sent `stop`, removed the forward and exited 0.

## Bounded caveats

1. **Live input is unproven.** The one live tap attempt reached display 9
   (`InputDispatcher: Inject motion … d=9`). However, logcat showed
   `non-interactive displays={0, 9}`: the phone was asleep, and the new
   display group started non-interactive, so the calculator ignored the
   taps. The fix, `DisplayPower`, holds a screen wake lock bound to the
   display id with `ACQUIRE_CAUSES_WAKEUP`, falling back to `KEYCODE_WAKEUP`
   on the display. It is committed but unverified. Also check that it does
   not turn on the phone's own screen.
2. **Not exercised against the phone:** the desktop executable, live resize
   and window title/icon.
3. **New dependency.** libavcodec, libavutil and libswscale are now needed,
   with VA-API through hwcontext. The desktop package needs
   `media-video/ffmpeg[vaapi]`, or `-DQINDAQT_BUILD_LINK_MIRROR=OFF`. An ADR
   or ADR-0433 consequence note for this dependency is the PM's call, so ADR
   numbering stays with the PM.
4. **The brief names `src/apps/CMakeLists.txt`,** which does not exist. The
   registration is in `src/CMakeLists.txt`.
5. **One finger.** The desktop drives a single finger. Multi-touch is in the
   protocol only.

## Requested next action

Once the phone is back on USB and unlocked, run these from the worktree. Use
only the calculator, and inspect the PNGs before quoting them.

```
phone/mirror_server/build-dev-jar.sh
M=build/mirror/src/apps/link_mirror/qindaqt-link-mirror
J=build/mirror/qindalink-mirror-dev.jar
$M --serial R3GL40ABQXM --package com.sec.android.app.popupcalculator --server-jar $J --probe --verbose
$M --serial R3GL40ABQXM --package com.sec.android.app.popupcalculator --server-jar $J \
   --grab build/mirror/calc-before.png --grab-after-frames 30 \
   --input tap:<X,Y of a digit from calc-before.png> --input text:12 \
   --grab-after-input build/mirror/calc-after.png --verbose
$M ... --grab build/mirror/r1.png --input resize:600x600 --grab-after-input build/mirror/r2.png
```

Confirm three things:

- The verbose line `display N held awake (interactive)` appears.
- The after-grab shows the typed digits.
- `adb shell dumpsys power` shows the default display's wakefulness
  unchanged.

Finally, clear the calculator with a tap on C. If the wake lock signature
differs on SDK 37, `Server probe` names it in its notes.


## Repair round (2026-11-02T23:57Z), review of 034709d51

### Merge

- `git merge main` (be31feb83) became `5f646c549`.
- Conflicts were only in `mkdocs.yml` and
  `docs/wiki/architecture/qindalink.md`. Both sides were kept: main's
  phone-UI rows plus the mirror rows.
- `phone/tools/build-apk.sh` stages `phone/mirror_server/src/main/java`.
- The APK it builds (`build/phone/QindaLink-debug.apk`, final sources)
  contains all 17 `org.qindaqt.link.mirror.*` classes, including the public
  `Server` and the final `DisplayPower`. Checked with dexdump and strings.

### P1

1. **Server lifetime** (`ListenPolicy`, `Server`, `Session`).
   - A peer gets 5 s to complete hello (socket read timeout).
   - The server exits after 3 failed greetings (exit 5).
   - The accept-deadline watchdog calls `System.exit(4)` when no desktop
     completed hello. That also covers a stdin that never delivers the
     secret.
   - After hello, 30 s without a frame ends the session.
2. **Secret** (`SessionSecret`, `adb_session.cpp`, `mirror_controller.cpp`).
   - The 32-hex secret goes only to the server's stdin via `adb shell -T`,
     so there is no pty echo, and inside hello, compared in constant time.
   - argv, the abstract socket name and the forward spec carry only a 16-hex
     suffix. With the fake rig, a scan of every logged adb argv found no
     32-hex string.
   - The AGENT-GUARD wording is corrected. The reference page now has a
     "Threat model" section: single-user desktop; what a same-uid process can
     still do.
3. **ADR-0435** (`docs/wiki/adr/0435-decode-qindalink-streams-with-ffmpeg.md`).
   - It records the licence, VA-API with software fallback, untrusted parsing
     confined to the mirror process, the caps, and the packaging obligation:
     `gui-wm/qindaqt-desktop` must DEPEND on `media-video/ffmpeg[vaapi]` or
     pass `-DQINDAQT_BUILD_LINK_MIRROR=OFF`. That overlay edit is not made
     here.
   - It is in `docs/wiki/adr/index.md` and `mkdocs.yml`.
   - With FFmpeg missing, configure fails with a message naming the option.
     Proven by configuring `src/apps/link_mirror` with an empty
     `PKG_CONFIG_LIBDIR`.
4. **Backlog** (`BacklogGate` in `qindaqt_link_mirror_core`, `DecodeWorker`).
   - Past 8 in-flight packets, non-key packets are dropped until a keyframe,
     and one `sync` is sent. Config and key packets always pass.
   - The gate has unit tests.

### P2

- **Caps.** A display or format size beyond 8192 per side or 64 MP is
  rejected, as is any decoded picture beyond them. The `QImage` allocation is
  checked.
- **adb output.** stdout and stderr are separate, and the forward port is
  read from the last non-empty stdout line. The fake rig printed a notice line
  first.
- **Keepalive.** The desktop sends `ping` (0x0B) every 5 s, and the server
  times out after 30 s idle.
- **Phone input.** Coordinates are clamped and NaN becomes 0. Buttons are
  masked. Keys pass only an allowlist that excludes POWER, SLEEP, WAKEUP,
  HOME, ASSIST, APP_SWITCH, volume, call, camera, brightness and META
  (`InputPolicy` and `InputCommands`).
- **Wake fallback removed.** `DisplayPower` has no KEYCODE_WAKEUP fallback.
  Live finding: see below.
- **Dev jar.** It is pushed to the content-addressed path
  `/data/local/tmp/qindalink-mirror-<sha16>.jar` and verified with
  `sha256sum` before and after. An unchanged jar is not re-pushed.
- **App exit.** `TaskWatcher` polls `getAllRootTaskInfosOnDisplay`, and the
  server sends `app-exited` (0x84). The window shows "App closed on phone" and
  closes after 2 s with exit 0; headless runs exit 3.
  - Fake rig: windowed (offscreen platform) exit 0 after 4.5 s; headless
    exit 3.
  - Two races were fixed so the server's close after app-exited is not
    reported as an ADB loss.

### Defects the live checks found, fixed in cc1e87814

- **Power group.** On SDK 37 the mirror display reports `displayGroupId 0`
  even with `FLAG_OWN_DISPLAY_GROUP`, and `dumpsys power` recorded the
  "per-display" lock as `powerGroupId=0`. That lock held the phone's own
  power group.
  - Fix: the lock is taken only for a non-default group. In group 0 the
    mirror follows the phone's own power state: it needs the phone awake,
    and otherwise fails with `display-asleep`.
- **App exit.** BACK closed the calculator before the first task poll, so
  the app was never "seen" and no exit was reported.
  - Fix: a 3-poll launch grace.
- **Decoder fallback.** After a VA-API→software fallback, the new decoder
  never received SPS/PPS.
  - Fix: the selector replays the generation's config packet (new test), and
    the encoder sets `KEY_PREPEND_HEADER_TO_SYNC_FRAMES`.

### Tests and checks (final tree)

- `ctest -R '^qindaqt\.link-mirror-'`: 3/3 passed (framing,
  input-mapping, decoder-selection).
- `phone/mirror_server/tests/run.sh` (new host-JVM tests for Frames,
  SessionSecret, InputPolicy, ListenPolicy): 78 passed, 0 failed.
- Fake rig, offscreen:
  - `--grab … --input tap/scroll/text/key/resize --grab-after-input`: exit 0,
    vaapi.
  - `--probe`: 30.4 fps, 0 dropped.
- `python3 tools/validate-docs`: 590 documents, exit 0.
- `mkdocs build --strict`: exit 0.

## Live checks on R3GL40ABQXM (SDK 37), final jar

Only the calculator was used. Only mirrored-display frames were captured,
all under `build/mirror/live/`, all inspected: calculator UI only, no
personal content. The phone was never locked, and `org.qindaqt.link` was
not installed or removed.

1. **Tap, scroll, text and keys.**
   - Before-grab showed "0". The run sent tap 7, text "45", Backspace and a
     scroll. The after-grab showed "74".
   - Server log: 6 KeyEvents and 3 MotionEvents (tap down/up and the
     scroll), all on the mirror display, all `-> true`.
   - Files: `final-input-{before,after}.png`.
2. **Resize to 600x600.**
   - One `resize` produced one encoder restart (`c2.qti.avc.encoder h264
     600x600`) and one `display` message at 600x600.
   - The decoder reopened and the after-frame is 600x600. The calculator
     re-laid out into the square (`final-resize-after.png`).
3. **App exit.**
   - BACK on the calculator: the task polls went "0 of 1" then "0 of 0
     live", the server sent `app-exited (no-tasks)`, and the headless run
     exited 3.
   - The windowed close-after-2 s path was proven on the fake rig.
4. **The phone's own screen.**
   - `dumpsys power | grep mWakefulness` showed `mWakefulness=Awake` and
     `dumpsys display | grep mScreenState` showed `mScreenState=ON`, both
     before the first run, during a session and after the last run.
   - `Display Power Controllers: size=1` before and after; `size=2` only
     while a mirror display existed.
   - With the final jar, no `qindalink-mirror` wake lock was held during a
     session.
   - Earlier jar (before cc1e87814): the lock was held as `powerGroupId=0`.
     That is the defect fixed above. The phone was awake throughout, so the
     asleep case was not exercised.
5. **Cleanup.** All `qindalink-mirror*.jar` files were removed from
   `/data/local/tmp`, and no mirror `app_process` was left running.

## Remaining bounded caveats

- **The phone must stay awake** on this device (display group 0). When its
  screen times out, the mirror pauses. Keeping the phone screen on while
  mirroring would need a deliberate group-0 wake lock, which is a product
  decision for the PM and user.
- **Host load.** During the live runs the host load average was about 84
  (other lanes compiling), and the GPU was idle. Some runs had a slow VA-API
  open (3–6 s) and early backlog drops (`dropped=12`, then recovery through
  `sync`). Two probe runs (one on the phone, one on the fake) decoded no
  frames, because the decoder had not opened within their 2–3 s windows.
- **Not exercised:** the asleep-phone path and a mid-stream VA-API failure.
  The second is covered by unit tests only.
- **Packaging.** The FFmpeg DEPEND for the overlay ebuild is still to do
  (ADR-0435).

## Requested next action

Re-review the repair (c2b95b168 + cc1e87814) and integrate. Then decide
whether mirroring should hold the phone's screen on, as noted above.
