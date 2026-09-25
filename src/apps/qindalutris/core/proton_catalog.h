// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the Proton catalog of ADR-0275 section 2. It answers two
// questions and nothing else: which concrete Proton builds are installed,
// and which exact build a pinned name means. It NEVER picks a substitute:
// a pinned build that is not installed is a typed failure with a
// plain-language reason, never "the nearest build" (ADR-0275 section 1 --
// a floating PROTONPATH silently moved World of Warcraft to a build that
// stalled it). Pure over injected roots: tests hand it QTemporaryDir trees,
// production hands it defaultProtonRoots(). Reads are shallow, fixed-name
// and bounded; a hostile or corrupt root yields fewer builds, never an
// error. Callers own the returned values; nothing here caches.

struct ProtonBuild final {
  enum class Origin {
    System, // /usr/share/steam/compatibilitytools.d -- Portage owns it
    User,   // a user compatibilitytools.d -- the app may remove it
    Steam,  // Valve Proton under a Steam library's steamapps/common
  };

  QString name;        // directory name: the identity a title pins
  QString displayName; // compatibilitytool.vdf "display_name", else name
  QString versionText; // first line of the `version` file; may be empty
  QString path;        // absolute build directory (the PROTONPATH value)
  Origin origin = Origin::User;
  bool removable = false; // true only for Origin::User

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
inline constexpr int kMaxProtonDirsPerRoot = 128;
inline constexpr int kMaxProtonBuilds = 64;
inline constexpr int kMaxProtonBuildNameChars = 128;

// The directory Portage installs pinned builds into
// (app-emulation/ge-proton-bin, one slot per release).
inline constexpr QLatin1StringView kSystemProtonRoot{
    "/usr/share/steam/compatibilitytools.d"};

// The ADR-0275 root order: the system root, $XDG_DATA_HOME/Steam/
// compatibilitytools.d (xdgDataHome empty => home/.local/share),
// home/.steam/root/compatibilitytools.d, then <library>/steamapps/common for
// each Steam library root. Pure: nothing is statted.
[[nodiscard]] QVector<ProtonRoot> defaultProtonRoots(
    const QString &home, const QString &xdgDataHome,
    const QStringList &steamLibraryRoots);

// Every usable build under the roots. A build is a directory holding an
// executable regular `proton` file. Roots are de-duplicated by canonical
// path; a build name seen in an earlier root shadows later ones, so a
// System build wins over a user copy of the same name. The result is sorted
// by origin (System, User, Steam) and then by name, newest-looking first
// (natural order, descending: GE-Proton11-6 before GE-Proton10-25).
// AGENT-GUARD: symlinked build directories and symlinked `proton` scripts
// are SKIPPED. A link such as compatibilitytools.d/GE-Proton -> GE-Proton11-7
// can be retargeted by anyone, which is the silent build move this catalog
// exists to prevent; a pinned name must always mean the same bytes. For the
// same reason a directory whose NAME is a floating alias ("GE-Proton") is
// skipped. Roots themselves may be symlinks (~/.steam/root usually is) and
// are resolved.
[[nodiscard]] QVector<ProtonBuild> discoverProtonBuilds(
    const QVector<ProtonRoot> &roots);

// The literal floating names umu accepts -- empty, "GE-Proton",
// "GE-Latest", "UMU-Latest", "UMU-Proton", "latest" -- compared
// case-insensitively. Pure syntax; knows nothing about what is installed.
[[nodiscard]] bool isProtonAliasName(const QString &value);

// True when value cannot be a pin: an alias name, or anything that is
// neither an absolute path nor the exact name of one of knownBuilds.
[[nodiscard]] bool isFloatingProtonAlias(const QString &value,
                                         const QVector<ProtonBuild> &knownBuilds);

// True for a string that could name a build directory: non-empty, bounded,
// single path component, no control characters, and not an alias. Used by
// stores that must refuse a record whose pin could never resolve.
[[nodiscard]] bool isValidProtonBuildName(const QString &name);

struct PinnedBuildResolution final {
  enum class Failure {
    None,
    NotChosen,     // empty pin
    FloatingAlias, // "GE-Proton" and friends
    NotInstalled,  // a concrete pin that is not in the catalog
  };

  std::optional<ProtonBuild> build;
  Failure failure = Failure::None;
  QString reason; // one plain-language sentence when failure != None

  [[nodiscard]] bool ok() const { return build.has_value(); }
};

// Resolves a pin to exactly one build. The pin is a build name (matched
// exactly, case-sensitively) or an absolute path, which matches a build's
// directory or -- for hand-added entries written before ADR-0275 -- that
// build's `proton` script. Anything else fails; there is no fallback.
[[nodiscard]] PinnedBuildResolution resolvePinnedBuild(
    const QString &pinned, const QVector<ProtonBuild> &builds);

// The build a NEW install should pin: preferredName when installed, else
// the first System build, else the first build, else none. Never consulted
// for an existing title (ADR-0275: a title never moves by itself).
[[nodiscard]] std::optional<ProtonBuild> chooseDefaultBuild(
    const QVector<ProtonBuild> &builds, const QString &preferredName);

// The pin a NEW entry records, always a build NAME: for an empty request,
// chooseDefaultBuild's pick; otherwise the build the request resolves to
// exactly (a name, a build directory, or its `proton` script). Nullopt when
// the request does not resolve or nothing is installed -- the caller then
// refuses to record the entry rather than store an unlaunchable pin.
[[nodiscard]] std::optional<QString> pinForNewEntry(
    const QString &requested, const QVector<ProtonBuild> &builds,
    const QString &preferredName);

} // namespace QindaQt::QindaLutris
