// SPDX-License-Identifier: GPL-3.0-or-later
#include "proton_catalog.h"

#include "store_io.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace QindaQt::QindaLutris {
namespace {

constexpr qint64 kMaxVersionFileBytes = 4096;
constexpr qint64 kMaxToolVdfBytes = qint64(16) * 1024;
constexpr int kMaxLabelChars = 128;

// A regular, non-symlink file's bytes (opened O_NOFOLLOW), or empty.
QByteArray readSmallRegularFile(const QString &path, qint64 maxBytes) {
  StoreIo::ReadStatus status = StoreIo::ReadStatus::Refused;
  const QByteArray bytes = StoreIo::readBoundedFile(path, maxBytes, &status);
  return status == StoreIo::ReadStatus::Ok ? bytes : QByteArray();
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
  const int byName = naturalCompare(a.name, b.name);
  if (byName != 0) {
    return byName > 0; // newest-looking first
  }
  return a.path < b.path;
}

// Scans one canonical root with a bounded iterator: at most
// kMaxProtonEntriesScannedPerRoot entries are read, and only eligible names
// count toward kMaxProtonDirsPerRoot.
void scanRoot(const QString &canonicalRoot, ProtonBuild::Origin origin,
              QVector<ProtonBuild> *out) {
  QDirIterator it(canonicalRoot, QDir::AllEntries | QDir::NoDotAndDotDot
                                     | QDir::Hidden | QDir::System);
  int scanned = 0;
  int candidates = 0;
  while (it.hasNext() && scanned < kMaxProtonEntriesScannedPerRoot
         && candidates < kMaxProtonDirsPerRoot) {
    const QFileInfo entry = it.nextFileInfo();
    ++scanned;
    const QString name = entry.fileName();
    if (name.startsWith(QLatin1Char('.'))) {
      continue; // hidden: job staging/trash, see AGENT-CONTRACT in the header
    }
    if (origin == ProtonBuild::Origin::Steam
        && !name.startsWith(QLatin1String("Proton"))) {
      continue; // steamapps/common holds games too
    }
    if (entry.isSymLink() || !entry.isDir() || !isValidProtonBuildName(name)) {
      continue; // see AGENT-GUARD in the header
    }
    ++candidates;
    const QString dir = canonicalRoot + QLatin1Char('/') + name;
    const QFileInfo script(dir + QStringLiteral("/proton"));
    if (!script.isFile() || script.isSymLink() || !script.isExecutable()) {
      continue;
    }
    ProtonBuild build;
    build.name = name;
    build.path = dir;
    build.versionText = versionTextFor(dir);
    build.displayName = displayNameFor(dir);
    if (build.displayName.isEmpty()) {
      build.displayName = name;
    }
    build.origin = origin;
    build.removable = origin == ProtonBuild::Origin::User;
    build.pinnable =
        !isRollingProtonChannel(name, origin) && !build.versionText.isEmpty();
    out->append(build);
  }
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
  const QString flatpak = home + QStringLiteral("/.var/app/com.valvesoftware.Steam");
  const QStringList flatpakSteam{flatpak + QStringLiteral("/.local/share/Steam"),
                                 flatpak + QStringLiteral("/data/Steam")};
  QVector<ProtonRoot> out{
      {QString(kSystemProtonRoot), ProtonBuild::Origin::System},
      {dataHome + QStringLiteral("/Steam/compatibilitytools.d"),
       ProtonBuild::Origin::User},
      {home + QStringLiteral("/.steam/root/compatibilitytools.d"),
       ProtonBuild::Origin::User},
  };
  for (const QString &steam : flatpakSteam) {
    out.append({steam + QStringLiteral("/compatibilitytools.d"),
                ProtonBuild::Origin::User});
  }
  QStringList libraries;
  for (const QString &library : steamLibraryRoots + flatpakSteam) {
    if (!library.isEmpty() && !libraries.contains(library)) {
      libraries.append(library);
    }
  }
  for (const QString &library : libraries) {
    if (out.size() >= kMaxProtonRoots) {
      break;
    }
    out.append({library + QStringLiteral("/steamapps/common"),
                ProtonBuild::Origin::Steam});
  }
  return out;
}

QVector<ProtonBuild> discoverProtonBuilds(const QVector<ProtonRoot> &roots) {
  QVector<ProtonBuild> out;
  QSet<QString> seenRoots;
  for (const ProtonRoot &root : roots) {
    if (seenRoots.size() >= kMaxProtonRoots) {
      break;
    }
    const QString canonicalRoot =
        root.path.isEmpty() ? QString() : QDir(root.path).canonicalPath();
    if (canonicalRoot.isEmpty() || seenRoots.contains(canonicalRoot)) {
      continue; // absent, or the same directory reached twice
    }
    seenRoots.insert(canonicalRoot);
    scanRoot(canonicalRoot, root.origin, &out);
  }
  std::sort(out.begin(), out.end(), buildLess);
  if (out.size() > kMaxProtonBuilds) {
    out.resize(kMaxProtonBuilds);
  }
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

bool isRollingProtonChannel(const QString &name, ProtonBuild::Origin origin) {
  if (origin != ProtonBuild::Origin::Steam) {
    return false;
  }
  for (const QLatin1String channel :
       {QLatin1String("experimental"), QLatin1String("hotfix"),
        QLatin1String("next")}) {
    if (name.contains(channel, Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
}

QString protonVersionLabel(const QString &versionText) {
  const QStringList parts =
      versionText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return QStringLiteral("unknown");
  }
  static const QRegularExpression digits(QStringLiteral("^\\d+$"));
  if (parts.size() > 1 && digits.match(parts.first()).hasMatch()) {
    return cleanLabel(parts.mid(1).join(QLatin1Char(' ')));
  }
  return cleanLabel(versionText);
}

QString protonBuildStatusLabel(const ProtonBuild &build) {
  if (isRollingProtonChannel(build.name, build.origin)) {
    return QStringLiteral("Updated by Steam — not pinnable");
  }
  if (build.versionText.isEmpty()) {
    return QStringLiteral("No version file — not pinnable");
  }
  return {};
}

bool protonBuildStillPresent(const ProtonBuild &build) {
  if (build.path.isEmpty()) {
    return false;
  }
  const QFileInfo script(build.path + QStringLiteral("/proton"));
  return script.isFile() && !script.isSymLink() && script.isExecutable();
}

} // namespace QindaQt::QindaLutris
