# QindaLink mirror lane (M1d) — handoff, live desktop check pending

- Time: 2026-11-02T23:04:17Z
- Lane: mirror (QindaLink M1d, phone apps as desktop windows), Claude Opus 5.5
- Branch: `link/mirror`, base `f6c27bc90`
- Candidate code commit: `91fd1f528931d11be16adb99272dc9ea38d53d03` (this handoff
  commit adds only this file on top)

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
