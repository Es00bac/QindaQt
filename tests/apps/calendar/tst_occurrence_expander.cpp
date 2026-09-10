// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/occurrence_expander.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Duration>
#include <KCalendarCore/Event>
#include <KCalendarCore/Recurrence>

#include <QTest>
#include <QTimeZone>

using namespace QindaQt::Apps::Calendar;

namespace {

const QTimeZone berlin("Europe/Berlin");

[[nodiscard]] KCalendarCore::MemoryCalendar::Ptr makeCalendar() {
  return KCalendarCore::MemoryCalendar::Ptr(
      new KCalendarCore::MemoryCalendar(berlin));
}

[[nodiscard]] KCalendarCore::Event::Ptr makeEvent(const QString &uid,
                                                  const QDateTime &start,
                                                  const QDateTime &end) {
  KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
  event->setUid(uid);
  event->setSummary(uid + QStringLiteral(" summary"));
  event->setDtStart(start);
  event->setDtEnd(end);
  return event;
}

[[nodiscard]] QList<QDateTime> starts(const QList<Occurrence> &occurrences) {
  QList<QDateTime> result;
  for (const Occurrence &occurrence : occurrences) {
    result.append(occurrence.start);
  }
  return result;
}

} // namespace

class TestOccurrenceExpander final : public QObject {
  Q_OBJECT

private slots:
  void dailyRuleExpandsWithinRange();
  void weeklyAndMonthlyRulesExpand();
  void exdateRemovesOneOccurrence();
  void rangeBoundariesAreHalfOpen();
  void allDayEventsExpand();
  void dstTransitionKeepsLocalTime();
  void invalidRangeYieldsNothing();
  void reminderExpansionAppliesValarmOffsets();
};

void TestOccurrenceExpander::dailyRuleExpandsWithinRange() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), berlin);
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("daily"), start, start.addSecs(3600));
  event->recurrence()->setDaily(1);
  event->recurrence()->setDuration(5);
  calendar->addEvent(event);

  const QList<Occurrence> occurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 9, 6), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 9, 12), QTime(0, 0), berlin));
  QCOMPARE(occurrences.size(), 5);
  for (int i = 0; i < 5; ++i) {
    QCOMPARE(occurrences.at(i).start, start.addDays(i));
    QCOMPARE(occurrences.at(i).end, start.addDays(i).addSecs(3600));
    QCOMPARE(occurrences.at(i).eventUid, QStringLiteral("daily"));
    QCOMPARE(occurrences.at(i).calendarId, QStringLiteral("personal"));
    QVERIFY(occurrences.at(i).recurring);
    QVERIFY(!occurrences.at(i).allDay);
  }
}

void TestOccurrenceExpander::weeklyAndMonthlyRulesExpand() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime monday(QDate(2026, 9, 7), QTime(10, 0), berlin);
  const KCalendarCore::Event::Ptr weekly =
      makeEvent(QStringLiteral("weekly"), monday, monday.addSecs(1800));
  weekly->recurrence()->setWeekly(1);
  weekly->recurrence()->setDuration(4);
  calendar->addEvent(weekly);

  const QDateTime fifteenth(QDate(2026, 1, 15), QTime(12, 0), berlin);
  const KCalendarCore::Event::Ptr monthly =
      makeEvent(QStringLiteral("monthly"), fifteenth, fifteenth.addSecs(3600));
  monthly->recurrence()->setMonthly(1);
  monthly->recurrence()->setDuration(3);
  calendar->addEvent(monthly);

  const QList<Occurrence> weeklyOccurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 9, 1), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 10, 15), QTime(0, 0), berlin));
  QCOMPARE(starts(weeklyOccurrences),
           (QList<QDateTime>{monday, monday.addDays(7), monday.addDays(14),
                             monday.addDays(21)}));

  const QList<Occurrence> monthlyOccurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 1, 1), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 4, 1), QTime(0, 0), berlin));
  QCOMPARE(monthlyOccurrences.size(), 3);
  QCOMPARE(starts(monthlyOccurrences),
           (QList<QDateTime>{fifteenth, fifteenth.addMonths(1),
                             fifteenth.addMonths(2)}));
}

void TestOccurrenceExpander::exdateRemovesOneOccurrence() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), berlin);
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("exdated"), start, start.addSecs(3600));
  event->recurrence()->setDaily(1);
  event->recurrence()->setDuration(7);
  event->recurrence()->addExDateTime(start.addDays(2));
  calendar->addEvent(event);

  const QList<Occurrence> occurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 9, 6), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 9, 20), QTime(0, 0), berlin));
  QCOMPARE(occurrences.size(), 6);
  QVERIFY(!starts(occurrences).contains(start.addDays(2)));
}

void TestOccurrenceExpander::rangeBoundariesAreHalfOpen() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime atStart(QDate(2026, 9, 7), QTime(9, 0), berlin);
  const QDateTime atEnd(QDate(2026, 9, 8), QTime(9, 0), berlin);
  calendar->addEvent(makeEvent(QStringLiteral("at-start"), atStart,
                               atStart.addSecs(3600)));
  calendar->addEvent(
      makeEvent(QStringLiteral("at-end"), atEnd, atEnd.addSecs(3600)));

  const QList<Occurrence> occurrences =
      expandOccurrences(*calendar, QStringLiteral("personal"), atStart, atEnd);
  QCOMPARE(occurrences.size(), 1);
  QCOMPARE(occurrences.constFirst().eventUid, QStringLiteral("at-start"));
}

void TestOccurrenceExpander::allDayEventsExpand() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("allday"),
                QDateTime(QDate(2026, 9, 10), QTime(0, 0)),
                QDateTime(QDate(2026, 9, 10), QTime(0, 0)));
  event->setAllDay(true);
  calendar->addEvent(event);

  const QList<Occurrence> occurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 9, 9), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 9, 12), QTime(0, 0), berlin));
  QCOMPARE(occurrences.size(), 1);
  QVERIFY(occurrences.constFirst().allDay);
  QVERIFY(!occurrences.constFirst().recurring);
  QCOMPARE(occurrences.constFirst().start.date(), QDate(2026, 9, 10));
  QCOMPARE(occurrences.constFirst().start.time(), QTime(0, 0));
  // KCalendarCore's all-day end is inclusive: the one-day event reports the
  // same date for start and end (see calendar_types.h).
  QCOMPARE(occurrences.constFirst().end.date(), QDate(2026, 9, 10));
}

void TestOccurrenceExpander::dstTransitionKeepsLocalTime() {
  // Europe/Berlin springs forward on Sunday 2026-03-29. A weekly Monday
  // 09:00 event must keep 09:00 local across the transition while its UTC
  // offset moves from +01:00 to +02:00.
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime first(QDate(2026, 3, 23), QTime(9, 0), berlin);
  QCOMPARE(first.offsetFromUtc(), 3600);
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("dst-weekly"), first, first.addSecs(3600));
  event->recurrence()->setWeekly(1);
  calendar->addEvent(event);

  const QList<Occurrence> occurrences = expandOccurrences(
      *calendar, QStringLiteral("personal"),
      QDateTime(QDate(2026, 3, 20), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 4, 6), QTime(0, 0), berlin));
  QCOMPARE(occurrences.size(), 2);

  QCOMPARE(occurrences.at(0).start.date(), QDate(2026, 3, 23));
  QCOMPARE(occurrences.at(0).start.time(), QTime(9, 0));
  QCOMPARE(occurrences.at(0).start.offsetFromUtc(), 3600);

  QCOMPARE(occurrences.at(1).start.date(), QDate(2026, 3, 30));
  QCOMPARE(occurrences.at(1).start.time(), QTime(9, 0));
  QCOMPARE(occurrences.at(1).start.offsetFromUtc(), 7200);
}

void TestOccurrenceExpander::invalidRangeYieldsNothing() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), berlin);
  calendar->addEvent(
      makeEvent(QStringLiteral("plain"), start, start.addSecs(3600)));

  QVERIFY(expandOccurrences(*calendar, QStringLiteral("personal"), start, start)
              .isEmpty());
  QVERIFY(expandOccurrences(*calendar, QStringLiteral("personal"),
                            start.addDays(1), start)
              .isEmpty());
  QVERIFY(expandOccurrences(*calendar, QStringLiteral("personal"), QDateTime(),
                            start.addDays(1))
              .isEmpty());
}

void TestOccurrenceExpander::reminderExpansionAppliesValarmOffsets() {
  const KCalendarCore::MemoryCalendar::Ptr calendar = makeCalendar();
  const QDateTime start(QDate(2026, 9, 7), QTime(9, 0), berlin);
  const KCalendarCore::Event::Ptr event =
      makeEvent(QStringLiteral("reminded"), start, start.addSecs(3600));
  event->recurrence()->setDaily(1);
  event->recurrence()->setDuration(3);

  KCalendarCore::Alarm::Ptr quarterHour(new KCalendarCore::Alarm(event.data()));
  quarterHour->setDisplayAlarm(event->summary());
  quarterHour->setStartOffset(KCalendarCore::Duration(-15 * 60));
  quarterHour->setEnabled(true);
  event->addAlarm(quarterHour);

  KCalendarCore::Alarm::Ptr oneHour(new KCalendarCore::Alarm(event.data()));
  oneHour->setDisplayAlarm(event->summary());
  oneHour->setStartOffset(KCalendarCore::Duration(-60 * 60));
  oneHour->setEnabled(true);
  event->addAlarm(oneHour);

  KCalendarCore::Alarm::Ptr silenced(new KCalendarCore::Alarm(event.data()));
  silenced->setDisplayAlarm(event->summary());
  silenced->setStartOffset(KCalendarCore::Duration(-5 * 60));
  silenced->setEnabled(false);
  event->addAlarm(silenced);

  calendar->addEvent(event);

  const QList<ReminderRequest> requests = expandReminders(
      *calendar, QDateTime(QDate(2026, 9, 6), QTime(0, 0), berlin),
      QDateTime(QDate(2026, 9, 12), QTime(0, 0), berlin));
  QCOMPARE(requests.size(), 6); // 3 occurrences x 2 enabled alarms
  for (const ReminderRequest &request : requests) {
    QCOMPARE(request.eventUid, QStringLiteral("reminded"));
    const qint64 offset = request.occurrenceStart.secsTo(request.fireAt);
    QVERIFY(offset == -900 || offset == -3600);
  }
}

QTEST_GUILESS_MAIN(TestOccurrenceExpander)
#include "tst_occurrence_expander.moc"
