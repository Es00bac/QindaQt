// SPDX-License-Identifier: GPL-3.0-or-later
#include "launch_intent.h"

#include <qindaqt/windows_runner/executable_open.h>

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>

#include <utility>

namespace QindaQt::Apps::FileManager {

DesktopFileLauncher::DesktopFileLauncher(
    QindaQt::WindowsRunner::WindowsExecutableOpenPort *windows,
    DefaultHandler defaultHandler)
    : m_windows(windows), m_defaultHandler(std::move(defaultHandler)) {}

LaunchResult windowsExecutableLaunchResult(
    const QindaQt::WindowsRunner::WindowsExecutableOpenResult &result) {
  using QindaQt::WindowsRunner::ExecutableOpenDisposition;
  switch (result.disposition) {
  case ExecutableOpenDisposition::Starting:
    return {LaunchError::None, result.message, LaunchProgress::Starting};
  case ExecutableOpenDisposition::SetupOpened:
    return {LaunchError::None, result.message, LaunchProgress::SetupOpened};
  case ExecutableOpenDisposition::OutcomeUnknown:
    return {LaunchError::LaunchFailed, result.message, LaunchProgress::OutcomeUnknown};
  case ExecutableOpenDisposition::UseDesktopHandler:
  case ExecutableOpenDisposition::Failed:
    return {LaunchError::LaunchFailed, result.message};
  }
  return {LaunchError::LaunchFailed, QStringLiteral("Windows executable opening is unavailable.")};
}

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

  if (m_windows) {
    const auto routed = QindaQt::WindowsRunner::routeWindowsExecutableOpen(
        canonicalPath, {}, *m_windows);
    if (routed.disposition
        != QindaQt::WindowsRunner::ExecutableOpenDisposition::UseDesktopHandler) {
      return windowsExecutableLaunchResult(routed);
    }
  }

  // AGENT-NOTE: QDesktopServices::openUrl only confirms the request was
  // accepted by the desktop's handler dispatch, not that the launched
  // application itself succeeded. A bounded launch intent stops at that
  // documented boundary; per-application success/failure is not observable
  // from here.
  const bool accepted = m_defaultHandler
      ? m_defaultHandler(canonicalPath)
      : QDesktopServices::openUrl(QUrl::fromLocalFile(canonicalPath));
  if (!accepted) {
    return {LaunchError::LaunchFailed,
            QStringLiteral("No application handled %1").arg(absolutePath)};
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
