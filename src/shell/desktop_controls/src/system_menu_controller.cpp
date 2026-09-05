// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"

#include "launcher_applet_controller.h"

#include <utility>

namespace QindaQt::Shell::DesktopControls {

QString SystemMenuController::defaultSettingsEntryId()
{
  return QStringLiteral("org.qindaqt.Settings");
}

SystemMenuController::SystemMenuController(
    QObject *sessionActions, Launcher::LauncherAppletController *launcher,
    bool applicationsLaunchGranted, QString settingsEntryId, QString versionText,
    QObject *parent)
    : QObject(parent)
    , m_sessionActions(sessionActions)
    , m_launcher(launcher)
    , m_launchGranted(applicationsLaunchGranted)
    , m_settingsEntryId(std::move(settingsEntryId))
    , m_versionText(std::move(versionText))
{
  if (m_launcher != nullptr) {
    connect(m_launcher, &Launcher::LauncherAppletController::stateChanged, this,
            &SystemMenuController::stateChanged);
  }
}

bool SystemMenuController::canOpenSettings() const
{
  return m_launchGranted && m_launcher != nullptr && m_launcher->launchGranted()
      && !m_settingsEntryId.isEmpty();
}

bool SystemMenuController::openSettings()
{
  if (!m_launchGranted) {
    publishFeedback(QStringLiteral("Opening System Settings is not granted"));
    return false;
  }
  if (m_launcher == nullptr) {
    publishFeedback(QStringLiteral("Application launching is unavailable"));
    return false;
  }
  if (!m_launcher->activate(m_settingsEntryId)) {
    const QString detail = m_launcher->feedback();
    publishFeedback(detail.isEmpty()
                        ? QStringLiteral("Could not open System Settings")
                        : QStringLiteral("Could not open System Settings: %1").arg(detail));
    return false;
  }
  clearFeedback();
  return true;
}

void SystemMenuController::clearFeedback()
{
  publishFeedback({});
}

void SystemMenuController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
