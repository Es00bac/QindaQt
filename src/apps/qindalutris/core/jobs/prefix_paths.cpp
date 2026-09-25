// SPDX-License-Identifier: GPL-3.0-or-later
#include "prefix_paths.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kMaxEntriesPerLevel = 64;

void expandInto(const QString &base, const QStringList &rest, int maxMatches,
                QStringList &out) {
  if (out.size() >= maxMatches) {
    return;
  }
  if (rest.isEmpty()) {
    const QFileInfo info(base);
    if (info.isFile() && !info.isSymLink()) {
      out.append(base);
    }
    return;
  }
  const QString head = rest.first();
  const QStringList tail = rest.mid(1);
  if (head != QLatin1String("*")) {
    expandInto(base + QLatin1Char('/') + head, tail, maxMatches, out);
    return;
  }
  QStringList names = QDir(base).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  std::sort(names.begin(), names.end(), std::greater<>());
  int visited = 0;
  for (const QString &name : std::as_const(names)) {
    if (++visited > kMaxEntriesPerLevel) {
      break;
    }
    const QString child = base + QLatin1Char('/') + name;
    if (QFileInfo(child).isSymLink()) {
      continue;
    }
    expandInto(child, tail, maxMatches, out);
  }
}

} // namespace

QString windowsPathToPrefixPath(const QString &prefixDir, const QString &windowsPath) {
  if (prefixDir.isEmpty() || QDir::isRelativePath(prefixDir) || windowsPath.size() < 3 ||
      prefixDir.split(QLatin1Char('/')).contains(QStringLiteral("*"))) {
    return {};
  }
  const QChar drive = windowsPath.at(0).toLower();
  if (drive < QLatin1Char('a') || drive > QLatin1Char('z') ||
      windowsPath.at(1) != QLatin1Char(':')) {
    return {};
  }
  const QChar separator = windowsPath.at(2);
  if (separator != QLatin1Char('\\') && separator != QLatin1Char('/')) {
    return {};
  }
  QString rest = windowsPath.mid(3);
  rest.replace(QLatin1Char('\\'), QLatin1Char('/'));
  const QStringList segments = rest.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (segments.isEmpty()) {
    return {};
  }
  for (const QString &segment : segments) {
    if (segment == QLatin1String(".") || segment == QLatin1String("..")) {
      return {};
    }
  }
  return QDir::cleanPath(prefixDir) + QStringLiteral("/drive_") + drive + QLatin1Char('/') +
         segments.join(QLatin1Char('/'));
}

QStringList expandPrefixCandidate(const QString &unixPattern, int maxMatches) {
  QStringList out;
  if (unixPattern.isEmpty() || QDir::isRelativePath(unixPattern) || maxMatches <= 0) {
    return out;
  }
  const QStringList segments = unixPattern.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  expandInto(QString(), segments, maxMatches, out);
  return out;
}

QString firstExistingCandidate(const QString &prefixDir,
                               const QStringList &windowsCandidates) {
  for (const QString &candidate : windowsCandidates) {
    const QString pattern = windowsPathToPrefixPath(prefixDir, candidate);
    if (pattern.isEmpty()) {
      continue;
    }
    const QStringList found = expandPrefixCandidate(pattern, 1);
    if (!found.isEmpty()) {
      return found.first();
    }
  }
  return {};
}

} // namespace QindaQt::QindaLutris
