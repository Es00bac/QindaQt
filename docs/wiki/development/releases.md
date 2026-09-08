# Release procedure

QindaQt releases are exact native-desktop snapshots. A passing preview or a
plugin-disabled build is useful development evidence, but it does not qualify
the shipped KWin plugin.

## Prepare the candidate

Work from a clean, reviewable commit. Confirm that the source manifest and
empty or rebased patch series describe the intended KWin state:

```sh
./compositor/tools/verify-kwin-source
./compositor/tools/verify-kwin-source --check-remote
./tools/check-release-contract
```

The remote check resolves the annotated tag and its peeled commit. Do not copy
an object ID from prose or infer it from the installed package. When changing
KWin, follow the [KWin upgrade procedure](kwin-upgrades.md) first.

Update the full-desktop ebuild's immutable QindaQt commit and package version
together. Regenerate its Manifest from the exact source archive and run a
Portage metadata check. Keep the applications-only package available for users
who do not install the desktop; the full package owns the same application
files and declares the replacement blocker.

## Build and qualify

Use an exact release-matched KWin development package. A fresh build root
prevents a previous plugin from satisfying discovery accidentally:

```sh
cmake -S . -B build/release-checkpoint -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
cmake --build build/release-checkpoint --parallel "$(nproc)" \
  --target src/all qindaqt-session-probe
cmake --install build/release-checkpoint \
  --prefix "$PWD/build/release-checkpoint-stage"
./tools/check-release-contract \
  --kwin-wayland /usr/bin/kwin_wayland \
  --kwin-cmake-version /usr/lib64/cmake/KWin/KWinConfigVersion.cmake \
  --build-root build/release-checkpoint \
  --install-root build/release-checkpoint-stage
```

Adjust only the KWin CMake version-file path for the distribution layout. The
checker requires the runtime and development package to report the exact same
release and requires non-empty plugin and session-launcher artifacts. It
requires stdout to contain only the exact `kwin VERSION` line while allowing
restricted containers to emit their realtime-scheduling diagnostic on stderr;
a missing, duplicate, or mismatched stdout line remains a hard failure.

Run the static ABI rows, then acquire the shared nested-test slot described in
the [testing harness](testing-harness.md). Execute nested rows one at a time:

```sh
ctest --test-dir build/release-checkpoint \
  -R '^(compositor\.kwin-abi-pin|compositor\.rejects-conflicting-kwin-abi|compositor\.kwin-plugin-dependency-contract)$' \
  --output-on-failure --no-tests=error
ctest --test-dir build/release-checkpoint \
  -R '^compositor\.kwin-plugin-nested$' \
  --output-on-failure --no-tests=error
ctest --test-dir build/release-checkpoint \
  -R '^session\.installed-plugin-discovery$' \
  --output-on-failure --no-tests=error
```

The first nested row must observe the plugin's live service. The second stages
an install and proves that the installed launcher discovers the relocated
plugin. Also run the focused tests for every changed module and the repository
documentation gates.

The native CI lane uses Gentoo's generic `desktop/systemd` profile and the
official KWin 6.6.6 binary with `shortcuts` enabled and `lock` disabled. That
lane qualifies the binary plugin ABI, complete source build, staged install,
and two private nested boots. Keeping lock-screen runtime integration out of
this build lane avoids pulling Plasma Workspace and Plasma login sessions into
a desktop that installs its own session entry. It does not reduce the package
contract: the full-desktop ebuild still requires release-matched KScreenLocker,
KWin `lock,shortcuts`, portals, hardware providers, and session utilities.
Before the two serialized native boot checks, CI creates its container-private
`/tmp/.X11-unix` directory with mode `1777`. Minimal container images may omit
this standard X11 socket path; both checks still require working XWayland.

For no-lock test sessions, `qindaqt-wm` inspects the selected KWin executable's
help and omits `--no-lockscreen` when the complete screen-locker feature was
compiled out; build-tree and installed-discovery boots therefore exercise the
same launcher without passing an option that this native binary cannot parse.

When the dated stage image and pinned Portage snapshot differ, the CI install
uses `--update --newuse` with `--usepkgonly`. This permits a compatible official
binary to replace a base package whose selected USE flags changed, while still
failing instead of compiling a missing binary package from source.
The lane also selects NetworkManager's client-library binary without its
Bluetooth, policykit, or systemd daemon integrations. QindaQt needs `libnm` to
compile in this lane; the full desktop package retains the resident provider
and hardware integrations. WirePlumber's PipeWire graph reaches `xdg-utils`
through libcanberra, PulseAudio, GTK, and CUPS. None of its desktop handlers
participate in the native build or nested boots, so the lane selects the
official headless `xdg-utils` binary explicitly. This avoids a KDE CLI tools
tail while retaining `--binpkg-respect-use=y` as a hard gate.

Both Arch jobs keep `QINDAQT_ENABLE_AUDIO_LIVE_RUNTIME_TESTS=ON` and install the
PipeWire daemon and CLI, WirePlumber daemon, and `pw-cat`. Gentoo's official
binhost does not provide the PipeWire `extra` variant that contains `pw-cat`,
so the native plugin lane sets this option `OFF`. The audio service and its unit
test still compile there. Configuration-contract sub-builds inherit the same
live-runtime selection, so they do not re-enable unavailable process-launching
audio rows. The complete production tree, static KWin ABI rows, staged install,
live plugin service boot, and installed-plugin discovery boot remain mandatory.

## Record and publish

Record the exact QindaQt commit, KWin package version, commands, exit statuses,
test counts, and unavailable hardware coverage. A workflow definition is not a
passing CI run: record the GitHub run URL and conclusion only after the remote
native lane completes. Obtain independent review of the exact candidate commit.

Before publishing, build the full Gentoo package in a clean Portage image,
inspect its file list, and confirm that File Manager, Text Editor, Terminal,
the production shell, services, session launcher, KDecoration, and KWin plugin
are present. Tagging and installation happen only after these gates and project
authorization. Keep the prior source commit and binary package available until
the installed-session smoke completes.
