// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "proton_catalog.h"

#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: pin identity and resolution (ADR-0275 sections 1 and 2).
// A pin is a build NAME plus the VERSION TEXT that build's `version` file
// held when the pin was recorded. It resolves to exactly one catalog build
// or to a typed failure with one plain-language sentence. It NEVER picks a
// substitute: not a newer build, not a prefix match, and not the same name
// after Steam or anyone else changed its contents in place -- a floating
// PROTONPATH silently moved World of Warcraft to a build that stalled it.
// Re-pinning is always an explicit act (confirmPinnedBuild). Pure over the
// catalog it is given; no I/O.

struct ProtonPin final {
  // A build directory name, or -- for hand-added entries saved before
  // ADR-0275 -- an absolute path of a build or of its `proton` script.
  QString name;
  QString version; // ProtonBuild::versionText at pin time

  [[nodiscard]] bool isEmpty() const { return name.trimmed().isEmpty(); }
  friend bool operator==(const ProtonPin &, const ProtonPin &) = default;
};

// True when value cannot be a pin: an alias name; anything that is neither
// an absolute path nor the exact name of one of knownBuilds; or the name of
// a known build that is not pinnable (a Steam rolling channel).
[[nodiscard]] bool isFloatingProtonAlias(const QString &value,
                                         const QVector<ProtonBuild> &knownBuilds);

struct PinnedBuildResolution final {
  enum class Failure {
    None,
    NotChosen,      // empty pin
    FloatingAlias,  // "GE-Proton" and friends
    NotInstalled,   // no build of that name / at that path
    NotPinnable,    // a Steam rolling channel, or no version file
    NotConfirmed,   // a pin with no recorded version (pre-version record)
    VersionChanged, // same name, different contents
  };

  std::optional<ProtonBuild> build;
  Failure failure = Failure::None;
  QString reason; // one plain-language sentence when failure != None

  [[nodiscard]] bool ok() const { return build.has_value(); }
};

// Resolves by (name, version): among the pinnable builds of that name whose
// versionText equals pin.version, the first in catalog order -- so a System
// build wins over a User copy with the same name and version. An absolute
// pin matches exactly one build's directory (or its `proton` script) and
// must also match its version. See Failure for every refusal.
[[nodiscard]] PinnedBuildResolution resolvePinnedBuild(
    const ProtonPin &pin, const QVector<ProtonBuild> &builds);

// The build a NEW install should pin: preferredName when installed and
// pinnable, else the newest pinnable System build, else the newest
// pinnable User build, else none. Steam-origin builds never take part in
// the fallback. Never consulted for an existing title.
[[nodiscard]] std::optional<ProtonBuild> chooseDefaultBuild(
    const QVector<ProtonBuild> &builds, const QString &preferredName);

// The pin that records `build` as it is now.
[[nodiscard]] ProtonPin pinForBuild(const ProtonBuild &build);

// The pin a NEW entry records, always by NAME: for an empty request,
// chooseDefaultBuild's pick; otherwise the pinnable build the request names
// (a name -- System copy first -- or an exact directory or `proton` script
// path). Nullopt when nothing suitable is installed or the request is an
// alias, a rolling channel or unknown; the caller then refuses the entry.
[[nodiscard]] std::optional<ProtonPin> pinForNewEntry(
    const QString &requested, const QVector<ProtonBuild> &builds,
    const QString &preferredName);

// The explicit "yes, use the build as it is now" re-pin the UI offers after
// VersionChanged or NotConfirmed. Keeps pinnedName as given (a legacy path
// stays a path) and records the current version of the build it names:
// by name, the first pinnable copy in catalog order; by path, that exact
// build. Nullopt when the name is not installed or not pinnable. Never
// called implicitly by a launch.
[[nodiscard]] std::optional<ProtonPin> confirmPinnedBuild(
    const QString &pinnedName, const QVector<ProtonBuild> &builds);

// The sentence for a pinned build that is gone.
[[nodiscard]] QString protonNotInstalledReason(const QString &pinnedName);

} // namespace QindaQt::QindaLutris
