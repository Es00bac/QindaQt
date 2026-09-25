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
most 8 redirects. It does not resume: a failed transfer is discarded.

## Proton builds

**Download.** The Proton manager lists releases from the GitHub releases
API for `GloriousEggroll/proton-ge-custom`. A release is offered only when it
has both `GE-Proton<N>-<M>-x86_64.tar.gz` and its `.sha512sum`. The build's
name is the tarball name without `.tar.gz` (for example
`GE-Proton11-6-x86_64`), the same directory name Portage's
`app-emulation/ge-proton-bin` installs. Installing:

1. refuses when `<root>/<name>` already exists (a build is never replaced);
2. downloads the tarball and checksum into a hidden
   `.qindalutris-staging-*` directory inside the target root;
3. verifies SHA-512 in slices, so the window stays responsive;
4. lists the archive with `tar --list` and refuses it unless every entry
   lies inside one top-level directory named `<name>` (no absolute paths,
   no `..`);
5. extracts it with `tar --extract --no-same-owner` into staging;
6. moves `<name>` into place with a no-replace atomic rename.

The target root defaults to `$XDG_DATA_HOME/Steam/compatibilitytools.d`
(normally `~/.local/share/Steam/compatibilitytools.d`). Any failure or
cancel removes the staging directory; nothing half-extracted is ever
visible.

**Remove.** Only builds directly inside the user root can be removed.
Removal is refused when any title is pinned to the build, when the root is
under a system directory (`/usr`, `/opt`, and the like — those builds
belong to Portage), or when the entry is a symbolic link. The build is first
renamed into `.qindalutris-trash/` (so it disappears at once) and then
deleted; leftovers from an interrupted removal are swept on the next run.

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
paths map to `<prefix>/drive_c/...`; Proton names the Windows user
`steamuser`.

## Launcher install job

1. **Preflight**: a pinned Proton build (a directory holding `proton`, never
   a floating alias), `umu-run`, a Vulkan driver, and the recipe's free-space
   floor. Each missing piece names the package that provides it.
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
afterwards; cancelling leaves the prefix in place.

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
| `SystemProbe` / `HostSystemProbe` | `install_preflight.h` | this package |
| `InstallerPlanner` (`InstallerPlanRequest` → `InstallerPlan`) | `installer_planning.h` | the umu launch-plan builder |
| pinned build names for removal | `ProtonRemovalRequest::pinnedBuildNames` | the TitleRecord store |
| `InstalledLauncher`, `SetupFileInstallResult` | job headers | consumed by the TitleRecord store |

Catalog scanners must ignore dot-directories in compatibility-tools roots,
because staging and trash directories live there while a job runs.

## Tests

`tests/apps/qindalutris/tst_download_allowlist.cpp`,
`tst_download_guard.cpp`, `tst_ge_proton_releases.cpp`,
`tst_proton_install_job.cpp`, `tst_proton_removal.cpp`,
`tst_process_runner.cpp`, `tst_store_recipes.cpp`,
`tst_launcher_install_job.cpp` and
`tst_setup_file_install_job.cpp` (label `jobs`). They use scripted fakes
(`job_fakes.h`) and never reach the network, umu, Wine or a vendor
installer; the Proton rows run the host's `tar` on archives they build
themselves in an isolated home.
