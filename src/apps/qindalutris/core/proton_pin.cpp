// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_pin.h"

#include <QDir>
#include <QFileInfo>

namespace QindaQt::QindaLutris {
namespace {

constexpr int kMaxReasonNameChars = 128;

QString printable(const QString &text) {
  QString out;
  for (const QChar ch : text) {
    if (out.size() >= kMaxReasonNameChars) {
      break;
    }
    if (ch.category() != QChar::Other_Control && !ch.isNull()) {
      out += ch;
    }
  }
  return out.trimmed();
}

// An absolute pin is reported by its last component: the user knows a
// build as "GE-Proton11-6", not as a directory (ADR-0275 section 5).
QString nameForReason(const QString &pinned) {
  if (QDir::isAbsolutePath(pinned)) {
    QString trimmed = QDir::cleanPath(pinned);
    if (trimmed.endsWith(QStringLiteral("/proton"))) {
      trimmed.chop(7);
    }
    return printable(QFileInfo(trimmed).fileName());
  }
  return printable(pinned);
}

bool matchesPath(const ProtonBuild &build, const QString &cleanedPath) {
  return cleanedPath == build.path
         || cleanedPath == build.path + QStringLiteral("/proton");
}

// The builds a pin could mean, in catalog order (System first).
QVector<ProtonBuild> candidatesFor(const QString &pinned,
                                   const QVector<ProtonBuild> &builds) {
  QVector<ProtonBuild> out;
  const bool absolute = QDir::isAbsolutePath(pinned);
  const QString cleaned = absolute ? QDir::cleanPath(pinned) : pinned;
  for (const ProtonBuild &build : builds) {
    if (absolute ? matchesPath(build, cleaned) : build.name == pinned) {
      out.append(build);
    }
  }
  return out;
}

PinnedBuildResolution failure(PinnedBuildResolution::Failure kind,
                              const QString &reason) {
  PinnedBuildResolution out;
  out.failure = kind;
  out.reason = reason;
  return out;
}

QString notPinnableReason(const ProtonBuild &build) {
  if (isRollingProtonChannel(build.name, build.origin)) {
    return QStringLiteral(
        "%1 is updated by Steam and cannot be pinned. Choose another Proton "
        "build for this game.").arg(printable(build.displayName));
  }
  return QStringLiteral(
      "%1 has no version file, so QindaLutris cannot tell whether it changed. "
      "Choose another Proton build for this game.")
      .arg(printable(build.displayName));
}

} // namespace

bool isFloatingProtonAlias(const QString &value,
                           const QVector<ProtonBuild> &knownBuilds) {
  if (isProtonAliasName(value)) {
    return true;
  }
  if (QDir::isAbsolutePath(value)) {
    return false;
  }
  bool known = false;
  for (const ProtonBuild &build : knownBuilds) {
    if (build.name == value) {
      known = true;
      if (!build.pinnable && isRollingProtonChannel(build.name, build.origin)) {
        return true; // "Proton - Experimental" moves under Steam's control
      }
    }
  }
  return !known;
}

QString protonNotInstalledReason(const QString &pinnedName) {
  return QStringLiteral(
      "%1 is not installed. Reinstall it or choose another Proton build for "
      "this game.").arg(nameForReason(pinnedName));
}

PinnedBuildResolution resolvePinnedBuild(const ProtonPin &pin,
                                         const QVector<ProtonBuild> &builds) {
  using Failure = PinnedBuildResolution::Failure;
  if (pin.isEmpty()) {
    return failure(Failure::NotChosen,
                   QStringLiteral("Choose a Proton build for this game."));
  }
  if (isProtonAliasName(pin.name)) {
    return failure(Failure::FloatingAlias,
                   QStringLiteral("\"%1\" is not a specific Proton build. "
                                  "Choose an installed build for this game.")
                       .arg(printable(pin.name)));
  }
  // AGENT-GUARD: exact matches only -- name (or path) AND version. No
  // prefix match, no version-family match, no "closest" build.
  const QVector<ProtonBuild> candidates = candidatesFor(pin.name, builds);
  if (candidates.isEmpty()) {
    return failure(Failure::NotInstalled, protonNotInstalledReason(pin.name));
  }
  QVector<ProtonBuild> pinnable;
  for (const ProtonBuild &build : candidates) {
    if (build.pinnable) {
      pinnable.append(build);
    }
  }
  if (pinnable.isEmpty()) {
    return failure(Failure::NotPinnable, notPinnableReason(candidates.first()));
  }
  if (pin.version.trimmed().isEmpty()) {
    return failure(Failure::NotConfirmed,
                   QStringLiteral("QindaLutris has not recorded which version "
                                  "of %1 this game uses. Confirm it in "
                                  "QindaLutris before playing.")
                       .arg(nameForReason(pin.name)));
  }
  for (const ProtonBuild &build : pinnable) {
    if (build.versionText == pin.version) {
      PinnedBuildResolution out;
      out.build = build;
      return out;
    }
  }
  const ProtonBuild &now = pinnable.first();
  return failure(Failure::VersionChanged,
                 QStringLiteral("%1 has changed since this game was set up "
                                "(was %2, now %3). Confirm the new version in "
                                "QindaLutris before playing.")
                     .arg(printable(now.displayName),
                          protonVersionLabel(pin.version),
                          protonVersionLabel(now.versionText)));
}

std::optional<ProtonBuild> chooseDefaultBuild(const QVector<ProtonBuild> &builds,
                                              const QString &preferredName) {
  if (!preferredName.isEmpty()) {
    for (const ProtonBuild &build : builds) {
      if (build.name == preferredName && build.pinnable) {
        return build;
      }
    }
  }
  // The catalog is sorted newest-first within each origin.
  for (const ProtonBuild::Origin origin :
       {ProtonBuild::Origin::System, ProtonBuild::Origin::User}) {
    for (const ProtonBuild &build : builds) {
      if (build.origin == origin && build.pinnable) {
        return build;
      }
    }
  }
  return std::nullopt;
}

ProtonPin pinForBuild(const ProtonBuild &build) {
  return {build.name, build.versionText};
}

std::optional<ProtonPin> pinForNewEntry(const QString &requested,
                                        const QVector<ProtonBuild> &builds,
                                        const QString &preferredName) {
  if (requested.trimmed().isEmpty()) {
    const std::optional<ProtonBuild> fallback =
        chooseDefaultBuild(builds, preferredName);
    return fallback.has_value() ? std::optional<ProtonPin>(pinForBuild(*fallback))
                                : std::nullopt;
  }
  if (isProtonAliasName(requested)) {
    return std::nullopt;
  }
  for (const ProtonBuild &build : candidatesFor(requested, builds)) {
    if (build.pinnable) {
      return pinForBuild(build);
    }
  }
  return std::nullopt;
}

std::optional<ProtonPin> confirmPinnedBuild(const QString &pinnedName,
                                            const QVector<ProtonBuild> &builds) {
  if (isProtonAliasName(pinnedName)) {
    return std::nullopt;
  }
  for (const ProtonBuild &build : candidatesFor(pinnedName, builds)) {
    if (build.pinnable) {
      return ProtonPin{pinnedName, build.versionText};
    }
  }
  return std::nullopt;
}

} // namespace QindaQt::QindaLutris
