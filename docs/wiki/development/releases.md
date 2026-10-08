# Release procedure

QindaQt releases are exact native-desktop snapshots. A passing preview or a
plugin-disabled build is useful development evidence, but it does not qualify
the shipped KWin plugin.

## Prepare the candidate

Work from a clean, reviewable commit. Confirm that the source manifest pins the
intended qindaqt-kwin fork release (ADR-0291):

```sh
./compositor/tools/verify-kwin-source
./compositor/tools/verify-kwin-source --check-remote
./tools/check-release-contract \
  --desktop-ebuild <QindaGentoo>/gui-wm/qindaqt-desktop/qindaqt-desktop-<version>.ebuild
```

The remote check resolves the annotated tag and its peeled commit. Do not copy
an object ID from prose or infer it from the installed package. When changing
KWin, follow the [KWin upgrade procedure](kwin-upgrades.md) first.

Update the full-desktop ebuild's immutable QindaQt commit and package version
together. Regenerate its Manifest from the exact source archive and run a
Portage metadata check. Keep the applications-only package available for users
who do not install the desktop; the full package owns the same application
files and declares the replacement blocker.

The full-desktop release contract requires `sys-auth/qindaqt-lock-pam-1` or
newer for the fixed native lock authentication service. It also requires
`QINDAQT_NATIVE_POWER_EXCLUSIVE=OFF` under accepted
[ADR-0345](../adr/0345-hold-physical-native-power-cutover-until-receipts-pass.md).
Power1, UPower and the selected power-profile provider remain required; OFF
does not retire native service delivery or prove full idle/suspend parity.
Re-enabling exclusive mode requires the ADR's physical receipt qualification
and a corresponding reviewed release-contract update.


### October 7 prepared AI usage delivery: r16

Desktop revision 0.1.0_pre20261002-r16 freezes source
2188d8e0e339ce4b56acb841a4b570f58a3002cc, including the independently
accepted AI coding-agent usage applet, QtCore report publisher, public SDK and
all eleven stock-profile placements. It retains ED-01–03 repairs, the Network
disk-fallback installation contract, exact fork r6 ABI 6.6.6.1, lock-PAM >=1 and
accepted native power exclusivity OFF. The publisher uses existing QtCore;
it introduces no Python runtime dependency.

Its reproducible QindaQt-COMMIT/ archive contains 9,431 verified tracked blobs,
39,011,614 bytes, SHA256
6b8eab5eebb2859cadf4c5be0b45486c64d967e473072ca799dfa60d6cfbbe94.
Mirrored recipes and the new Manifest row preserve every historical recipe and
distfile record. Immutable r15 is held unchanged and does not contain this
applet. Source/recipe review, private signed full-source artifact review and
Portage-installed session adoption are separate gates. Preparation does not
claim an installed plugin or completion of the remaining ED-01–24 journeys.

### October 8 urgent core repair delivery: r17

Desktop revision 0.1.0_pre20261002-r17 freezes the manager-qualified runtime
5b7f5b5910e262a4bf2be2326bc92fc97af4e2ac. It contains the accepted Audio graph
channel projection/observer diagnostics repair and the existing public
audio_console.h FILE_SET export, together with the reviewed Printing/Copier
cohort already in that exact source. It retains compiled AgentUsage, its
QtCore publisher/public SDK/all eleven profiles, Network fallback, fork r6,
lock-PAM >=1 and native power exclusivity OFF. Runtime/build dependency
declarations and whole-tree install are identical to r16.

The reproducible git archive plus gzip-n distribution contains 9,626 tracked
blobs (9,594 regular, 32 executable), 39,276,265 bytes, SHA256
bbb3bc4715d2dd4ac5df5f47648f8d6ab2a60d79dae0b1b558f7be3a379e2709.
Git blob contents and git-archive default tar modes are verified against the
exact freeze; two independent cuts match. The new recipe and distfile record
mirror exactly, preserving each repository's historical Manifest records.
Inherited older Manifest inventories differ and are not rewritten.

Manager integrated Audio 24 CTest/221 Qt, real public SDK positive/required-header
poison/restoration and Printing/Copier 26 CTest/157 Qt evidence qualified that
runtime freeze. Recipe review, private signed full-source artifact/image
verification, exact artifact review and later Portage-installed session
controls remain separate gates. Preparation does not prove installed volume
or speaker controls. Unaccepted Clipboard/Bluetooth/Power work is deferred
to a later revision. Immutable r16 recipe/archive/artifact stays unchanged;
it remains the rollback package boundary.

### October 8 prepared core usability delivery: r18

Desktop revision 0.1.0_pre20261002-r18 pins candidate runtime
5858bccdf82808a74a6bab358d8e6f08c7ac256a. It packages the reviewed compact,
screen-bounded Audio popup and denser Settings presentation, the public
Bluetooth radio helper and sender-preserving SDK, authenticated Clipboard
startup and replaced-Power-owner retirement fixes already in that source.
The manager qualified this exact runtime with strict28-target build0,
33 owning CTests/185 Qt cases and four profile/resolution CTests/88 Qt cases:
37/37 checks and273 Qt passes, zero failure/skip/blacklist. Raw manager proof
is retained in ignored .cache/manager-compact-audio-integrated-20261008.
These are source gates; signed package/image and installed adoption remain open.

The helper and its static public client directly consume libdbus. This recipe
declares sys-apps/dbus explicitly in both DEPEND and RDEPEND. Gentoo packages
libdbus headers, dbus-1.pc and the shared library with the broker in this atom;
dev-libs/dbus does not exist. The original r18 resolver failure is retained
as evidence, and the repaired recipe requires actual pretend resolution. It preserves all other r17 dependency
declarations, exact fork r6 ABI6.6.6.1, lock-PAM>=1, native power exclusivity OFF,
whole-tree install and the compiled AgentUsage publisher/SDK/all eleven profiles.
Historical r16/r17 recipes, distfiles and rollback artifacts stay immutable.

The reproducible Git archive plus gzip-n contains 9,747 tracked blobs,
39,433,447 bytes, SHA256
620ad2fcfb7c006b55fa42b0f5e54a856ab888f29fa199b0c7072e8a0962f323.
Independent cuts and every Git blob, executable mode and symbolic-link target
are checked. Each Manifest retains its old rows and appends only the matching
r18 DIST row; the new recipe and DIST row mirror exactly.

After independent source/recipe review and a separate compiler lease, build
only through Portage into a private signed gpkg/image using the configured
MAKEOPTS. Inspect source identity, actual signer and full image closure:
Audio public headers/archive and installed-only consumer with named-header
poison/restoration; Bluetooth radio client headers/archive and libdbus-linked
consumer with both required-header poisons; helper executable, systemd unit and
D-Bus activation descriptor, including their reviewed namespace/device fences
and no direct-activation fallback. Preserve the existing compiled AgentUsage
SDK/publisher/manifest/profile/QML checks, seven-component Network compiled/disk
fallback checks, and exact compositor/plugin ABI. Do not activate a real helper,
write rfkill, contact host buses or infer hardware success from image presence.
Actual artifact review and later authorized Portage installation/session
adoption remain separate gates; no source preparation restarts the desktop.

## Build and qualify

Use the exact qindaqt-kwin development package the manifest pins. A fresh build root
prevents a previous plugin from satisfying discovery accidentally:

```sh
cmake -S . -B build/release-checkpoint -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON \
  -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_BUILD_VIEWER=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF
qq_makeopts="$(portageq envvar MAKEOPTS)"
cmake --build build/release-checkpoint \
  --target src/all qindaqt-session-probe -- ${qq_makeopts}
cmake --install build/release-checkpoint \
  --prefix "$PWD/build/release-checkpoint-stage"
./tools/check-release-contract \
  --compositor /usr/bin/qindaqt-kwin \
  --compositor-cmake-version /usr/lib64/cmake/QindaQtKWin/QindaQtKWinConfigVersion.cmake \
  --build-root build/release-checkpoint \
  --install-root build/release-checkpoint-stage
```

The direct release build must use the machine's configured Portage job and
load limits exactly. Do not replace `MAKEOPTS` with a guessed `--parallel`
value or a command-local override; if a demonstrated failure requires different
limits, obtain the operator's explicit approval first.

Adjust only the fork's CMake version-file path for the distribution layout. The
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
an install with `DESTDIR` and prefix `/`, including absolute destinations
such as `/etc/xdg/autostart`, and proves that the installed launcher discovers
the relocated plugin without `--plugin-root`. The `session.installed-plugin-stage`
fixture separately checks absolute/relative path containment without a compositor. Also run the focused tests for every changed module and the repository
documentation gates.

The historical native CI lane uses Gentoo's generic `desktop/systemd` profile and the
official KWin 6.6.6 binary with `shortcuts` enabled and `lock` disabled. That
lane qualifies the binary plugin ABI, configured source build, staged install,
and two private nested boots. Keeping lock-screen runtime integration out of
this build lane avoids pulling Plasma Workspace and Plasma login sessions into
a desktop that installs its own session entry. That stock-KWin lane does not
qualify the Plasma-free delivery. Its package uses the exact `gui-wm/qindaqt-kwin`
fork with native lock and shortcuts, the QindaQt portal providers, native power
service and generic desktop profile. Production builds keep both private
compositor authorization options disabled. Qualify its plugin and installed
launcher against this fork; require the final dependency closure to contain
no retired Plasma runtime packages.
Hosted CI explicitly disables Viewer in its current reduced build matrix.
[QindaTK source](https://github.com/Es00bac/QindaTK) is publicly available;
publishing it does not expand that CI coverage. Full desktop releases require
`QINDAQT_BUILD_VIEWER=ON`, installed
QindaTK/Poppler Qt 6/image-format plugins, and the four `apps.viewer.*` gates.
QindaMPV is a separate post-dependency because it builds against the desktop's
installed AppShell libraries. The packaged defaults reference its existing
`org.qindaqt.QQMpv.desktop` entry.
The companion `media-video/qqmpv` checkpoint recipe lives in this repository's
Gentoo packaging tree. Its fetch-restricted archive is generated from the
exact QindaMPV commit named in the recipe, with the matching
`QindaMPV-COMMIT/` prefix and `gzip -n`; its Manifest verifies those bytes.
This avoids depending on a developer-specific checkout path. The desktop
and player archives must both be available before a package transaction.

### Public source repositories

The [ecosystem catalog](https://github.com/Es00bac/QindaQt#related-projects)
links the desktop, toolkit, applications, Gentoo overlay, screensavers, themes
and games. The September 19 publication compared the committed tips from both
development machines and fast-forwarded the existing GitHub default branches.
Uncommitted source is preserved separately on dated `snapshot/qinda-20260919`
and `snapshot/qinda-top-20260919` branches, with `SOURCE_SNAPSHOT.md` identifying
the source and qualification limits. Such a checkpoint is not an accepted
candidate, completed milestone, tested release or package installation.

Public GitHub publication and the existing package-delivery hub have separate
roles. Publishing these sources does not change the `qinda-sync` contract,
override package pins, update either working tree or restart a live session.

### Sharing a delivery between qinda and qinda-top

[ADR-0219](../adr/0219-share-completed-work-through-qinda.md) keeps qinda's
existing bare Git repositories, QindaGentoo overlay and binary server as the
shared source. Run the installed `qinda-sync` command as the normal user:

```sh
qinda-sync code /path/to/clean/project
qinda-sync publish /path/to/exact-package.tar.gz
qinda-sync packages --prepare-only
qinda-sync
```

Publish the source commit and the matching overlay recipe/Manifest before its
archive. QindaGentoo's committed `metadata/qinda-delivery` names the exact
versions to install on either machine. The last command fetches missing inputs
and uses Portage, including the existing binary server and host build settings;
already installed versions are skipped. `qinda-sync code` exchanges only the
current clean branch and stops on divergence. Other active worktrees and local
changes stay with their owners. Updates are invoked explicitly; no timer,
world update or desktop restart is part of this workflow.

The September 27 long-session repair pairs the desktop with Gabbee
`0.1.0_p20260927` or newer. Recovery drop-ins inherit the physical supervisor's
fresh user-manager environment; an old `EnvironmentFile` must not override a
new login. The installed timer covers Voice1, Audio1, Clipboard1, Display1,
Network1, Bluetooth1, Power1, Settings1 and the appearance portal. Missing-owner
and systemd crash recovery is bounded and reports locally to qinda's maintenance
inbox (over SSH from another host). This is not hung-GUI detection.
Verification for this repair covers installed
unit templates, available names and delivery queues on **both** hosts. A
representative controlled resident crash returned a new owner and a reported
incident on each host without ending the desktop. The handoff records the
laptop Clipboard circuit explicitly unblocked after its source repair.

Shell replacements can load the new presentation code without ending ordinary
applications. A running KWin process still retains its loaded compositor binary;
record that distinction and qualify a later physical login separately. Private
native boot and repaint tests protect the session boundary while keeping the
operator's current applications intact. The [retained-dock contract](../adr/0278-retain-dock-delegates-by-presentation-identity.md)
and current [handoff](https://github.com/Es00bac/QindaQt/blob/main/docs/HANDOFF.md) describe this repair's evidence.

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
