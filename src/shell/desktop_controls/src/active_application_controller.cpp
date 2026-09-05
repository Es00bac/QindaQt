// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/active_application_controller.h"

#include "qindaqt/shell/task_list/applet/task_list_applet_controller.h"

namespace QindaQt::Shell::DesktopControls {

ActiveApplicationController::ActiveApplicationController(
    ShellTaskListApplet::TaskListAppletController *taskList,
    ActiveApplicationGrants grants, QObject *parent)
    : QObject(parent)
    , m_taskList(grants.windowsRead ? taskList : nullptr)
    , m_grants(grants)
{
  if (m_taskList != nullptr) {
    connect(m_taskList,
            &ShellTaskListApplet::TaskListAppletController::stateReprojected,
            this, &ActiveApplicationController::reproject);
    connect(m_taskList,
            &ShellTaskListApplet::TaskListAppletController::feedbackChanged,
            this, [this] {
              if (m_taskList->feedbackPresent()) {
                publishFeedback(m_taskList->feedback());
              }
            });
  }
  reproject();
}

bool ActiveApplicationController::available() const noexcept
{
  return m_taskList != nullptr;
}

QString ActiveApplicationController::phaseText() const
{
  if (m_taskList == nullptr) {
    return QStringLiteral("unavailable");
  }
  return m_taskList->phaseText();
}

QString ActiveApplicationController::title() const
{
  return m_row.value(QStringLiteral("title")).toString();
}

QString ActiveApplicationController::applicationName() const
{
  const QString name = m_row.value(QStringLiteral("applicationName")).toString();
  return name.isEmpty() ? m_row.value(QStringLiteral("applicationId")).toString()
                        : name;
}

QString ActiveApplicationController::applicationId() const
{
  return m_row.value(QStringLiteral("applicationId")).toString();
}

QString ActiveApplicationController::iconName() const
{
  return m_row.value(QStringLiteral("iconName")).toString();
}

bool ActiveApplicationController::iconResolved() const
{
  return m_row.value(QStringLiteral("iconResolved")).toBool();
}

QString ActiveApplicationController::iconText() const
{
  return m_row.value(QStringLiteral("iconText")).toString();
}

int ActiveApplicationController::windowCount() const
{
  return m_row.value(QStringLiteral("windowCount"), 0).toInt();
}

bool ActiveApplicationController::minimized() const
{
  return m_row.value(QStringLiteral("minimized")).toBool();
}

bool ActiveApplicationController::pending() const
{
  return m_row.value(QStringLiteral("pending")).toBool();
}

QString ActiveApplicationController::taskId() const
{
  return m_row.value(QStringLiteral("taskId")).toString();
}

quint64 ActiveApplicationController::revision() const
{
  return m_row.value(QStringLiteral("generationRevision")).toULongLong();
}

bool ActiveApplicationController::canManage() const
{
  return m_taskList != nullptr && m_grants.windowsManage && hasActiveWindow()
      && m_taskList->canManage() && !pending();
}

QString ActiveApplicationController::accessibleName() const
{
  if (!hasActiveWindow()) {
    return QStringLiteral("No active application");
  }
  return QStringLiteral("Active application: %1").arg(applicationName());
}

QString ActiveApplicationController::accessibleDescription() const
{
  if (m_taskList == nullptr) {
    return QStringLiteral("Window information is not granted");
  }
  if (!hasActiveWindow()) {
    return QStringLiteral("No window is focused");
  }
  const QString windowText = windowCount() == 1
      ? QStringLiteral("1 window")
      : QStringLiteral("%1 windows").arg(windowCount());
  return QStringLiteral("%1; %2; opens window actions").arg(title(), windowText);
}

bool ActiveApplicationController::dispatchAllowed(QString *message) const
{
  if (m_taskList == nullptr) {
    *message = QStringLiteral("Window information is not granted");
    return false;
  }
  if (!m_grants.windowsManage) {
    *message = QStringLiteral("Window management is not granted");
    return false;
  }
  if (!hasActiveWindow()) {
    *message = QStringLiteral("No window is focused");
    return false;
  }
  if (pending()) {
    *message = QStringLiteral("A window request is still pending");
    return false;
  }
  return true;
}

bool ActiveApplicationController::minimize()
{
  QString message;
  if (!dispatchAllowed(&message)) {
    publishFeedback(message);
    return false;
  }
  clearFeedback();
  return m_taskList->minimizeTask(taskId(), revision());
}

bool ActiveApplicationController::close()
{
  QString message;
  if (!dispatchAllowed(&message)) {
    publishFeedback(message);
    return false;
  }
  clearFeedback();
  return m_taskList->closeTask(taskId(), revision());
}

void ActiveApplicationController::clearFeedback()
{
  publishFeedback({});
}

void ActiveApplicationController::reproject()
{
  QVariantMap active;
  if (m_taskList != nullptr) {
    for (const QVariant &value : m_taskList->entryRows()) {
      const QVariantMap row = value.toMap();
      if (row.value(QStringLiteral("active")).toBool()) {
        active = row;
        break;
      }
    }
  }
  m_row = active;
  Q_EMIT stateChanged();
}

void ActiveApplicationController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
