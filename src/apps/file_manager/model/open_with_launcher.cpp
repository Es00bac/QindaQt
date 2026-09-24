// SPDX-License-Identifier: GPL-3.0-or-later
#include "open_with_launcher.h"

#include "qindaqt/application_catalog/launch_support.h"

#include <QProcess>

#include <utility>

namespace QindaQt::Apps::FileManager {
namespace {

using QindaQt::ApplicationCatalog::LaunchPreparation;
using QindaQt::ApplicationCatalog::LaunchSupport;
using QindaQt::ApplicationCatalog::ScannedApplication;

[[nodiscard]] LaunchResult refused(const QString &message) {
  return {LaunchError::LaunchFailed, message};
}

[[nodiscard]] LaunchPreparation plan(const ScannedApplication &application,
                                     const QStringList &files) {
  // Plans from the exact validated bytes the scan retained (ADR-0164).
  return QindaQt::ApplicationCatalog::planApplicationLaunch(
      application.documentText, QString(), application.entry.name,
      application.desktopFilePath, files);
}

} // namespace

OpenWithLauncher::OpenWithLauncher(DetachedStarter start, QStringList terminalCommandPrefix)
    : m_start(std::move(start)), m_terminalPrefix(std::move(terminalCommandPrefix)) {}

DetachedStarter OpenWithLauncher::processStarter() {
  return [](const QString &program, const QStringList &arguments) {
    return QProcess::startDetached(program, arguments);
  };
}

QStringList OpenWithLauncher::desktopTerminalPrefix() {
  // AGENT-CONTRACT: the same prefix the shell launcher routes Terminal=true
  // entries through (shellruntimeapplication_applets.cpp): QQ_Term's
  // `-e PROGRAM ARG...` runs the command verbatim, never through a shell.
  return {QStringLiteral("qqterm"), QStringLiteral("-e")};
}

LaunchResult OpenWithLauncher::launch(const ScannedApplication &application,
                                      const QStringList &absolutePaths) const {
  if (absolutePaths.isEmpty() || absolutePaths.size() > maximumFiles) {
    return refused(QStringLiteral("Open between 1 and %1 files at a time").arg(maximumFiles));
  }
  QStringList files;
  for (const QString &path : absolutePaths) {
    QString canonical;
    const LaunchResult validation = DesktopFileLauncher::validateRegularFile(path, &canonical);
    if (!validation.ok()) {
      return validation;
    }
    files.append(canonical);
  }
  const LaunchPreparation all = plan(application, files);
  QList<LaunchPreparation> launches{all};
  if (all.spawnable() && all.fileArguments == 0) {
    return refused(QStringLiteral("%1 does not open files").arg(application.entry.name));
  }
  if (all.spawnable() && all.fileArguments < files.size()) {
    // The specification's rule for %f/%u: one process per file.
    launches.clear();
    for (const QString &file : std::as_const(files)) {
      launches.append(plan(application, {file}));
    }
  }
  for (const LaunchPreparation &preparation : std::as_const(launches)) {
    if (preparation.support == LaunchSupport::DbusActivatable) {
      return refused(QStringLiteral("%1 starts through D-Bus activation, which Open With "
                                    "cannot use yet").arg(application.entry.name));
    }
    if (!preparation.spawnable()) {
      return refused(preparation.message.isEmpty()
                         ? QStringLiteral("%1 cannot be started").arg(application.entry.name)
                         : preparation.message);
    }
  }
  for (const LaunchPreparation &preparation : std::as_const(launches)) {
    QString program = preparation.program;
    QStringList arguments = preparation.arguments;
    if (preparation.support == LaunchSupport::TerminalRequired) {
      const QStringList command = QindaQt::ApplicationCatalog::terminalCommandLine(
          m_terminalPrefix, program, arguments);
      if (command.isEmpty()) {
        return refused(QStringLiteral("%1 needs a terminal").arg(application.entry.name));
      }
      program = command.constFirst();
      arguments = command.mid(1);
    }
    // AGENT-GUARD: one detached start per planned launch; the started
    // process outlives the window, so nothing after this is reportable.
    if (!m_start || !m_start(program, arguments)) {
      return refused(QStringLiteral("Could not start %1").arg(application.entry.name));
    }
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
