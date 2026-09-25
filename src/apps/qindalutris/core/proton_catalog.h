// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the Proton catalog of ADR-0275 section 2 -- which Proton
// builds are installed, and the facts about each. Pin identity and
// resolution live in proton_pin.h; this file never chooses a build. Pure
// over injected roots: tests hand it QTemporaryDir trees, production hands
// it defaultProtonRoots(). Reads are shallow, fixed-name and bounded; a
// hostile or corrupt root yields fewer builds, never an error. Callers own
// the returned values; nothing here caches.

struct ProtonBuild final {
  enum class Origin {
    System, // /usr/share/steam/compatibilitytools.d -- Portage owns it
    User,   // a user compatibilitytools.d -- the app may remove it
    Steam,  // Valve Proton under a Steam library's steamapps/common
  };

  QString name;        // directory name
  QString displayName; // compatibilitytool.vdf "display_name", else name
  QString versionText; // first line of the `version` file; may be empty
  QString path;        // absolute build directory (the PROTONPATH value)
  Origin origin = Origin::User;
  bool removable = false; // true only for Origin::User
  // False for a Steam rolling channel (isRollingProtonChannel) and for a
  // build without a readable `version` file: identity is name + version
  // (ADR-0275 section 2), so neither can be pinned.
  bool pinnable = true;

  friend bool operator==(const ProtonBuild &, const ProtonBuild &) = default;
};

[[nodiscard]] QString protonOriginId(ProtonBuild::Origin origin); // "system" | "user" | "steam"

// One directory that may hold builds. For Origin::Steam the path is a
// library's steamapps/common and only children named "Proton*" count.
struct ProtonRoot final {
  QString path;
  ProtonBuild::Origin origin = ProtonBuild::Origin::User;

  friend bool operator==(const ProtonRoot &, const ProtonRoot &) = default;
};

inline constexpr int kMaxProtonRoots = 16;
// Directory entries READ per root (every entry counts, matching or not), and
// candidate build directories kept per root (only eligible names count, so
// a Steam library with hundreds of games before "Proton*" still yields it).
inline constexpr int kMaxProtonEntriesScannedPerRoot = 4096;
inline constexpr int kMaxProtonDirsPerRoot = 128;
// Builds returned, applied AFTER sorting so the cap drops the oldest-looking
// builds of the last origin, never an arbitrary directory-order subset.
inline constexpr int kMaxProtonBuilds = 64;
inline constexpr int kMaxProtonBuildNameChars = 128;

// The directory Portage installs pinned builds into
// (app-emulation/ge-proton-bin, one slot per release).
inline constexpr QLatin1StringView kSystemProtonRoot{
    "/usr/share/steam/compatibilitytools.d"};

// The scanned roots, in precedence order:
//   System: /usr/share/steam/compatibilitytools.d
//   User:   $XDG_DATA_HOME/Steam/compatibilitytools.d (xdgDataHome empty =>
//           home/.local/share), home/.steam/root/compatibilitytools.d,
//           and the Flatpak Steam's
//           home/.var/app/com.valvesoftware.Steam/.local/share/Steam/… and
//           home/.var/app/com.valvesoftware.Steam/data/Steam/…
//           compatibilitytools.d
//   Steam:  <library>/steamapps/common for each of steamLibraryRoots, then
//           for the two Flatpak Steam roots above (duplicates collapse).
// Capped at kMaxProtonRoots. Pure: nothing is statted.
[[nodiscard]] QVector<ProtonRoot> defaultProtonRoots(
    const QString &home, const QString &xdgDataHome,
    const QStringList &steamLibraryRoots);

// Every usable build under the roots. A build is a directory holding an
// executable regular `proton` file. Roots are de-duplicated by canonical
// path. Builds of the same name in different roots are ALL kept, each with
// its own origin and path, so an exact-path pin to a shadowed copy still
// resolves (proton_pin.h prefers System when name and version agree). The
// result is sorted by origin (System, User, Steam), then by name newest-
// looking first (natural order, descending: GE-Proton11-6 before
// GE-Proton10-25), then by path; then capped at kMaxProtonBuilds.
// AGENT-GUARD: symlinked build directories and symlinked `proton` scripts
// are SKIPPED. A link such as compatibilitytools.d/GE-Proton -> GE-Proton11-7
// can be retargeted by anyone, which is the silent build move this catalog
// exists to prevent. For the same reason a directory whose NAME is a
// floating alias ("GE-Proton") is skipped. Roots themselves may be symlinks
// (~/.steam/root usually is) and are resolved.
// AGENT-CONTRACT: every directory whose name starts with '.' is skipped in
// every root. The GE-Proton download jobs (feat/qindalutris-jobs) stage and
// retire builds in hidden `.qindalutris-staging-*` and `.qindalutris-trash/`
// directories inside a compatibilitytools.d while they run; a half-extracted
// build must never be listed or pinned. A finished download is renamed to
// the release tarball name without `.tar.gz` (e.g. GE-Proton11-6-x86_64),
// the same directory name Portage's ge-proton-bin slots use, so a pin made
// against either is the same name.
[[nodiscard]] QVector<ProtonBuild> discoverProtonBuilds(
    const QVector<ProtonRoot> &roots);

// The literal floating names umu accepts -- empty, "GE-Proton",
// "GE-Latest", "UMU-Latest", "UMU-Proton", "latest" -- compared
// case-insensitively. Pure syntax; knows nothing about what is installed.
[[nodiscard]] bool isProtonAliasName(const QString &value);

// True for a string that could name a build directory: non-empty, bounded,
// single path component, no control characters, and not an alias.
[[nodiscard]] bool isValidProtonBuildName(const QString &name);

// Valve's rolling channels, which Steam replaces in place: a Steam-origin
// build whose name contains "Experimental", "Hotfix" or "Next"
// (case-insensitive) -- "Proton - Experimental", "Proton Hotfix",
// "Proton Next". Floating by construction; never pinnable.
[[nodiscard]] bool isRollingProtonChannel(const QString &name,
                                          ProtonBuild::Origin origin);

// The human part of a version file line: "1756415527 GE-Proton11-6" ->
// "GE-Proton11-6" (a leading all-digit timestamp is dropped). Empty in,
// "unknown" out.
[[nodiscard]] QString protonVersionLabel(const QString &versionText);

// "Updated by Steam — not pinnable", "No version file — not pinnable", or
// empty for a pinnable build. For lists that show every build.
[[nodiscard]] QString protonBuildStatusLabel(const ProtonBuild &build);

// Re-checks, at launch time, that the build's `proton` entry point is still
// an executable regular file and not a symlink. Bounded: one lstat.
[[nodiscard]] bool protonBuildStillPresent(const ProtonBuild &build);

} // namespace QindaQt::QindaLutris
