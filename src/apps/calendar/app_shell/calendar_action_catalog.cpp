// SPDX-License-Identifier: GPL-3.0-or-later
#include "calendar_action_catalog.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QDBusConnection>
#include <QKeySequence>
#include <QPointer>
#include <QWindow>

namespace QindaQt::Apps::Calendar {
namespace {

[[nodiscard]] QindaQt::AppShell::ActionSpec action(
    const QString &id, const QString &menuId, const QString &menuLabel,
    const QString &label, const QString &description,
    const QKeySequence &shortcut, int menuOrder, int order,
    bool destructive = false, bool checkable = false) {
  return {.id = id,
          .menuId = menuId,
          .menuLabel = menuLabel,
          .label = label,
          .accessibleDescription = description,
          .shortcut = shortcut,
          .menuOrder = menuOrder,
          .order = order,
          .enabled = true,
          .checkable = checkable,
          .checked = false,
          .destructive = destructive};
}

} // namespace

QList<QindaQt::AppShell::ActionSpec> calendarActionCatalog() {
  return {
      action(QStringLiteral("event.new"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("New Event"),
             QStringLiteral("Create an event in the default calendar"),
             QKeySequence(QStringLiteral("Ctrl+N")), 0, 0),
      action(QStringLiteral("file.import-ics"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Import Calendar…"),
             QStringLiteral("Import events from an iCalendar file"),
             QKeySequence(QStringLiteral("Ctrl+I")), 0, 1),
      action(QStringLiteral("file.export-ics"), QStringLiteral("file"),
             QStringLiteral("File"), QStringLiteral("Export Calendar…"),
             QStringLiteral("Export a calendar to an iCalendar file"),
             QKeySequence(QStringLiteral("Ctrl+Shift+E")), 0, 2),
      action(QStringLiteral("event.delete"), QStringLiteral("edit"),
             QStringLiteral("Edit"), QStringLiteral("Delete Event"),
             QStringLiteral("Delete the selected event from its calendar"),
             QKeySequence(QStringLiteral("Delete")), 1, 0, true),
      action(QStringLiteral("view.month"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Month View"),
             QStringLiteral("Show one month of events"),
             QKeySequence(QStringLiteral("Ctrl+1")), 2, 0, false, true),
      action(QStringLiteral("view.week"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Week View"),
             QStringLiteral("Show one week of events"),
             QKeySequence(QStringLiteral("Ctrl+2")), 2, 1, false, true),
      action(QStringLiteral("view.day"), QStringLiteral("view"),
             QStringLiteral("View"), QStringLiteral("Day View"),
             QStringLiteral("Show one day of events"),
             QKeySequence(QStringLiteral("Ctrl+3")), 2, 2, false, true),
      action(QStringLiteral("go.today"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Today"),
             QStringLiteral("Return the view to the current date"),
             QKeySequence(QStringLiteral("Ctrl+T")), 3, 0),
      action(QStringLiteral("go.previous-period"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Previous Period"),
             QStringLiteral("Show the previous month, week, or day"),
             QKeySequence(QStringLiteral("Ctrl+PageUp")), 3, 1),
      action(QStringLiteral("go.next-period"), QStringLiteral("go"),
             QStringLiteral("Go"), QStringLiteral("Next Period"),
             QStringLiteral("Show the next month, week, or day"),
             QKeySequence(QStringLiteral("Ctrl+PageDown")), 3, 2),
      action(QStringLiteral("calendar.new"), QStringLiteral("calendar"),
             QStringLiteral("Calendar"), QStringLiteral("New Calendar…"),
             QStringLiteral("Create a local calendar"),
             QKeySequence(QStringLiteral("Ctrl+Shift+N")), 4, 0),
  };
}

std::unique_ptr<QObject> composeCalendarMenuExport(
    QindaQt::AppShell::ApplicationCoordinator &coordinator, QObject *qmlRoot) {
  auto *window = qobject_cast<QWindow *>(qmlRoot);
  if (window == nullptr) {
    return {};
  }
  // AGENT-CONTRACT: one shared first-party composition entry for File
  // Manager, Terminal, Text Editor, and Calendar; its fail-closed, lifecycle,
  // and test-seam behavior is owned by src/app_shell/menu_export.
  return QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
      coordinator, *window, QDBusConnection::sessionBus(),
      [root = QPointer<QObject>(qmlRoot)](bool visible) {
        if (root) {
          root->setProperty("inWindowMenuVisible", visible);
        }
      });
}

} // namespace QindaQt::Apps::Calendar
