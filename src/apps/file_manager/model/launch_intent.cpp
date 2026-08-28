// SPDX-License-Identifier: GPL-3.0-or-later
#include "launch_intent.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

namespace QindaQt::Apps::FileManager {

LaunchResult DesktopFileLauncher::validateRegularFile(const QString &absolutePath,
                                                      QString *canonicalPath) {
  QFileInfo info(absolutePath);
  if (!info.exists()) {
    return {LaunchError::NotFound,
            QStringLiteral("%1 does not exist").arg(absolutePath)};
  }
  if (info.isSymLink()) {
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty()) {
      return {LaunchError::NotFound,
              QStringLiteral("%1 is a broken link").arg(absolutePath)};
    }
    info = QFileInfo(canonical);
  }
  if (!info.isFile()) {
    return {LaunchError::NotRegularFile,
            QStringLiteral("%1 is not a file").arg(absolutePath)};
  }
  if (!info.isReadable()) {
    return {LaunchError::Unreadable,
            QStringLiteral("%1 cannot be read").arg(absolutePath)};
  }
  if (canonicalPath) {
    *canonicalPath = info.absoluteFilePath();
  }
  return {};
}

LaunchResult DesktopFileLauncher::launch(const QString &absolutePath) const {
  QString canonicalPath;
  const LaunchResult validation = validateRegularFile(absolutePath, &canonicalPath);
  if (!validation.ok()) {
    return validation;
  }

  // AGENT-NOTE: QDesktopServices::openUrl only confirms the request was
  // accepted by the desktop's handler dispatch, not that the launched
  // application itself succeeded. A bounded launch intent stops at that
  // documented boundary; per-application success/failure is not observable
  // from here.
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(canonicalPath))) {
    return {LaunchError::LaunchFailed,
            QStringLiteral("No application handled %1").arg(absolutePath)};
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
