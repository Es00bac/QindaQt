# QindaLutris installs and Proton builds

This page describes the machinery that lets [QindaLutris](qindalutris.md)
install store launchers, run any Windows setup file, and download or remove
GE-Proton builds, as decided in
[ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md).
The code lives in `src/apps/qindalutris/core/jobs/` (the
`QindaQt::QindaLutrisJobs` library). It deliberately does not depend on the
library model, the Proton catalog or the umu launch planner: those are wired
in by the composition root through the seams listed at the end.

## What the user sees

Every flow is one button with a progress bar and a line of plain text
("Downloading the Battle.net installer", "Checking the download",
"Installing Battle.net. If a window opens, follow it to the end"). Every
job ends in **one plain sentence**, never a code or a path:

| Situation | Sentence the user sees |
|---|---|
| Not enough space | There is not enough free disk space for Battle.net: it needs at least 2.0 GB, and only 512.0 MB is free. Free up some space, then try again. |
| umu missing | The game runner umu is not installed. Install the games-util/umu-launcher package, then try again. |
| No Vulkan driver | No Vulkan graphics driver was found. Install your graphics card's Vulkan driver (…), then try again. |
| No pinned build | No Proton build is chosen for Battle.net. Choose one under Proton builds, or install the app-emulation/ge-proton-bin package. |
| Download failed | The Battle.net installer could not be downloaded. Check your internet connection and try again. |
| Installer failed | The Battle.net installer stopped with an error, and Battle.net was not installed. |
| Checksum mismatch | The downloaded Proton build did not pass its safety check, so it was not installed. Try again later. |
| Unsafe archive | The downloaded Proton build was not in the expected shape, so it was not installed. |
| Proton space | There is not enough free disk space to install GE-Proton11-6-x86_64: it needs at least 2.0 GB, and only 1.0 GB is free. Free up some space, then try again. |
| No atomic rename | The folder for Proton builds is on a disk that cannot install them safely. Keep your Steam folder on a Linux disk such as ext4 or btrfs. |
| tar failed | The Proton build could not be unpacked, so it was not installed. Try again, or copy the details for someone helping you. |
| Build in use | GE-Proton11-7-x86_64 is still used by at least one of your games, so it was not removed. Move those games to another Proton build first. |

Behind **Copy details**, each job keeps a bounded, timestamped log of every
stage (`detailsText()`): URLs, the umu command, exit codes and the first
lines of the installer's error output. It is meant to be pasted to someone
helping, so it never contains credentials or the whole environment.

## Download allowlist

QindaLutris downloads only over HTTPS and only from these exact hosts
(`download_allowlist.h`). Matching is exact — no suffixes or wildcards — so
`downloader.battle.net.evil.com`, URLs with user-info (`good@evil`), other
ports and plain `http` are all refused. Every redirect hop is re-checked
before it is followed.

| Purpose | Hosts |
|---|---|
| GE-Proton releases | `github.com`, `objects.githubusercontent.com`, `release-assets.githubusercontent.com`, `api.github.com` |
| Battle.net | `downloader.battle.net` |
| EA app | `origin-a.akamaihd.net` |
| Ubisoft Connect | `static3.cdn.ubi.com` |
| Epic Games Launcher | `launcher-public-service-prod06.ol.epicgames.com`, `epicgames-download1.akamaized.net` |
| GOG Galaxy | `webinstallers.gog-statics.com` |
| Amazon Games | `download.amazongames.com` |
| Compatibility database refresh | `compat-db.qindaqt.invalid` (placeholder until the host is decided) |

The production downloader writes to `<file>.part` and renames it into place
only after the transfer completes at the announced length. It enforces a
2 GiB size cap, a 60-second stall timeout, a 3-hour total timeout and at
most 8 redirects (its own count; Qt's limit is set one higher so the
guard answers first). Stalls, early disconnects, redirect loops and
insecure redirects each get their own reason in the details log. It does
not resume: a failed transfer is discarded. Its URL policy can be replaced
only through a test-only hook, so tests can run it against a local server.

## Stopping a job completely

Installers and `tar` run through `QProcessRunner`, which starts every
process inside its own transient systemd user scope
(`systemd-run --user --scope --quiet --collect --unit=qindalutris-job-<id>`).
Cancelling, or reaching a time limit, sends SIGTERM to the whole scope
(and to umu-run itself, which forwards it), waits up to 5 seconds, then
sends SIGKILL, and reports the job stopped only when the scope and every
tracked process are gone. This matters because umu starts the Steam
Runtime in a new session: killing umu-run alone would leave Wine and the
installer running. Without a user systemd manager the runner falls back to
the child's own process group plus tracking of its descendants in `/proc`;
a program that detaches itself before it is tracked can escape that
fallback, which the scope cannot. The downloaded installer, or Proton's
staging folder, is deleted only after the tree is confirmed gone; if it
cannot be confirmed, the file is kept and the details log says so. No
`PYTHON*` variable ever reaches a job process, because umu is written in
Python. The same helpers (`process_tree.h`) are meant for the game
launcher's Force quit ([ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md) §4b).

## Proton builds

**Download.** The Proton manager lists releases from the GitHub releases
API for `GloriousEggroll/proton-ge-custom`. A release is offered only when it
has both `GE-Proton<N>-<M>-x86_64.tar.gz` and its `.sha512sum`, named after
the release tag and served from exactly
`https://github.com/GloriousEggroll/proton-ge-custom/releases/download/<tag>/`;
the install job checks this again, so a fork or another host is never
used. The build's
name is the tarball name without `.tar.gz` (for example
`GE-Proton11-6-x86_64`), the same directory name Portage's
`app-emulation/ge-proton-bin` installs. Installing:

1. refuses when `<root>/<name>` already exists (a build is never replaced);
2. checks free space: four times the tarball size (a build unpacks to about
   2.9 times its download; 3 GiB when the size is unknown);
3. downloads the tarball and checksum into a hidden
   `.qindalutris-staging-*` directory inside the target root;
4. verifies SHA-512 in slices, so the window stays responsive;
5. lists the archive with `tar --list --verbose` and refuses it unless every
   entry lies inside one top-level folder named `<name>`: no absolute paths
   or `..`, no symbolic link pointing outside the folder, no hard link to
   outside it, no devices, FIFOs or setuid files, no name listed twice and
   nothing written through a link;
6. extracts it with `tar --extract --no-same-owner --no-same-permissions`
   into staging;
7. moves `<name>` into place with a no-replace atomic rename.

The target root defaults to `$XDG_DATA_HOME/Steam/compatibilitytools.d`
(normally `~/.local/share/Steam/compatibilitytools.d`). Any failure or
cancel removes the staging directory, read-only folders included, once
`tar` is confirmed stopped; nothing half-extracted is ever visible.

**Remove.** Only builds directly inside the user root can be removed.
Removal is refused when any title is pinned to the build, when the build
lies under a system directory such as `/usr` or `/opt` (compared both as
written and after resolving links — those builds belong to Portage), or
when the entry is a symbolic link. `/var` is not a system directory here,
because some systems keep home folders under `/var/home`. The build is
first renamed into `.qindalutris-trash/` (so it disappears at once) and
then deleted, read-only folders included. If anything cannot be deleted
the result says so plainly; leftovers are swept on a later run.

## Store launcher recipes

Each launcher is a fixed recipe (`store_recipes.cpp`). The installer
arguments are only those Lutris's own install scripts use; where none is
documented the vendor installer shows its own window.

| Recipe | Installer | Arguments | Prefix | umu STORE | Launcher detected at |
|---|---|---|---|---|---|
| `battlenet` | Battle.net-Setup.exe | — | `battlenet` | `battlenet` | `C:\Program Files (x86)\Battle.net\Battle.net Launcher.exe` |
| `ea` | EAappInstaller.exe | `EAX_LAUNCH_CLIENT=0` | `ea-app` | `ea` | `C:\Program Files\Electronic Arts\EA Desktop\EA Desktop\EALauncher.exe` (also a versioned folder) |
| `ubisoft` | UbisoftConnectInstaller.exe | `/S` | `ubisoft-connect` | `ubisoft` | `C:\Program Files (x86)\Ubisoft\Ubisoft Game Launcher\UbisoftConnect.exe` |
| `egs-launcher` | EpicGamesLauncherInstaller.msi (`msiexec /i`) | `/q` | `epic-games-store` | `egs` | `…\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe` |
| `gog-galaxy` | GOG_Galaxy_2.0.exe | — | `gog-galaxy` | `gog` | `C:\Program Files (x86)\GOG Galaxy\GalaxyClient.exe` |
| `amazon-games` | AmazonGamesSetup.exe | — | `amazon-games` | `amazon` | `C:\users\steamuser\AppData\Local\Amazon Games\App\Amazon Games.exe` |

Every launcher runs as umu id `umu-0` (umu-database has no entry for the
launchers themselves). Prefixes default to `~/Games/<prefix>`. Windows
paths on drive C: map to `<prefix>/drive_c/...` (other drive letters are
never guessed); Proton names the Windows user `steamuser`. Where a
launcher lives in a versioned folder (the EA app), the highest version
wins, compared numerically.

## Launcher install job

1. **Preflight**: a pinned Proton build (a directory holding `proton`, never
   a floating alias), `umu-run`, a Vulkan driver, and the recipe's free-space
   floor. Each missing piece names the package that provides it. A Vulkan
   driver counts only if its manifest names a 64-bit hardware driver:
   software renderers (lavapipe, SwiftShader) and 32-bit-only manifests are
   ignored.
2. **Adopt**: if the prefix already holds the launcher (for example an
   existing `~/Games/battlenet`), it is registered without reinstalling.
3. **Download** the vendor installer (allowlisted HTTPS only).
4. **Run** it through umu with the pinned build.
5. **Detect** the launcher: the first recipe candidate that exists.

Vendor installers often return odd exit codes, so the launcher's presence
decides: a non-zero code with the launcher present is a success with a
note. Proton keeps umu running until the prefix's programs exit, so an
installer that opens its launcher at the end finishes when the user closes
it (the progress text says so). The downloaded installer is deleted
afterwards; cancelling stops the whole installer tree first and leaves the
prefix in place. Every job reports its result from the event loop, never
from inside `start()` or `cancel()`.

## Setup-file install job

For any `.exe` or `.msi` the user picks: preflight, create the prefix,
record the executables already there, run the installer, and offer the new
executables for the user to confirm as the game. The scan looks under
`Program Files`, `Program Files (x86)`, `users/*/AppData/Local/Programs`,
`Games` and `GOG Games` inside `drive_c`, is bounded (50,000 entries,
depth 10) and never follows symbolic links. Uninstallers (`unins*`), setup
programs, crash reporters, redistributables and prerequisite installers are
dropped. The rest are ranked by how closely the file and folder names
match the title, then by size. The user's installer file is never deleted.

## Seams for wiring

| Seam | Header | Supplied by |
|---|---|---|
| `Downloader` / `NetworkDownloader` | `downloader.h`, `network_downloader.h` | this package |
| `ProcessRunner` / `QProcessRunner` (also `InstallerRunner`) | `process_runner.h` | this package |
| scope naming and whole-tree stop (`ProcessTreeStopper`) | `process_tree.h` | this package; reused by Force quit |
| `SystemProbe` / `HostSystemProbe` | `install_preflight.h` | this package |
| `InstallerPlanner` (`InstallerPlanRequest` → `InstallerPlan`) | `installer_planning.h` | the umu launch-plan builder |
| pinned build names for removal | `ProtonRemovalRequest::pinnedBuildNames` | the TitleRecord store |
| `InstalledLauncher`, `SetupFileInstallResult` | job headers | consumed by the TitleRecord store |

Catalog scanners must ignore dot-directories in compatibility-tools roots,
because staging and trash directories live there while a job runs.

## Tests

`tests/apps/qindalutris/tst_download_allowlist.cpp`,
`tst_download_guard.cpp`, `tst_network_downloader.cpp` (a local HTTP
server), `tst_ge_proton_releases.cpp`, `tst_archive_listing.cpp`,
`tst_proton_install_job.cpp`, `tst_proton_install_cancel.cpp`,
`tst_proton_removal.cpp`, `tst_process_runner.cpp` (process trees with
`setsid` grandchildren; the systemd scope row runs only where a user
manager answers), `tst_store_recipes.cpp`, `tst_install_preflight.cpp`,
`tst_launcher_install_job.cpp` and
`tst_setup_file_install_job.cpp` (label `jobs`). They use scripted fakes
(`job_fakes.h`, `proton_job_fixture.h`) and never reach the internet, umu, Wine or a vendor
installer; the Proton rows run the host's `tar` on archives they build
themselves in an isolated home.
