// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/event_store.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Duration>
#include <KCalendarCore/Recurrence>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace QindaQt::Apps::Calendar;

namespace {

[[nodiscard]] CalendarInfo personalInfo() {
  return {.id = QStringLiteral("personal"),
          .displayName = QStringLiteral("Personal"),
          .colorToken = QStringLiteral("accent"),
          .enabled = true};
}

[[nodiscard]] KCalendarCore::Event::Ptr makeEvent(const QString &uid,
                                                  const QString &summary,
                                                  const QDateTime &start,
                                                  const QDateTime &end) {
  KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
  event->setUid(uid);
  event->setSummary(summary);
  event->setDtStart(start);
  event->setDtEnd(end);
  return event;
}

[[nodiscard]] KCalendarCore::Event::Ptr findByUid(
    const KCalendarCore::Event::List &events, const QString &uid) {
  for (const KCalendarCore::Event::Ptr &event : events) {
    if (event && event->uid() == uid) {
      return event;
    }
  }
  return {};
}

} // namespace

class TestEventStore final : public QObject {
  Q_OBJECT

private slots:
  void loadFromNonexistentRootYieldsEmptyStore();
  void saveLoadRoundTrip();
  void saveFailureSurfacesErrorAndRollsBack();
  void invalidAndConflictingOperationsAreRejected();
  void removeCalendarDeletesItsFile();
};

void TestEventStore::loadFromNonexistentRootYieldsEmptyStore() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  EventStore store(directory.filePath(QStringLiteral("missing/deeper")));
  const EventStoreLoadResult result = store.loadAll({personalInfo()});
  QVERIFY2(result.ok(), "absent files are a clean first run, not a failure");
  QVERIFY(store.hasCalendar(QStringLiteral("personal")));
  QVERIFY(store.events(QStringLiteral("personal")).isEmpty());

  EventStore empty(directory.filePath(QStringLiteral("also-missing")));
  QVERIFY(empty.loadAll({}).ok());
  QVERIFY(empty.calendars().isEmpty());
}

void TestEventStore::saveLoadRoundTrip() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString root = directory.filePath(QStringLiteral("calendar"));

  const QTimeZone berlin("Europe/Berlin");
  const QDateTime start(QDate(2026, 9, 10), QTime(9, 30), berlin);
  const QDateTime end(QDate(2026, 9, 10), QTime(10, 30), berlin);

  {
    EventStore store(root);
    QVERIFY(store.addCalendar(personalInfo()).ok());

    const KCalendarCore::Event::Ptr plain =
        makeEvent(QStringLiteral("uid-plain"), QStringLiteral("Review"),
                  start, end);
    plain->setLocation(QStringLiteral("Room 4"));
    QVERIFY(store.addEvent(QStringLiteral("personal"), plain).ok());

    const KCalendarCore::Event::Ptr allDay =
        makeEvent(QStringLiteral("uid-allday"), QStringLiteral("Holiday"),
                  QDateTime(QDate(2026, 9, 11), QTime(0, 0)),
                  QDateTime(QDate(2026, 9, 11), QTime(0, 0)));
    allDay->setAllDay(true);
    QVERIFY(store.addEvent(QStringLiteral("personal"), allDay).ok());

    const KCalendarCore::Event::Ptr recurring =
        makeEvent(QStringLiteral("uid-recurring"), QStringLiteral("Standup"),
                  start, end);
    recurring->recurrence()->setDaily(1);
    recurring->recurrence()->setDuration(5);
    QVERIFY(store.addEvent(QStringLiteral("personal"), recurring).ok());

    const KCalendarCore::Event::Ptr reminded =
        makeEvent(QStringLiteral("uid-reminded"), QStringLiteral("Dentist"),
                  start, end);
    KCalendarCore::Alarm::Ptr alarm(
        new KCalendarCore::Alarm(reminded.data()));
    alarm->setDisplayAlarm(QStringLiteral("Dentist"));
    alarm->setStartOffset(KCalendarCore::Duration(-15 * 60));
    alarm->setEnabled(true);
    reminded->addAlarm(alarm);
    QVERIFY(store.addEvent(QStringLiteral("personal"), reminded).ok());

    QVERIFY(QFileInfo::exists(
        store.calendarFilePath(QStringLiteral("personal"))));
  }

  EventStore reloaded(root);
  const EventStoreLoadResult result = reloaded.loadAll({personalInfo()});
  QVERIFY2(result.ok(), qPrintable(result.failures.isEmpty()
                                       ? QString()
                                       : result.failures.first().diagnostic));
  const KCalendarCore::Event::List events =
      reloaded.events(QStringLiteral("personal"));
  QCOMPARE(events.size(), 4);

  const KCalendarCore::Event::Ptr plain = findByUid(events, QStringLiteral("uid-plain"));
  QVERIFY(plain);
  QCOMPARE(plain->summary(), QStringLiteral("Review"));
  QCOMPARE(plain->location(), QStringLiteral("Room 4"));
  QCOMPARE(plain->dtStart(), start);
  QCOMPARE(plain->dtStart().offsetFromUtc(), start.offsetFromUtc());
  QCOMPARE(plain->dtEnd(), end);
  QVERIFY(!plain->allDay());
  QVERIFY(!plain->recurs());

  const KCalendarCore::Event::Ptr allDay =
      findByUid(events, QStringLiteral("uid-allday"));
  QVERIFY(allDay);
  QVERIFY(allDay->allDay());
  QCOMPARE(allDay->dtStart().date(), QDate(2026, 9, 11));

  const KCalendarCore::Event::Ptr recurring =
      findByUid(events, QStringLiteral("uid-recurring"));
  QVERIFY(recurring);
  QVERIFY(recurring->recurs());
  QCOMPARE(recurring->recurrence()->frequency(), 1);
  QCOMPARE(recurring->recurrence()->duration(), 5);

  const KCalendarCore::Event::Ptr reminded =
      findByUid(events, QStringLiteral("uid-reminded"));
  QVERIFY(reminded);
  QCOMPARE(reminded->alarms().size(), 1);
  QCOMPARE(reminded->alarms().constFirst()->startOffset().asSeconds(), -900);
}

void TestEventStore::saveFailureSurfacesErrorAndRollsBack() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  // A regular file as a path component makes every save fail regardless of
  // the test user's privileges.
  const QString blocker = directory.filePath(QStringLiteral("blocker"));
  QFile file(blocker);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.close();

  EventStore store(blocker + QStringLiteral("/sub"));
  QVERIFY(store.addCalendar(personalInfo()).ok());

  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("uid-1"), QStringLiteral("One"),
                QDateTime(QDate(2026, 9, 10), QTime(9, 0), QTimeZone::UTC),
                QDateTime(QDate(2026, 9, 10), QTime(10, 0), QTimeZone::UTC));
  const EventStoreResult added =
      store.addEvent(QStringLiteral("personal"), event);
  QCOMPARE(added.error, EventStoreError::WriteFailed);
  QVERIFY(!added.diagnostic.isEmpty());
  QVERIFY2(store.events(QStringLiteral("personal")).isEmpty(),
           "a failed persist must roll the in-memory add back");
}

void TestEventStore::invalidAndConflictingOperationsAreRejected() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());

  QCOMPARE(store.addCalendar({.id = QStringLiteral("../escape"),
                              .displayName = QStringLiteral("Escape"),
                              .colorToken = QString(),
                              .enabled = true})
               .error,
           EventStoreError::InvalidCalendarId);

  QVERIFY(store.addCalendar(personalInfo()).ok());
  QCOMPARE(store.addCalendar(personalInfo()).error,
           EventStoreError::DuplicateCalendar);

  const QDateTime start(QDate(2026, 9, 10), QTime(9, 0), QTimeZone::UTC);
  const QDateTime end(QDate(2026, 9, 10), QTime(10, 0), QTimeZone::UTC);
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("uid-1"), QStringLiteral("One"), start, end);

  QCOMPARE(store.addEvent(QStringLiteral("missing"), event).error,
           EventStoreError::UnknownCalendar);
  QVERIFY(store.addEvent(QStringLiteral("personal"), event).ok());
  QCOMPARE(store
               .addEvent(QStringLiteral("personal"),
                         makeEvent(QStringLiteral("uid-1"),
                                   QStringLiteral("Again"), start, end))
               .error,
           EventStoreError::DuplicateEvent);
  QCOMPARE(store
               .addEvents(QStringLiteral("personal"),
                          {KCalendarCore::Event::Ptr()})
               .error,
           EventStoreError::Malformed);

  QCOMPARE(store
               .updateEvent(QStringLiteral("personal"),
                            makeEvent(QStringLiteral("uid-missing"),
                                      QStringLiteral("Ghost"), start, end))
               .error,
           EventStoreError::UnknownEvent);

  const KCalendarCore::Event::Ptr replacement =
      makeEvent(QStringLiteral("uid-1"), QStringLiteral("One v2"), start, end);
  QVERIFY(store.updateEvent(QStringLiteral("personal"), replacement).ok());
  QCOMPARE(store.events(QStringLiteral("personal")).size(), 1);
  QCOMPARE(store.events(QStringLiteral("personal")).constFirst()->summary(),
           QStringLiteral("One v2"));

  QCOMPARE(store.removeEvent(QStringLiteral("personal"),
                             QStringLiteral("uid-missing"))
               .error,
           EventStoreError::UnknownEvent);
}

void TestEventStore::removeCalendarDeletesItsFile() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(personalInfo()).ok());
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("uid-1"), QStringLiteral("One"),
                QDateTime(QDate(2026, 9, 10), QTime(9, 0), QTimeZone::UTC),
                QDateTime(QDate(2026, 9, 10), QTime(10, 0), QTimeZone::UTC));
  QVERIFY(store.addEvent(QStringLiteral("personal"), event).ok());
  const QString path = store.calendarFilePath(QStringLiteral("personal"));
  QVERIFY(QFileInfo::exists(path));

  QCOMPARE(store.removeCalendar(QStringLiteral("missing")).error,
           EventStoreError::UnknownCalendar);
  QVERIFY(store.removeCalendar(QStringLiteral("personal")).ok());
  QVERIFY(!QFileInfo::exists(path));
  QVERIFY(!store.hasCalendar(QStringLiteral("personal")));
}

QTEST_GUILESS_MAIN(TestEventStore)
#include "tst_event_store.moc"
