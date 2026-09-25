// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace QindaQt::QindaLutris {
namespace {

constexpr qint64 kMaxVersionFileBytes = 4096;
constexpr qint64 kMaxToolVdfBytes = qint64(16) * 1024;
constexpr int kMaxLabelChars = 128;

// A regular, non-symlink file's first maxBytes, or empty.
QByteArray readSmallRegularFile(const QString &path, qint64 maxBytes) {
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink() || info.size() > maxBytes) {
    return {};
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  return file.read(maxBytes);
}

// Keeps a label printable: control characters dropped, length capped.
QString cleanLabel(const QString &text) {
  QString out;
  for (const QChar ch : text) {
    if (out.size() >= kMaxLabelChars) {
      break;
    }
    if (ch.category() != QChar::Other_Control && !ch.isNull()) {
      out += ch;
    }
  }
  return out.trimmed();
}

QString versionTextFor(const QString &buildDir) {
  const QByteArray bytes =
      readSmallRegularFile(buildDir + QStringLiteral("/version"),
                           kMaxVersionFileBytes);
  const qsizetype newline = bytes.indexOf('\n');
  return cleanLabel(QString::fromUtf8(newline < 0 ? bytes : bytes.left(newline)));
}

// compatibilitytool.vdf is Steam's own description of a tool. Only the one
// "display_name" value is read, by pattern, from a bounded file: this is a
// label, never a key into anything.
QString displayNameFor(const QString &buildDir) {
  const QByteArray bytes = readSmallRegularFile(
      buildDir + QStringLiteral("/compatibilitytool.vdf"), kMaxToolVdfBytes);
  if (bytes.isEmpty()) {
    return {};
  }
  static const QRegularExpression pattern(
      QStringLiteral("\"display_name\"\\s+\"([^\"\\n]{1,128})\""));
  const QRegularExpressionMatch match = pattern.match(QString::fromUtf8(bytes));
  return match.hasMatch() ? cleanLabel(match.captured(1)) : QString();
}

int originRank(ProtonBuild::Origin origin) {
  switch (origin) {
  case ProtonBuild::Origin::System: return 0;
  case ProtonBuild::Origin::User: return 1;
  case ProtonBuild::Origin::Steam: return 2;
  }
  Q_UNREACHABLE();
}

// Natural comparison: digit runs compare numerically, so "11-6" > "10-25"
// and "11-10" > "11-6". Returns <0, 0, >0.
int naturalCompare(const QString &a, const QString &b) {
  qsizetype i = 0;
  qsizetype j = 0;
  while (i < a.size() && j < b.size()) {
    if (a.at(i).isDigit() && b.at(j).isDigit()) {
      qsizetype ie = i;
      qsizetype je = j;
      while (ie < a.size() && a.at(ie).isDigit()) ++ie;
      while (je < b.size() && b.at(je).isDigit()) ++je;
      // Strip leading zeros, then longer run is larger, then lexical.
      QStringView ra = QStringView(a).mid(i, ie - i);
      QStringView rb = QStringView(b).mid(j, je - j);
      while (ra.size() > 1 && ra.front() == QLatin1Char('0')) ra = ra.mid(1);
      while (rb.size() > 1 && rb.front() == QLatin1Char('0')) rb = rb.mid(1);
      if (ra.size() != rb.size()) {
        return ra.size() < rb.size() ? -1 : 1;
      }
      const int byDigits = ra.compare(rb);
      if (byDigits != 0) {
        return byDigits;
      }
      i = ie;
      j = je;
      continue;
    }
    const int byChar = QString::compare(QString(a.at(i)), QString(b.at(j)),
                                        Qt::CaseInsensitive);
    if (byChar != 0) {
      return byChar;
    }
    ++i;
    ++j;
  }
  if (i < a.size()) return 1;
  if (j < b.size()) return -1;
  return QString::compare(a, b); // case-only differences: deterministic
}

bool buildLess(const ProtonBuild &a, const ProtonBuild &b) {
  const int ra = originRank(a.origin);
  const int rb = originRank(b.origin);
  if (ra != rb) {
    return ra < rb;
  }
  return naturalCompare(a.name, b.name) > 0; // newest-looking first
}

QString failureName(const QString &pinned) {
  // An absolute pin is reported by its last component: the user knows a
  // build as "GE-Proton11-6", not as a directory (ADR-0275 section 5).
  if (QDir::isAbsolutePath(pinned)) {
    QString trimmed = QDir::cleanPath(pinned);
    if (trimmed.endsWith(QStringLiteral("/proton"))) {
      trimmed.chop(7);
    }
    return cleanLabel(QFileInfo(trimmed).fileName());
  }
  return cleanLabel(pinned);
}

} // namespace

QString protonOriginId(ProtonBuild::Origin origin) {
  switch (origin) {
  case ProtonBuild::Origin::System: return QStringLiteral("system");
  case ProtonBuild::Origin::User: return QStringLiteral("user");
  case ProtonBuild::Origin::Steam: return QStringLiteral("steam");
  }
  Q_UNREACHABLE();
}

QVector<ProtonRoot> defaultProtonRoots(const QString &home,
                                       const QString &xdgDataHome,
                                       const QStringList &steamLibraryRoots) {
  const QString dataHome = xdgDataHome.isEmpty()
                               ? home + QStringLiteral("/.local/share")
                               : xdgDataHome;
  QVector<ProtonRoot> out{
      {QString(kSystemProtonRoot), ProtonBuild::Origin::System},
      {dataHome + QStringLiteral("/Steam/compatibilitytools.d"),
       ProtonBuild::Origin::User},
      {home + QStringLiteral("/.steam/root/compatibilitytools.d"),
       ProtonBuild::Origin::User},
  };
  for (const QString &library : steamLibraryRoots) {
    if (out.size() >= kMaxProtonRoots) {
      break;
    }
    if (!library.isEmpty()) {
      out.append({library + QStringLiteral("/steamapps/common"),
                  ProtonBuild::Origin::Steam});
    }
  }
  return out;
}

QVector<ProtonBuild> discoverProtonBuilds(const QVector<ProtonRoot> &roots) {
  QVector<ProtonBuild> out;
  QSet<QString> seenRoots;
  QSet<QString> seenNames;
  int rootsScanned = 0;
  for (const ProtonRoot &root : roots) {
    if (rootsScanned >= kMaxProtonRoots || out.size() >= kMaxProtonBuilds) {
      break;
    }
    const QString canonicalRoot = QDir(root.path).canonicalPath();
    if (root.path.isEmpty() || canonicalRoot.isEmpty()
        || seenRoots.contains(canonicalRoot)) {
      continue; // absent, or the same directory reached twice
    }
    seenRoots.insert(canonicalRoot);
    ++rootsScanned;
    const QFileInfoList entries = QDir(canonicalRoot).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    int considered = 0;
    for (const QFileInfo &entry : entries) {
      if (++considered > kMaxProtonDirsPerRoot || out.size() >= kMaxProtonBuilds) {
        break;
      }
      const QString name = entry.fileName();
      if (entry.isSymLink() || !isValidProtonBuildName(name)) {
        continue; // see AGENT-GUARD in the header
      }
      if (root.origin == ProtonBuild::Origin::Steam
          && !name.startsWith(QLatin1String("Proton"))) {
        continue; // steamapps/common holds games too
      }
      const QString dir = canonicalRoot + QLatin1Char('/') + name;
      const QFileInfo script(dir + QStringLiteral("/proton"));
      if (!script.isFile() || script.isSymLink() || !script.isExecutable()) {
        continue;
      }
      if (seenNames.contains(name)) {
        continue; // an earlier root (System first) already owns the name
      }
      seenNames.insert(name);
      ProtonBuild build;
      build.name = name;
      build.path = dir;
      build.versionText = versionTextFor(dir);
      build.displayName = displayNameFor(dir);
      if (build.displayName.isEmpty()) {
        build.displayName = name;
      }
      build.origin = root.origin;
      build.removable = root.origin == ProtonBuild::Origin::User;
      out.append(build);
    }
  }
  std::stable_sort(out.begin(), out.end(), buildLess);
  return out;
}

bool isProtonAliasName(const QString &value) {
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    return true;
  }
  for (const QLatin1String alias :
       {QLatin1String("GE-Proton"), QLatin1String("GE-Latest"),
        QLatin1String("UMU-Latest"), QLatin1String("UMU-Proton"),
        QLatin1String("latest")}) {
    if (trimmed.compare(alias, Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return false;
}

bool isFloatingProtonAlias(const QString &value,
                           const QVector<ProtonBuild> &knownBuilds) {
  if (isProtonAliasName(value)) {
    return true;
  }
  if (QDir::isAbsolutePath(value)) {
    return false;
  }
  for (const ProtonBuild &build : knownBuilds) {
    if (build.name == value) {
      return false;
    }
  }
  return true;
}

bool isValidProtonBuildName(const QString &name) {
  if (name.isEmpty() || name.size() > kMaxProtonBuildNameChars
      || name == QLatin1String(".") || name == QLatin1String("..")
      || name.contains(QLatin1Char('/')) || isProtonAliasName(name)) {
    return false;
  }
  for (const QChar ch : name) {
    if (ch.category() == QChar::Other_Control || ch.isNull()) {
      return false;
    }
  }
  return true;
}

PinnedBuildResolution resolvePinnedBuild(const QString &pinned,
                                         const QVector<ProtonBuild> &builds) {
  PinnedBuildResolution out;
  if (pinned.trimmed().isEmpty()) {
    out.failure = PinnedBuildResolution::Failure::NotChosen;
    out.reason = QStringLiteral("Choose a Proton build for this game.");
    return out;
  }
  if (isProtonAliasName(pinned)) {
    out.failure = PinnedBuildResolution::Failure::FloatingAlias;
    out.reason = QStringLiteral(
        "\"%1\" is not a specific Proton build. Choose an installed build "
        "for this game.").arg(cleanLabel(pinned));
    return out;
  }
  // AGENT-GUARD: exact matches only. No prefix match, no version-family
  // match, no "closest" build -- a substitute is the ADR-0275 bug.
  const bool absolute = QDir::isAbsolutePath(pinned);
  const QString cleaned = absolute ? QDir::cleanPath(pinned) : pinned;
  for (const ProtonBuild &build : builds) {
    const bool matches = absolute
        ? (cleaned == build.path
           || cleaned == build.path + QStringLiteral("/proton"))
        : build.name == pinned;
    if (matches) {
      out.build = build;
      return out;
    }
  }
  out.failure = PinnedBuildResolution::Failure::NotInstalled;
  out.reason = QStringLiteral(
      "%1 is not installed. Reinstall it or choose another Proton build for "
      "this game.").arg(failureName(pinned));
  return out;
}

std::optional<ProtonBuild> chooseDefaultBuild(const QVector<ProtonBuild> &builds,
                                              const QString &preferredName) {
  if (!preferredName.isEmpty()) {
    for (const ProtonBuild &build : builds) {
      if (build.name == preferredName) {
        return build;
      }
    }
  }
  for (const ProtonBuild &build : builds) {
    if (build.origin == ProtonBuild::Origin::System) {
      return build;
    }
  }
  if (!builds.isEmpty()) {
    return builds.constFirst();
  }
  return std::nullopt;
}

std::optional<QString> pinForNewEntry(const QString &requested,
                                      const QVector<ProtonBuild> &builds,
                                      const QString &preferredName) {
  if (requested.trimmed().isEmpty()) {
    const std::optional<ProtonBuild> fallback =
        chooseDefaultBuild(builds, preferredName);
    return fallback.has_value() ? std::optional<QString>(fallback->name)
                                : std::nullopt;
  }
  const PinnedBuildResolution pin = resolvePinnedBuild(requested, builds);
  return pin.ok() ? std::optional<QString>(pin.build->name) : std::nullopt;
}

} // namespace QindaQt::QindaLutris
