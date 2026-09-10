// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/calendar_controller.h"
#include "model/event_store.h"
#include "model/occurrence_list_model.h"
#include "settings/calendar_preferences.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Duration>
#include <KCalendarCore/Event>
#include <KCalendarCore/Recurrence>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::Calendar;

namespace {

// The preferences client degrades to schema defaults without a session bus;
// that path logs once at qInfo level and never warns.
struct ControllerFixture {
  QTemporaryDir directory;
  CalendarPreferences preferences;
  OccurrenceListModel occurrenceModel;
  std::unique_ptr<CalendarController> controller;

  ControllerFixture() {
    controller = std::make_unique<CalendarController>(
        directory.filePath(QStringLiteral("calendar")), &preferences,
        &occurrenceModel);
  }

  [[nodiscard]] QString dataRoot() const {
    return directory.filePath(QStringLiteral("calendar"));
  }

  [[nodiscard]] KCalendarCore::Event::Ptr reloadOnlyEvent() const {
    EventStore reloaded(dataRoot());
    const EventStoreLoadResult loadResult =
        reloaded.loadAll({{.id = QStringLiteral("personal"),
                           .displayName = QStringLiteral("Personal"),
                           .colorToken = QStringLiteral("accent"),
                           .enabled = true}});
    if (!loadResult.ok()) {
      return {};
    }
    const auto events = reloaded.events(QStringLiteral("personal"));
    return events.size() == 1 ? events.constFirst() : nullptr;
  }
};

} // namespace

class TestEventUpdate final : public QObject {
  Q_OBJECT

private slots:
  void updatePreservesUidAndBumpsRevision();
  void updateRecurrenceAndReminderPersistAndClear();
  void updateRejectsInvalidInput();
  void selectedEventReflectsStoredFields();
};

void TestEventUpdate::updatePreservesUidAndBumpsRevision() {
  ControllerFixture fixture;
  QVERIFY2(fixture.controller->createEvent(
               QStringLiteral("personal"), QStringLiteral("Standup"),
               QStringLiteral("2026-09-10T10:00:00"),
               QStringLiteral("2026-09-10T10:30:00"), false, QString(),
               QString(), QString(), -1),
           "create failed");
  const KCalendarCore::Event::Ptr created = fixture.reloadOnlyEvent();
  QVERIFY(created);
  QCOMPARE(created->revision(), 0);

  QVERIFY2(fixture.controller->updateEvent(
               created->uid(), QStringLiteral("Standup v2"),
               QStringLiteral("2026-09-10T11:00:00"),
               QStringLiteral("2026-09-10T11:30:00"), false,
               QStringLiteral("Room 4"), QStringLiteral("Notes"), QString(),
               -1),
           "update failed");

  const KCalendarCore::Event::Ptr updated = fixture.reloadOnlyEvent();
  QVERIFY(updated);
  QCOMPARE(updated->uid(), created->uid());
  QCOMPARE(updated->summary(), QStringLiteral("Standup v2"));
  QCOMPARE(updated->location(), QStringLiteral("Room 4"));
  QCOMPARE(updated->description(), QStringLiteral("Notes"));
  QCOMPARE(updated->dtStart(),
           QDateTime::fromString(QStringLiteral("2026-09-10T11:00:00"),
                                 Qt::ISODate));
  QCOMPARE(updated->revision(), 1);
  QVERIFY(updated->lastModified().isValid());
}

void TestEventUpdate::updateRecurrenceAndReminderPersistAndClear() {
  ControllerFixture fixture;
  QVERIFY(fixture.controller->createEvent(
      QStringLiteral("personal"), QStringLiteral("Review"),
      QStringLiteral("2026-09-10T14:00:00"),
      QStringLiteral("2026-09-10T15:00:00"), false, QString(), QString(),
      QString(), -1));
  const KCalendarCore::Event::Ptr created = fixture.reloadOnlyEvent();
  QVERIFY(created);

  QVERIFY(fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Review"),
      QStringLiteral("2026-09-10T14:00:00"),
      QStringLiteral("2026-09-10T15:00:00"), false, QString(), QString(),
      QStringLiteral("weekly"), 10));
  KCalendarCore::Event::Ptr updated = fixture.reloadOnlyEvent();
  QVERIFY(updated);
  QCOMPARE(updated->uid(), created->uid());
  QCOMPARE(updated->revision(), 1);
  QVERIFY(updated->recurs());
  QCOMPARE(updated->recurrence()->recurrenceType(),
           KCalendarCore::Recurrence::rWeekly);
  QCOMPARE(updated->alarms().size(), 1);
  QCOMPARE(updated->alarms().constFirst()->startOffset().asSeconds(), -600);

  // Clearing recurrence and the reminder is another uid-stable update.
  QVERIFY(fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Review"),
      QStringLiteral("2026-09-10T14:00:00"),
      QStringLiteral("2026-09-10T15:00:00"), false, QString(), QString(),
      QString(), -1));
  updated = fixture.reloadOnlyEvent();
  QVERIFY(updated);
  QCOMPARE(updated->uid(), created->uid());
  QCOMPARE(updated->revision(), 2);
  QVERIFY(!updated->recurs());
  QVERIFY(updated->alarms().isEmpty());
}

void TestEventUpdate::updateRejectsInvalidInput() {
  ControllerFixture fixture;
  QVERIFY(fixture.controller->createEvent(
      QStringLiteral("personal"), QStringLiteral("Standup"),
      QStringLiteral("2026-09-10T10:00:00"),
      QStringLiteral("2026-09-10T10:30:00"), false, QString(), QString(),
      QString(), -1));
  const KCalendarCore::Event::Ptr created = fixture.reloadOnlyEvent();
  QVERIFY(created);

  QSignalSpy failed(fixture.controller.get(),
                    &CalendarController::operationFailed);
  QVERIFY(!fixture.controller->updateEvent(
      QStringLiteral("missing-uid"), QStringLiteral("Ghost"),
      QStringLiteral("2026-09-10T10:00:00"),
      QStringLiteral("2026-09-10T10:30:00"), false, QString(), QString(),
      QString(), -1));
  QVERIFY(!fixture.controller->updateEvent(
      created->uid(), QStringLiteral("  "),
      QStringLiteral("2026-09-10T10:00:00"),
      QStringLiteral("2026-09-10T10:30:00"), false, QString(), QString(),
      QString(), -1));
  QVERIFY(!fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Standup"),
      QStringLiteral("2026-09-10T12:00:00"),
      QStringLiteral("2026-09-10T10:00:00"), false, QString(), QString(),
      QString(), -1));
  QVERIFY(!fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Standup"),
      QStringLiteral("not-a-date"), QStringLiteral("2026-09-10T10:30:00"),
      false, QString(), QString(), QString(), -1));
  QVERIFY(!fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Standup"),
      QStringLiteral("2026-09-10T10:00:00"),
      QStringLiteral("2026-09-10T10:30:00"), false, QString(), QString(),
      QStringLiteral("hourly"), -1));
  QCOMPARE(failed.size(), 5);

  // Rejections left the stored event untouched.
  const KCalendarCore::Event::Ptr stored = fixture.reloadOnlyEvent();
  QVERIFY(stored);
  QCOMPARE(stored->revision(), 0);
  QCOMPARE(stored->summary(), QStringLiteral("Standup"));
}

void TestEventUpdate::selectedEventReflectsStoredFields() {
  ControllerFixture fixture;
  QCOMPARE(fixture.controller->selectedEvent(), QVariantMap{});

  QVERIFY(fixture.controller->createEvent(
      QStringLiteral("personal"), QStringLiteral("Day off"),
      QStringLiteral("2026-09-10T00:00:00"),
      QStringLiteral("2026-09-10T00:00:00"), true, QStringLiteral("Home"),
      QStringLiteral("Rest"), QStringLiteral("yearly"), 30));
  const KCalendarCore::Event::Ptr created = fixture.reloadOnlyEvent();
  QVERIFY(created);

  fixture.controller->selectEvent(created->uid());
  const QVariantMap details = fixture.controller->selectedEvent();
  QCOMPARE(details.value(QStringLiteral("uid")).toString(), created->uid());
  QCOMPARE(details.value(QStringLiteral("calendarId")).toString(),
           QStringLiteral("personal"));
  QCOMPARE(details.value(QStringLiteral("calendarName")).toString(),
           QStringLiteral("Personal"));
  QCOMPARE(details.value(QStringLiteral("summary")).toString(),
           QStringLiteral("Day off"));
  QCOMPARE(details.value(QStringLiteral("allDay")).toBool(), true);
  // All-day end is the inclusive date; the editor ISO text round-trips it.
  QCOMPARE(details.value(QStringLiteral("startIso")).toString(),
           QStringLiteral("2026-09-10T00:00"));
  QCOMPARE(details.value(QStringLiteral("endIso")).toString(),
           QStringLiteral("2026-09-10T00:00"));
  QCOMPARE(details.value(QStringLiteral("recurrenceRule")).toString(),
           QStringLiteral("yearly"));
  QCOMPARE(details.value(QStringLiteral("recurring")).toBool(), true);
  QCOMPARE(details.value(QStringLiteral("reminderMinutes")).toInt(), 30);
  QCOMPARE(details.value(QStringLiteral("location")).toString(),
           QStringLiteral("Home"));
  QCOMPARE(details.value(QStringLiteral("description")).toString(),
           QStringLiteral("Rest"));

  // An edit is visible through the same map without re-selecting.
  QVERIFY(fixture.controller->updateEvent(
      created->uid(), QStringLiteral("Conference"),
      QStringLiteral("2026-09-10T09:00:00"),
      QStringLiteral("2026-09-10T17:00:00"), false, QString(), QString(),
      QStringLiteral("daily"), 5));
  const QVariantMap edited = fixture.controller->selectedEvent();
  QCOMPARE(edited.value(QStringLiteral("summary")).toString(),
           QStringLiteral("Conference"));
  QCOMPARE(edited.value(QStringLiteral("recurrenceRule")).toString(),
           QStringLiteral("daily"));
  QCOMPARE(edited.value(QStringLiteral("reminderMinutes")).toInt(), 5);
  QCOMPARE(edited.value(QStringLiteral("revision")).toInt(), 1);

  // Deleting the selection clears the map.
  QVERIFY(fixture.controller->deleteEvent(created->uid()));
  QCOMPARE(fixture.controller->selectedEvent(), QVariantMap{});
}

QTEST_GUILESS_MAIN(TestEventUpdate)
#include "tst_event_update.moc"
