// SPDX-License-Identifier: GPL-3.0-or-later
#include "executable_candidates.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

namespace QindaQt::QindaLutris {

namespace {

// Lower-case letters and digits only: "Hollow Knight" -> "hollowknight".
QString squashed(const QString &text) {
  QString out;
  for (const QChar c : text) {
    if (c.isLetterOrNumber()) {
      out.append(c.toLower());
    }
  }
  return out;
}

QStringList words(const QString &text) {
  QStringList out;
  QString current;
  for (const QChar c : text) {
    if (c.isLetterOrNumber()) {
      current.append(c.toLower());
    } else if (!current.isEmpty()) {
      out.append(current);
      current.clear();
    }
  }
  if (!current.isEmpty()) {
    out.append(current);
  }
  return out;
}

struct Walk {
  ExecutableScanLimits limits;
  int visited = 0;
  ExecutableSnapshot found;
};

void walk(Walk &state, const QString &directory, int depth) {
  if (depth > state.limits.maxDepth) {
    return;
  }
  const QFileInfoList entries =
      QDir(directory).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                    QDir::Name);
  for (const QFileInfo &entry : entries) {
    if (++state.visited > state.limits.maxEntries) {
      return;
    }
    if (entry.isSymLink()) {
      continue;
    }
    if (entry.isDir()) {
      walk(state, entry.filePath(), depth + 1);
    } else if (entry.isFile() &&
               entry.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0) {
      state.found.insert(entry.filePath(), entry.size());
    }
  }
}

int scoreFor(const QString &path, qint64 size, const QString &title) {
  const QFileInfo info(path);
  const QString name = squashed(info.completeBaseName());
  const QString wanted = squashed(title);
  int score = 0;
  if (!wanted.isEmpty() && !name.isEmpty()) {
    if (name == wanted) {
      score += 100;
    } else if ((name.size() >= 3 && wanted.contains(name)) ||
               (wanted.size() >= 3 && name.contains(wanted))) {
      score += 60;
    }
  }
  const QString folders = squashed(info.absolutePath().section(QLatin1String("drive_c"), 1));
  for (const QString &word : words(title)) {
    if (word.size() < 2) {
      continue;
    }
    if (name.contains(word)) {
      score += 15;
    } else if (folders.contains(word)) {
      score += 5;
    }
  }
  if (size > 64 * 1024) {
    const double units = std::log2(double(size) / (64.0 * 1024.0));
    score += std::min(40, static_cast<int>(units * 4.0));
  }
  return score;
}

} // namespace

ExecutableSnapshot scanPrefixExecutables(const QString &prefixDir, ExecutableScanLimits limits) {
  Walk state;
  state.limits = limits;
  if (prefixDir.isEmpty() || QDir::isRelativePath(prefixDir)) {
    return state.found;
  }
  const QString driveC = QDir::cleanPath(prefixDir) + QStringLiteral("/drive_c");
  QStringList roots{driveC + QStringLiteral("/Program Files"),
                    driveC + QStringLiteral("/Program Files (x86)"),
                    driveC + QStringLiteral("/Games"), driveC + QStringLiteral("/GOG Games")};
  const QFileInfoList users =
      QDir(driveC + QStringLiteral("/users")).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &user : users) {
    if (!user.isSymLink()) {
      roots.append(user.filePath() + QStringLiteral("/AppData/Local/Programs"));
    }
  }
  for (const QString &root : std::as_const(roots)) {
    const QFileInfo info(root);
    if (info.isDir() && !info.isSymLink()) {
      walk(state, root, 0);
    }
  }
  return state.found;
}

bool isHelperExecutableName(const QString &fileName) {
  const QString name = fileName.toLower();
  if (name.startsWith(QLatin1String("unins"))) {
    return true;
  }
  static const QStringList fragments{
      QStringLiteral("setup"),     QStringLiteral("uninstall"), QStringLiteral("installer"),
      QStringLiteral("crash"),     QStringLiteral("redist"),    QStringLiteral("prereq"),
      QStringLiteral("bugreport"), QStringLiteral("reporter"),  QStringLiteral("dxweb"),
      QStringLiteral("vcredist"),  QStringLiteral("dotnetfx")};
  for (const QString &fragment : fragments) {
    if (name.contains(fragment)) {
      return true;
    }
  }
  return false;
}

QVector<ExecutableCandidate> rankNewExecutables(const ExecutableSnapshot &before,
                                                const ExecutableSnapshot &after,
                                                const QString &title, int maxCandidates) {
  QVector<ExecutableCandidate> out;
  for (auto it = after.constBegin(); it != after.constEnd(); ++it) {
    const auto previous = before.constFind(it.key());
    if (previous != before.constEnd() && previous.value() == it.value()) {
      continue;
    }
    if (isHelperExecutableName(QFileInfo(it.key()).fileName())) {
      continue;
    }
    out.append({it.key(), it.value(), scoreFor(it.key(), it.value(), title)});
  }
  std::sort(out.begin(), out.end(), [](const ExecutableCandidate &a, const ExecutableCandidate &b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    if (a.sizeBytes != b.sizeBytes) {
      return a.sizeBytes > b.sizeBytes;
    }
    return a.unixPath < b.unixPath;
  });
  if (out.size() > maxCandidates) {
    out.resize(std::max(0, maxCandidates));
  }
  return out;
}

} // namespace QindaQt::QindaLutris
