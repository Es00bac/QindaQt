# Joan Clarke — independent first-party route review

| Field | Value |
|---|---|
| Persona | Joan Clarke (`joan-clarke`) |
| Provider / model | OpenAI Codex `gpt-5.6-sol`, reasoning high |
| Exact candidate SHA | `ce66a98dd4835048761142425253c72af89b77e2` |
| Tree SHA | `bd4b8603b43e8d042953bd812592f11687c9622f` |
| Sole parent SHA | `74da46345c7a5094d45c756ad8b23ca87591fcd3` |
| Base SHA | `74da46345c7a5094d45c756ad8b23ca87591fcd3` |
| Review worktree | `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route-codex-review` |
| Build root | `/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex` |

## Findings ledger

### P0

None.

### P1

#### P1-1 — A valid Audio1 projection can make Settings host entry target a disabled control

`src/apps/settings/audio/qml/AudioDeviceSection.qml:52-56` chooses the first
default output's volume slider as `firstActionTarget` without checking whether
that slider is enabled. `AudioPage.qml:16-20` publishes it as the page target,
and `src/apps/settings_center/SettingsRouteHost.qml:39-51` calls
`forceActiveFocus()` and returns success without confirming that focus moved.

This is reachable with a valid public Audio1 snapshot. The protocol permits a
default output with `canSetVolume == false`; the route then truthfully projects
`volumeAvailable == false` at
`src/apps/settings/audio/audio_settings_projection.cpp:91-96`. A second output
may simultaneously expose an admitted set-default action. The route therefore
has an enabled action but tells the host to focus the disabled first slider.
This violates the Settings Center contract that Tab from a route tab enters the
active page (`docs/wiki/apps/settings-center.md:85-102`) and the Audio page's
declared focus-entry contract (`docs/wiki/apps/audio-settings.md:81-90`).

Reproduction (scratch source is outside the worktree at
`/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro.cpp`):

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
QML_IMPORT_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/qml \
LD_LIBRARY_PATH=/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens \
/home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro
```

Observed: exit 1 and

```text
firstFocusTarget= audioOutputVolume_10 enabled= false
activeFocusItem=
```

Expected: the target is enabled and becomes the window's active focus item
(here the enabled `audioOutputDefault_12` is the first admitted fallback).

#### P1-2 — Reverse Tab is hard-wired to loop on Close

When Retry is hidden, `src/apps/settings/audio/qml/AudioPage.qml:247-255` sets
the Close button's `KeyNavigation.backtab` to the Close button itself. This
prevents reverse traversal to the last enabled route control and contradicts
the documented forward-and-reverse route cycle. The sole compact keyboard test
at `tests/apps/settings/audio/tst_audio_page.cpp:241-264` checks only forward
Tab from Close, so it is tautologically green against this direction.

The same scratch reproduction observed:

```text
reverseTabFromClose= audioCloseButton
```

Expected: reverse Tab moves to the preceding enabled Audio control while
remaining within the route.

### P2

#### P2-1 — Settings Center never exercises the integrated Audio route's keyboard or accessibility contract

`tests/apps/settings_center/tst_settings_navigation_page.cpp:156-161` has no
Audio stub; all three normal `Main.qml` constructions at lines 217-226,
306-315, and 380-389 omit `audioSettings`; and its shortcut/host-entry coverage
stops at Network/Ctrl+4 at lines 420-437. The candidate adds the Audio QML build
dependency in `tests/apps/settings_center/CMakeLists.txt:72-80`, but adds no
Ctrl+5, Audio loader, wide/compact route-tab entry, Escape return, or Audio
accessible-name/role assertion.

Reproduction:

```sh
rg -n 'audioSettings|Key_5|Ctrl\+5|audioPage' \
  tests/apps/settings_center/tst_settings_navigation_page.cpp
```

Observed: exit 1, no matches, while the Settings Center selector still passes
9/9 in both configurations. Expected: an injected Audio model and assertions
that construct/select the Audio page and prove tab entry plus accessible
names/roles in both the 720x520 and 440x360 host layouts. The standalone page
test covers 900x760 and 420x320 and checks Button/Slider roles, but it neither
tests the host transition nor the disabled-target negative control above.

### P3

#### P3-1 — The owning architecture page still says the Audio UI is unimplemented

`docs/wiki/architecture/audio-service.md:134-139` still describes the Settings
view model as future work and states that neither UI is implemented or
qualified. The candidate implements and documents the route. This is stale
architecture text for the Program Manager to correct during integration, as
the implementer's handoff already noted.

#### P3-2 — The shared Settings Center test edit is not strictly append-only

The route/enum/host/main registrations preserve all prior routes and append
Audio last, but `tests/apps/settings_center/CMakeLists.txt:148` replaces the
existing route-construction timeout from 15 to 25 seconds. Reproduction:

```sh
git diff --unified=0 \
  74da46345c7a5094d45c756ad8b23ca87591fcd3..ce66a98dd4835048761142425253c72af89b77e2 \
  -- src/apps/settings_center tests/apps/settings_center
```

Observed: the timeout replacement in addition to the semantic extensions.
The increase is bounded and understandable for a fifth internally bounded
route process, so this is precision against the lane's literal append-only
constraint rather than a blocking product defect.

## Review-question evidence

1. **Public-client-only composition:** The route library links only
   `QindaQt::AudioClient`, `QindaQt::AudioProtocol`, and Qt Core
   (`src/apps/settings/audio/CMakeLists.txt:16-19`). The application composes
   the public `QtAudioTransport`/`AudioClient`; no route source includes the
   service, PipeWire/WirePlumber, or Qt D-Bus. The boundary test scans C++/QML,
   CMake, and Settings main, and its poison row positively permits the closed
   surface before rejecting service, QtDBus, WirePlumber, private transport,
   extra invokable, text-entry, and stream-move poisons
   (`tests/apps/settings/audio/check_boundary_negative.cmake:39-124`). Both
   boundary rows passed in Debug and Release. Operation visibility otherwise
   matches public-client admission: `snapshotAdmitsOperation()` checks pending,
   snapshot, owner, Ready/Degraded, and capability
   (`audio_settings_model.cpp:59-75`), and projection adds the target can-set
   fences (`audio_settings_projection.cpp:60-97`). The fake-transport tests
   pass owner-loss, owner-replacement, epoch-replacement, stale-handle,
   foreign/late completion, and retained-stale-truth cases
   (`tst_audio_settings_model_adversarial.cpp:45-188`). P1-1 is a presentation
   focus defect, not a widening of mutation admission.

2. **Audio1 behavior:** The page projects only public outputs, inputs,
   defaults, device/stream volume and mute, and stream inventory; no stream
   movement, profile, port, channel, or persistence action is invented. The
   public validator rejects zero/malformed lineage, duplicate/unsorted/global
   serial collisions, stale targets, NaN and >1 levels, overlong names/reasons,
   and oversized lists (`audio_validation.cpp:90-215`). The existing injected
   protocol/client hostile rows were executed adjacent to this candidate and
   passed 2/2 in each profile. The valid capability mixture used for P1-1 is
   an additional negative control the candidate page test lacks.

3. **Settings Center integration:** Audio is the deterministic fifth route and
   Ctrl+5 target; enum, loader, RPATH/import, construction, and installed-stage
   package rows follow Network and passed. Route construction uses an absent
   session-bus path (`check_route_construction.cmake:28-48`); installed package
   poisoning withholds the staged Audio module while the build module remains
   present (`check_installed_routes.cmake:193-237`). Keyboard/accessibility
   integration is not proved and is defective as described in P1-1, P1-2, and
   P2-1. The Audio page test was additionally run under
   `QT_FATAL_WARNINGS=1` in both profiles and remained green, demonstrating the
   negative-control gap rather than clearing it.

4. **Additive registry edits:** Product registration remains semantically
   additive and prior route ordering is preserved. Existing count/bounds/wrap
   expectations are extended from four to five. The literal exception is the
   test timeout replacement in P3-2; no shared registry entry was removed or
   reordered.

5. **Documentation:** The new route page accurately limits protocol and
   packaging scope, but its keyboard-cycle and page-test proof claims are false
   for P1-1/P1-2 and incomplete per P2-1. The known stale architecture statement
   is confirmed in P3-1.

## Commands and results

All commands below ran from the review worktree unless an absolute path says
otherwise. No nested compositor/session row, live D-Bus service, hardware,
uinput, or network was used.

### Identity and cleanliness

```sh
git rev-parse HEAD
git rev-parse HEAD^{tree}
git rev-parse HEAD^
git rev-list --parents -n 1 HEAD
git status --porcelain
```

Result: exit 0; candidate/tree/sole-parent matched the metadata above. Status
was empty before review and after all review execution.

### Configure

```sh
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON

cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/release -G Ninja \
  -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

Result: both exit 0. CMake emitted nonfatal existing runtime-search-path
warnings; generation completed.

### Build

For each of `debug` and `release`:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  --parallel 3 --target \
  qindaqt_settings_audio qindaqt_settings_audio_qml qindaqt-settings \
  qindaqt_settings_audio_model_tests \
  qindaqt_settings_audio_model_adversarial_tests \
  qindaqt_settings_audio_page_tests \
  qindaqt_settings_route_registry_test \
  qindaqt_settings_navigation_controller_test \
  qindaqt_settings_navigation_page_test
```

Result: Debug exit 0; Release exit 0.

Adjacent public-boundary targets, for each profile:

```sh
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  --parallel 3 --target qindaqt_audio_protocol_tests qindaqt_audio_client_tests
```

Result: Debug exit 0; Release exit 0.

### Tests

For each profile:

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.settings-audio-' --output-on-failure --no-tests=error
```

Result: Debug 5/5 passed, exit 0; Release 5/5 passed, exit 0.

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.(settings-(route-registry|navigation-controller|navigation-page)|settings-app-(offscreen|rejects-(unknown-route|missing-theme)|desktop-identity|route-construction|installed-routes))$' \
  --output-on-failure --no-tests=error
```

Result: Debug 9/9 passed, exit 0 (32.00 s); Release 9/9 passed,
exit 0 (31.28 s).

```sh
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.audio-(protocol|client)$' --output-on-failure --no-tests=error
```

Result: Debug 2/2 passed, exit 0; Release 2/2 passed, exit 0.

```sh
QT_FATAL_WARNINGS=1 ctest --test-dir \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/<profile> \
  -R '^qindaqt\.settings-audio-page$' --output-on-failure --no-tests=error
```

Result: Debug 1/1 passed, exit 0; Release 1/1 passed, exit 0.

### Static gates

```sh
./tools/validate-docs
```

Result: exit 0; 117 Markdown documents and `mkdocs.yml` navigation validated.

```sh
/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict \
  --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/site
```

Result: exit 0; strict site built successfully (1.27 s).

```sh
./tools/check-source-shape
```

Result: exit 0. It reported the existing warnings for
`tests/compositor/CMakeLists.txt` (500 nonblank lines) and
`tests/services/display_color_model/tst_color_model.cpp` (539 nonblank lines);
neither path is changed by this candidate.

```sh
git diff --check \
  74da46345c7a5094d45c756ad8b23ca87591fcd3..ce66a98dd4835048761142425253c72af89b77e2
git diff --name-only \
  74da46345c7a5094d45c756ad8b23ca87591fcd3..ce66a98dd4835048761142425253c72af89b77e2 \
  -- '*.json'
```

Result: both exit 0; `diff --check` was clean and the JSON query returned no
paths, so `python3 -m json.tool` was not applicable.

### Scratch focus reproduction build

```sh
/usr/bin/c++ -std=c++20 -g -fPIC -no-pie \
  -I src/apps/settings/appearance/include -I src/themes/include -I src/design_tokens/include \
  $(pkg-config --cflags Qt6Core Qt6Gui Qt6Qml Qt6Quick Qt6Test) \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro.cpp \
  -o /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/audio-focus-repro \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/apps/settings/appearance/libqindaqt_settings_appearance.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens/libqindaqt_tokens_qml.so \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/themes/libqindaqt_themes.a \
  /home/cabewse/work_SPaC3/builds/qindaqt/review-audioset-codex/debug/src/design_tokens/libqindaqt_design_tokens.a \
  $(pkg-config --libs Qt6QuickControls2 Qt6Quick Qt6Qml Qt6Gui Qt6Test Qt6Core)
```

Result: exit 0. Running it produced the P1 evidence above and intentionally
returned exit 1 because the candidate failed both focus assertions.

## Verdict

The exact candidate is rejected. The public-client boundary, lineage fencing,
hostile protocol validation, packaging, and all requested build/test/static
gates are otherwise sound, but the documented keyboard contract fails in two
reproducible ways and the required integrated Audio navigation/accessibility
negative control is absent.

VERDICT REJECT P0/P1/P2/P3=0/2/1/2
