// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_action_probe.h"

#include "model/calendar_controller.h"
#include "model/event_store.h"
#include "model/occurrence_list_model.h"

#include "qindaqt/app_shell/application_coordinator.h"

#include <KCalendarCore/Event>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QVariant>

#include <utility>

namespace QindaQt::Apps::Calendar {
namespace {

bool fail(QString *error, const QString &message) {
  if (error) {
    *error = message;
  }
  return false;
}

void pumpEvents() {
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
  QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

[[nodiscard]] QObject *requireObject(QObject *root, const QString &objectName,
                                     QString *error) {
  QObject *object = root->findChild<QObject *>(objectName);
  if (!object) {
    fail(error, QStringLiteral("missing UI object %1").arg(objectName));
    return nullptr;
  }
  return object;
}

// The occurrence model is exposed as an initial property on the QML root.
[[nodiscard]] OccurrenceListModel *occurrenceModelOf(QObject *root,
                                                     QString *error) {
  const QVariant modelProperty = root->property("occurrenceModel");
  auto *model = qvariant_cast<OccurrenceListModel *>(modelProperty);
  if (!model) {
    fail(error, QStringLiteral("occurrenceModel initial property missing"));
    return nullptr;
  }
  return model;
}

// Reloads the probe's disposable store from disk and returns the target
// calendar's events, failing with a step-tagged diagnostic on load errors.
[[nodiscard]] bool reloadStoredEvents(
    const QString &dataRoot, const QString &calendarId, const QString &step,
    KCalendarCore::Event::List *events, QString *error) {
  EventStore reloaded(dataRoot);
  const EventStoreLoadResult loadResult =
      reloaded.loadAll({{.id = calendarId,
                         .displayName = QStringLiteral("Personal"),
                         .colorToken = QStringLiteral("accent"),
                         .enabled = true}});
  if (!loadResult.ok()) {
    return fail(error, QStringLiteral("reload after %1 failed").arg(step));
  }
  *events = reloaded.events(calendarId);
  return true;
}

[[nodiscard]] bool checkViewAction(
    QindaQt::AppShell::ApplicationCoordinator *coordinator,
    CalendarController *controller, const QString &actionId,
    const QString &expectedMode, QString *error) {
  if (!coordinator->activateAction(actionId)) {
    return fail(error,
                QStringLiteral("could not activate %1").arg(actionId));
  }
  pumpEvents();
  if (controller->viewMode() != expectedMode) {
    return fail(error, QStringLiteral("%1 left viewMode %2")
                           .arg(actionId, controller->viewMode()));
  }
  for (const auto &[id, mode] :
       {std::pair{QStringLiteral("view.month"), QStringLiteral("month")},
        {QStringLiteral("view.week"), QStringLiteral("week")},
        {QStringLiteral("view.day"), QStringLiteral("day")}}) {
    bool found = false;
    bool checked = false;
    for (const auto &spec : coordinator->actionRegistry().actions()) {
      if (spec.id == id) {
        found = true;
        checked = spec.checked;
        break;
      }
    }
    if (!found || checked != (mode == expectedMode)) {
      return fail(error,
                  QStringLiteral("checked state of %1 not synced").arg(id));
    }
  }
  return true;
}

} // namespace

QString missingUiContractObject(QObject *qmlRoot) {
  const QStringList requiredObjects = {
      QStringLiteral("newEventButton"),     QStringLiteral("todayButton"),
      QStringLiteral("previousPeriodButton"), QStringLiteral("nextPeriodButton"),
      QStringLiteral("monthView"),          QStringLiteral("weekView"),
      QStringLiteral("dayView"),            QStringLiteral("calendarSidebar"),
      QStringLiteral("calendarList"),       QStringLiteral("newCalendarButton"),
      QStringLiteral("eventEditorDialog"),  QStringLiteral("importDialog"),
      QStringLiteral("exportDialog"),       QStringLiteral("reminderBanner"),
      QStringLiteral("eventDetailsPane"),   QStringLiteral("editEventButton"),
      QStringLiteral("deleteEventButton")};
  for (const QString &objectName : requiredObjects) {
    if (!qmlRoot->findChild<QObject *>(objectName)) {
      return objectName;
    }
  }
  return {};
}

bool verifyCalendarUiActions(
    QObject *qmlRoot, QindaQt::AppShell::ApplicationCoordinator *coordinator,
    CalendarController *controller, const QString &dataRoot,
    const QString &fixturesDir, QString *error) {
  // (a) The event.new action opens the production editor dialog; filling and
  // accepting it must persist the event into the default calendar's .ics.
  if (!coordinator->activateAction(QStringLiteral("event.new"))) {
    return fail(error, QStringLiteral("could not activate event.new"));
  }
  pumpEvents();
  QObject *dialog = requireObject(qmlRoot, QStringLiteral("eventEditorDialog"), error);
  if (!dialog) {
    return false;
  }
  if (!dialog->property("visible").toBool()) {
    return fail(error, QStringLiteral("event.new did not open eventEditorDialog"));
  }
  if (!dialog->setProperty("summaryText", QStringLiteral("Probe Event"))) {
    return fail(error, QStringLiteral("eventEditorDialog has no summaryText"));
  }
  if (!QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not accept eventEditorDialog"));
  }
  pumpEvents();

  {
    KCalendarCore::Event::List events;
    if (!reloadStoredEvents(dataRoot, controller->defaultCalendarId(),
                            QStringLiteral("create"), &events, error)) {
      return false;
    }
    if (events.size() != 1 ||
        events.constFirst()->summary() != QLatin1String("Probe Event")) {
      return fail(error,
                  QStringLiteral("created event was not persisted to .ics"));
    }
  }

  // (a2) Editing through the production dialog is a uid-stable in-place
  // update: the summary change plus a weekly recurrence and a 10-minute
  // reminder must persist with the RFC 5545 revision bumped to 1.
  OccurrenceListModel *model = occurrenceModelOf(qmlRoot, error);
  if (!model) {
    return false;
  }
  if (model->rowCount({}) != 1) {
    return fail(error, QStringLiteral("occurrence model does not show the new event"));
  }
  const QString createdUid =
      model->data(model->index(0, 0), OccurrenceListModel::EventUidRole)
          .toString();
  controller->selectEvent(createdUid);
  pumpEvents();
  if (!QMetaObject::invokeMethod(dialog, "openForEdit", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not open eventEditorDialog for edit"));
  }
  pumpEvents();
  if (!dialog->property("visible").toBool()) {
    return fail(error, QStringLiteral("openForEdit did not open eventEditorDialog"));
  }
  if (dialog->property("summaryText").toString() !=
      QLatin1String("Probe Event")) {
    return fail(error, QStringLiteral("editor did not load the stored summary"));
  }
  if (!dialog->setProperty("summaryText", QStringLiteral("Edited Probe Event")) ||
      !dialog->setProperty("recurrenceRule", QStringLiteral("weekly")) ||
      !dialog->setProperty("reminderMinutes", 10)) {
    return fail(error, QStringLiteral("could not set edit fields"));
  }
  if (!QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not accept the edit"));
  }
  pumpEvents();
  {
    KCalendarCore::Event::List events;
    if (!reloadStoredEvents(dataRoot, controller->defaultCalendarId(),
                            QStringLiteral("edit"), &events, error)) {
      return false;
    }
    if (events.size() != 1) {
      return fail(error, QStringLiteral("edit changed the event count"));
    }
    const KCalendarCore::Event::Ptr &edited = events.constFirst();
    if (edited->uid() != createdUid) {
      return fail(error, QStringLiteral("edit changed the event uid"));
    }
    if (edited->summary() != QLatin1String("Edited Probe Event")) {
      return fail(error, QStringLiteral("edited summary was not persisted"));
    }
    if (edited->revision() != 1) {
      return fail(error, QStringLiteral("edit did not bump the RFC 5545 revision"));
    }
    if (!edited->recurs()) {
      return fail(error, QStringLiteral("recurrence edit did not persist"));
    }
    if (edited->alarms().size() != 1) {
      return fail(error, QStringLiteral("reminder edit did not persist"));
    }
  }

  // (a3) Clearing recurrence and the reminder through the editor persists as
  // a second uid-stable update.
  if (!QMetaObject::invokeMethod(dialog, "openForEdit", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not reopen eventEditorDialog"));
  }
  pumpEvents();
  if (!dialog->setProperty("recurrenceRule", QString()) ||
      !dialog->setProperty("reminderMinutes", -1)) {
    return fail(error, QStringLiteral("could not clear edit fields"));
  }
  if (!QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection)) {
    return fail(error, QStringLiteral("could not accept the second edit"));
  }
  pumpEvents();
  {
    KCalendarCore::Event::List events;
    if (!reloadStoredEvents(dataRoot, controller->defaultCalendarId(),
                            QStringLiteral("clearing edit"), &events, error)) {
      return false;
    }
    if (events.size() != 1 ||
        events.constFirst()->uid() != createdUid ||
        events.constFirst()->revision() != 2 ||
        events.constFirst()->recurs() ||
        !events.constFirst()->alarms().isEmpty()) {
      return fail(error,
                  QStringLiteral("clearing recurrence/reminder did not persist"));
    }
  }

  // (b) The occurrence is visible in the model for the current range.
  if (model->rowCount({}) != 1) {
    return fail(error, QStringLiteral("occurrence model does not show the edited event"));
  }

  // (c) View actions drive the controller and coordinator checked states.
  if (!checkViewAction(coordinator, controller, QStringLiteral("view.week"),
                       QStringLiteral("week"), error) ||
      !checkViewAction(coordinator, controller, QStringLiteral("view.day"),
                       QStringLiteral("day"), error) ||
      !checkViewAction(coordinator, controller, QStringLiteral("view.month"),
                       QStringLiteral("month"), error)) {
    return false;
  }

  // (d) Deleting through the production action removes the event from disk.
  controller->selectEvent(createdUid);
  if (!coordinator->activateAction(QStringLiteral("event.delete"))) {
    return fail(error, QStringLiteral("could not activate event.delete"));
  }
  pumpEvents();
  {
    KCalendarCore::Event::List events;
    if (!reloadStoredEvents(dataRoot, controller->defaultCalendarId(),
                            QStringLiteral("delete"), &events, error)) {
      return false;
    }
    if (!events.isEmpty()) {
      return fail(error, QStringLiteral("deleted event is still persisted"));
    }
  }

  // (e) Importing the Google fixture brings its events in.
  const QVariantMap importResult = controller->importIcs(
      QDir(fixturesDir).filePath(QStringLiteral("google_basic.ics")),
      QStringLiteral("personal"));
  if (!importResult.value(QStringLiteral("ok")).toBool() ||
      importResult.value(QStringLiteral("imported")).toInt() <= 0) {
    return fail(error,
                QStringLiteral("fixture import failed: %1")
                    .arg(importResult.value(QStringLiteral("error"))
                             .toString()));
  }
  pumpEvents();
  if (model->rowCount({}) <= 0) {
    return fail(error,
                QStringLiteral("imported fixture produced no occurrences"));
  }
  return true;
}

} // namespace QindaQt::Apps::Calendar
