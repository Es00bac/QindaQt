// SPDX-License-Identifier: GPL-3.0-or-later
#include "reveal_request.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace QindaQt::Apps::FileManager {
namespace {

// Caller text only ever returns to the caller in a D-Bus error reply; cap it.
[[nodiscard]] QString quoted(const QString &uri) {
  constexpr qsizetype limit = 200;
  return uri.size() > limit ? uri.left(limit) + QStringLiteral("…") : uri;
}

[[nodiscard]] RevealPlan refused(RevealError error, const QString &diagnostic) {
  return {error, diagnostic, {}};
}

// The local absolute path a file: URI names, or an empty string.
[[nodiscard]] QString localPathOf(const QString &uri) {
  const QUrl url(uri);
  const QString host = url.host();
  if (!url.isValid() || url.scheme() != QLatin1String("file") ||
      !(host.isEmpty() || host == QLatin1String("localhost")) || !url.userInfo().isEmpty() ||
      url.port() != -1 || url.hasQuery() || url.hasFragment()) {
    return {};
  }
  // AGENT-NOTE: path(), not toLocalFile(): toLocalFile() keeps a "localhost"
  // host as an SMB-style //localhost/... path.
  const QString path = url.path(QUrl::FullyDecoded);
  if (!path.startsWith(QLatin1Char('/')) || path.contains(QChar::Null)) {
    return {};
  }
  return path;
}

// Resolves `path` once to a canonical directory File Manager can list and
// enter -- the rule FileBoundary::openLocalFolder applies to a folder.
[[nodiscard]] RevealError resolveFolder(const QString &path, QString *canonical) {
  const QString resolved = QFileInfo(path).canonicalFilePath();
  if (resolved.isEmpty()) {
    return RevealError::NotFound;
  }
  const QFileInfo target(resolved);
  if (!target.isDir()) {
    return RevealError::NotDirectory;
  }
  if (!target.isReadable() || !target.isExecutable()) {
    return RevealError::Unreadable;
  }
  *canonical = resolved;
  return RevealError::None;
}

[[nodiscard]] QString folderDiagnostic(RevealError error, const QString &uri) {
  switch (error) {
  case RevealError::NotDirectory:
    return QStringLiteral("%1 is not a folder").arg(quoted(uri));
  case RevealError::Unreadable:
    return QStringLiteral("%1 cannot be opened").arg(quoted(uri));
  default:
    return QStringLiteral("%1 does not exist").arg(quoted(uri));
  }
}

} // namespace

bool isRevealableName(const QString &name) {
  return !name.isEmpty() && name != QLatin1String(".") && name != QLatin1String("..") &&
         !name.contains(QLatin1Char('/')) && !name.contains(QChar::Null);
}

RevealPlan planReveal(RevealKind kind, const QStringList &uris) {
  if (uris.size() > maximumRevealUris) {
    return refused(RevealError::TooMany,
                   QStringLiteral("At most %1 locations can be shown at once")
                       .arg(maximumRevealUris));
  }
  RevealPlan plan;
  for (const QString &uri : uris) {
    QString path = localPathOf(uri);
    if (path.isEmpty()) {
      return refused(RevealError::NotLocal,
                     QStringLiteral("%1 is not a local file URI").arg(quoted(uri)));
    }
    // An item is shown by selecting it in the folder that holds it. "/" has
    // no such folder, so it is shown as a folder instead.
    QString name;
    if (kind != RevealKind::Folders) {
      while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
      }
      if (path.size() > 1) {
        const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
        name = path.mid(slash + 1);
        path = slash == 0 ? QStringLiteral("/") : path.left(slash);
        if (!isRevealableName(name)) {
          return refused(RevealError::NotFound,
                         QStringLiteral("%1 does not name an entry").arg(quoted(uri)));
        }
      }
    }
    QString folder;
    const RevealError folderError = resolveFolder(path, &folder);
    if (folderError != RevealError::None) {
      return refused(folderError, folderDiagnostic(folderError, uri));
    }
    if (!name.isEmpty()) {
      // lstat semantics: a symbolic link is the entry, even when it dangles.
      const QFileInfo entry(QDir(folder).filePath(name));
      if (!entry.exists() && !entry.isSymLink()) {
        return refused(RevealError::NotFound,
                       QStringLiteral("%1 does not exist").arg(quoted(uri)));
      }
    }
    RevealRequest *request = nullptr;
    for (RevealRequest &candidate : plan.requests) {
      if (candidate.folder == folder) {
        request = &candidate;
        break;
      }
    }
    if (request == nullptr) {
      if (plan.requests.size() == maximumRevealWindows) {
        return refused(RevealError::TooMany,
                       QStringLiteral("At most %1 folders can be shown at once")
                           .arg(maximumRevealWindows));
      }
      plan.requests.append({folder, {}, kind == RevealKind::ItemProperties});
      request = &plan.requests.last();
    }
    if (!name.isEmpty() && !request->names.contains(name)) {
      request->names.append(name);
    }
  }
  return plan;
}

} // namespace QindaQt::Apps::FileManager
