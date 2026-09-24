# Settings install and login checkpoint

This checkpoint audits the Settings, Audio1, and Network1 install boundary and
stages the current source tree beneath the ignored `build/` directory. Its
default commands only read the live session or write the isolated stage. They
do not install files, stop or restart services, change a route or radio, or log
out.

The helper owns no product runtime behavior. It inspects the CMake install
outputs and the Settings route registry, then writes a manifest with the exact
source commit, protocol versions, executable hashes, unit and desktop-entry
paths, observed user state, and a rollback recipe. The runtime contracts remain
owned by [Settings Center](../apps/settings-center.md), [Audio1](../architecture/audio-service.md),
[Network1](../architecture/network-service.md), and the
[login supervisor](../architecture/session-autostart.md).

## Read-only preflight

From the repository root, run:

```sh
python3 tools/install-checkpoint/checkpoint.py preflight \
  > build/install-checkpoint/preflight.json
```

The report includes the current `git` commit and dirty paths, the installed
Portage package and rollback source archive, CMake's expected output, current
Settings desktop entries, Settings route IDs, user units, matching processes,
and relevant saved-state paths. For each live service, it calls `GetNameOwner`
on the session bus and then calls read-only `GetSnapshot` on that already-owned
unique name with D-Bus auto-start disabled. It retains only the protocol
version from the response; it does not print device, SSID, or profile values.
The systemd queries use `show` only. The preflight never calls NetworkManager,
`ip route`, `systemctl start`, `stop`, `restart`, or `daemon-reload`.

## Build host and deployment target

The isolated source build and staged-install proof for this checkpoint run on
`qinda`. The eventual desktop install and logout/login qualification target is
`qinda-top`. The tool derives its checkout and ignored build paths from its own
location, resolves the Portage overlay from the installed package's recorded
repository identity, and reads user state from the current process environment;
it contains no qinda-specific checkout or home-directory path.

Every preflight report and stage manifest includes `executionHost`. Its
`installedPackage`, service/process, saved-state, and rollback sections describe
only the machine that ran the command. In particular, a manifest produced on
`qinda` is build-host evidence and is not a preflight or rollback recipe for
`qinda-top`. The generated capture and rollback scripts also use paths and
artifacts from the host that produced them; do not transfer or run qinda's
scripts on qinda-top.

The manager-controlled deployment package must be built by Portage on
`qinda-top` from the exact integrated source archive, then use a fresh
preflight and target-local rollback inputs there before any package install.
This worker runs the containment stage on `qinda` as a packaging diagnostic;
its stage output is not transferable release proof. The source archive and
recorded source hash can move between hosts, but staged binaries,
host-specific manifest paths, and capture/rollback scripts must be rebuilt or
regenerated on `qinda-top`. Each generated rollback script checks its bound
execution host before mutation. A rollback recipe produced on `qinda` must
never restore `qinda-top`. This worker only builds and stages on `qinda` and
does not install or restart a live package or service on either host.

Saved-state output contains path, kind, mode, size or entry count, and SHA-256
for files. It does not copy or print file contents. The future capture script
archives only the Settings1 file, Audio1 console/VBAN/macro/preset state, and
QindaQt user unit drop-ins. It excludes NetworkManager profiles and
credentials.

## Build and stage

The stage command configures a Release tree, builds the production install
targets, and stages only below the ignored root
`build/install-checkpoint/stage`:

```sh
python3 tools/install-checkpoint/checkpoint.py stage
```

The CMake options match the QindaQt desktop package's shell, plugin, and Viewer
selection; the stage disables the separately packaged System Monitor and OBS
bridge. `BUILD_TESTING=OFF` keeps test executables out of this install build;
the focused checkpoint test module runs separately. The stage sets
`QINDAQT_ENABLE_STRICT_WARNINGS=OFF` to match the desktop ebuild. Configure,
production build, and install outputs are retained under
`build/install-checkpoint/logs/` and named in the stage manifest. The helper
reads the configured job and load limits from
`portageq envvar MAKEOPTS` and passes those exact tokens to `cmake --build`; it
does not set or rewrite `MAKEOPTS`. The CMake install prefix remains `/usr` so
generated unit and D-Bus activation files retain their production paths. The
helper audits every generated `cmake_install.cmake` before the build, then
checks the CMake cache still has prefix `/usr` and repeats the audit after the
build immediately before `cmake --install` with
`DESTDIR=build/install-checkpoint/stage`. This catches install-script
regeneration or prefix changes during the build. CMake
maps both `/usr/...` and absolute destinations such as `/etc/xdg/autostart`
beneath that ignored root. The stage manifest records the generated script and
rule counts, literal absolute destinations and their mapped stage paths,
install-manifest paths, staged tree entry count, and a summary of other
generated side effects. The audit allows generated `file(WRITE)` outputs only
under the ignored build tree, RPATH operations and `/usr/bin/strip` only on
DESTDIR-rooted outputs, and includes only for build-local generated CMake
scripts that are themselves audited. Missing optional includes are accepted
only beneath the build tree. It rejects unknown generated commands, custom
writes or processes outside the build tree, changes to `DESTDIR` or `/usr`,
unsafe list mutations or manifest names, external install scripts, unknown
variables, and destinations that cannot map beneath the stage root. The helper
checks every installed manifest path and staged tree path after the install.
If the stage root already exists, the helper refuses to overwrite it. The
stage action is gated on review of the containment code and focused test
evidence in the assigned work queue.

The stage gate requires these executables:

| Purpose | Staged executable |
| --- | --- |
| Settings UI and Settings1 service | `stage/usr/bin/qindaqt-settings`, `stage/usr/bin/qindaqt-settings-service` |
| Audio1 and Network1 services | `stage/usr/bin/qindaqt-audio-service`, `stage/usr/bin/qindaqt-network-service` |
| Next login | `stage/usr/bin/qindaqt-wm`, `stage/usr/bin/qindaqt-session`, `stage/usr/bin/qindaqt-shell` |

It also checks both systemd user units, Audio1/Network1/Settings1 D-Bus
activation files, `org.qindaqt.Settings.desktop`, and the QindaQt Wayland
session entry. The session entry must contain `Exec=/usr/bin/qindaqt-wm --drm`
and `TryExec=/usr/bin/qindaqt-wm`, and the staged executable must be present
and executable. These paths are recorded under `stage/usr/...`; the absolute
OBS autostart rule is contained at `stage/etc/xdg/autostart/`. The generated
`stage-manifest.json` records each executable and unit hash so a later
installer can verify the exact staged bytes.

Focused script tests are:

```sh
python3 -m unittest tests.tools.test_install_checkpoint -v
```

The focused suite configures temporary CMake projects and audits their actual
generated install scripts without running `cmake --install`. It verifies an
absolute `/etc/xdg/autostart` destination maps beneath DESTDIR, generated
shared-library RPATH operations target DESTDIR, and the live autostart file
remains unchanged. Negative probes cover `install(CODE)` file writes and
processes aimed at live `/etc`, attempts to clear DESTDIR or mutate `/usr`,
build-local and external `install(SCRIPT)` includes, and include helpers that
write outside the build tree. Missing optional includes are tested both inside
and outside the build root. Stage-order tests verify the production build is
followed by a prefix check and fresh audit before the install invocation, and
that a changed prefix aborts before audit or install. The stage
invocation test also checks that installation uses `DESTDIR` and does not pass
`--prefix`.

## Current audit evidence

On build host `qinda`, at base commit `864c9ce5d3df572a009a1661702dc09e9346ae59`
on 2026-09-23, source
declares Settings1 wire schema 1 and settings schema 2, Audio1 schema 12, and
Network1 protocol 1. The installed package was
`gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2`, built from source commit
`8a86b74877c2fadbd0b3d4c8b8c03100db374642`. A read-only query of the running
service snapshots found Audio1 schema 11 and Network1 protocol 1. Audio1 must
therefore be replaced and restarted before this source's Audio Settings route
can be qualified against the resident service.

Both Audio1 and Network1 user units were active at the audit, although their
unit-file state was `disabled`; D-Bus activation is the relevant startup path.
The Audio1 unit had the user drop-ins `10-console-store.conf` and
`20-bus-socket.conf`, including its shared-bus `PrivateTmp=false` override.
The rollback capture preserves those files. The Settings desktop entry starts
`qindaqt-settings` and has Appearance and Display desktop actions. Audio and
Network are in-app Settings routes, IDs `audio` and `network`, rather than
desktop actions. The login entry is `qindaqt.desktop`, which starts
`qindaqt-wm --drm`; a new login is required to load the updated shell/session
binaries.

The Settings1 file is `$XDG_CONFIG_HOME/qindaqt/settings-v2.json`; Audio1
state uses `audio-console.json`, `audio-vban.json`, `audio-macros.json`, and
`audio-presets` under `$XDG_CONFIG_HOME/qindaqt`. Existing files remain in
place during a package install. The rollback capture is available if a live
Audio1 update changes state that must be restored with the old package.
NetworkManager owns saved network profiles; this checkpoint does not read,
copy, or modify them.

## Later bounded install and rollback

The Settings repair slices listed as integrated in `docs/HANDOFF.md` and
`ops/team/queues/first-party.md` are already present in this worktree's source
base. The install-checkpoint candidate itself still requires independent
review and integration. Live
qualification stays blocked until that exact candidate is integrated, its
stage and documentation gates pass on the integrated tree, and the manager
rechecks the source/runtime versions immediately before install.

After the user is untethered and the manager has selected a bounded install
window on `qinda-top`, the target-local recipe is:

1. Stop only the services shown active by the fresh preflight.
2. Run `build/install-checkpoint/capture-live-state.sh
   COPY-QINDAQT-USER-STATE` to preserve Settings1/Audio1 files and user unit
   drop-ins. The script is generated but is not run by `preflight` or `stage`.
3. Install the reviewed package, then run `systemctl --user daemon-reload` and
   restart the services that were active before install.
4. Check the new Audio1 snapshot reports schema 12 and Network1 reports
   protocol 1. Compare installed executable SHA-256 values with the
   `qinda-top`-local Portage package evidence; the qinda stage manifest is not
   deployment proof. Open Settings with `qindaqt-settings --page audio` and
   `qindaqt-settings --page network`; verify each route loads and shows current
   service status without invoking Connect, Scan, or radio controls.
5. Log out and back in, then repeat the version and route checks against the
   new session. This proves the login path adopted the installed shell and
   session binaries. It does not prove physical speaker output or Wi-Fi
   hardware behavior.

The host-local stage generates `rollback-live-install.sh` using the exact
previously installed Portage atom qualified with its VDB repository name, plus
SHA-256 hashes of that repository ebuild and retained source archive. The
script also verifies the captured state
archive and its host identity before mutation. The `qinda` script is only
qinda-local evidence and must never be run on `qinda-top`; target rollback
inputs and scripts must be generated and reviewed on `qinda-top` itself. A
target-local rollback requires `RESTORE-QINDAQT-DESKTOP`, re-emerges that exact
package while services remain active, then stops the listed services only for
state restore and restart. An exit trap attempts to reload the user manager
and restart the previously active services if restore fails. It ends by
requiring logout/login so already loaded session binaries are replaced. Do not
remove the prior ebuild or source archive until install and login qualification
is accepted.

The first stage attempt stopped during production compilation before
`cmake --install`; its warning-policy mismatch is corrected in the current
candidate, but the production stage has not been rerun while manager scheduling
and fresh-clearance gates are active. No package installation, service
restart, route interaction, or logout/login was performed. The evidence here
is a read-only system audit, generated-script containment checks, and focused
checkpoint tests; no live Settings route, radio, network route, speaker, or
post-login claim is made.
