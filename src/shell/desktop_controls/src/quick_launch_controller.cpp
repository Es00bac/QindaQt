// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/quick_launch_controller.h"

#include "launcher_applet_controller.h"

#include <QVariantMap>

namespace QindaQt::Shell::DesktopControls {

QuickLaunchController::QuickLaunchController(
    Launcher::LauncherAppletController *launcher, bool applicationsLaunchGranted,
    QObject *parent)
    : QObject(parent), m_launcher(launcher), m_launchGranted(applicationsLaunchGranted)
{
  if (m_launcher != nullptr) {
    connect(m_launcher, &Launcher::LauncherAppletController::stateChanged, this,
            &QuickLaunchController::rebuild);
  }
  rebuild();
}

bool QuickLaunchController::available() const noexcept
{
  return m_launcher != nullptr && m_launchGranted;
}

QString QuickLaunchController::phaseText() const
{
  if (m_launcher == nullptr) {
    return QStringLiteral("unavailable");
  }
  const QString phase = m_launcher->phase();
  if (phase == QLatin1StringView("ready") && m_rows.isEmpty()) {
    return QStringLiteral("empty");
  }
  return phase;
}

void QuickLaunchController::rebuild()
{
  QVariantList rows;
  if (m_launcher != nullptr) {
    // AGENT-NOTE: sectionsForQuery("") is the launcher's query-independent
    // browse projection. Reading `sections` instead would empty this strip
    // whenever the user types in the launcher popup, because the live
    // projection collapses to search results.
    const QVariantList sections = m_launcher->sectionsForQuery(QString{});
    for (const QVariant &sectionValue : sections) {
      const QVariantMap section = sectionValue.toMap();
      if (section.value(QStringLiteral("kind")).toString()
          != QLatin1StringView("pinned")) {
        continue;
      }
      int index = 0;
      for (const QVariant &itemValue : section.value(QStringLiteral("items")).toList()) {
        const QVariantMap item = itemValue.toMap();
        const QString displayText = item.value(QStringLiteral("displayText")).toString();
        rows.append(QVariantMap{
            {QStringLiteral("entryId"), item.value(QStringLiteral("entryId"))},
            {QStringLiteral("displayText"), displayText},
            {QStringLiteral("iconName"), item.value(QStringLiteral("iconName"))},
            {QStringLiteral("accessibleName"), displayText},
            {QStringLiteral("accessibleDescription"),
             item.value(QStringLiteral("accessibleDescription"))},
            {QStringLiteral("index"), index},
        });
        ++index;
      }
      break;
    }
  }
  m_rows = rows;
  Q_EMIT stateChanged();
}

bool QuickLaunchController::knownEntry(const QString &entryId) const
{
  for (const QVariant &row : m_rows) {
    if (row.toMap().value(QStringLiteral("entryId")).toString() == entryId) {
      return true;
    }
  }
  return false;
}

bool QuickLaunchController::activate(const QString &entryId)
{
  if (!m_launchGranted) {
    publishFeedback(QStringLiteral("Application launching is not granted"));
    return false;
  }
  if (m_launcher == nullptr) {
    publishFeedback(QStringLiteral("Application launching is unavailable"));
    return false;
  }
  if (!knownEntry(entryId)) {
    publishFeedback(QStringLiteral("That application is no longer pinned"));
    return false;
  }
  if (!m_launcher->activate(entryId)) {
    const QString detail = m_launcher->feedback();
    publishFeedback(detail.isEmpty() ? QStringLiteral("Could not start the application")
                                     : detail);
    return false;
  }
  clearFeedback();
  return true;
}

bool QuickLaunchController::unpin(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId) && m_launcher->unpin(entryId);
}

bool QuickLaunchController::moveUp(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId)
      && m_launcher->movePinnedUp(entryId);
}

bool QuickLaunchController::moveDown(const QString &entryId)
{
  return m_launcher != nullptr && knownEntry(entryId)
      && m_launcher->movePinnedDown(entryId);
}

void QuickLaunchController::clearFeedback()
{
  publishFeedback({});
}

void QuickLaunchController::publishFeedback(const QString &message)
{
  if (m_feedback == message) {
    return;
  }
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::DesktopControls
