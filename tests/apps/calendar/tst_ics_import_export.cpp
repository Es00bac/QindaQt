// SPDX-License-Identifier: GPL-3.0-or-later
#include "ics/ics_import_export.h"

#include "model/event_store.h"
#include "model/occurrence_expander.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Event>

#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace QindaQt::Apps::Calendar;

namespace {

[[nodiscard]] QString fixturePath(const QString &name) {
  return QStringLiteral(QINDAQT_CALENDAR_FIXTURE_DIR "/") + name;
}

[[nodiscard]] CalendarInfo infoFor(const QString &id) {
  return {.id = id,
          .displayName = id,
          .colorToken = QStringLiteral("accent"),
          .enabled = true};
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

class TestIcsImportExport final : public QObject {
  Q_OBJECT

private slots:
  void importsGoogleExportWithRecurrenceAlarmAndTimezones();
  void importsAppleExport();
  void reimportSkipsDuplicateUids();
  void malformedFileErrorsWithoutMutatingTheStore();
  void exportImportRoundTripPreservesEvents();
  void exportAllCombinesCalendars();
  void unknownCalendarAndMissingFileAreReported();
};

void TestIcsImportExport::importsGoogleExportWithRecurrenceAlarmAndTimezones() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());

  const IcsImportResult result =
      importFile(fixturePath(QStringLiteral("google_basic.ics")),
                 QStringLiteral("personal"), store);
  QVERIFY2(result.ok(), qPrintable(result.error));
  QCOMPARE(result.imported, 4);
  QCOMPARE(result.skippedDuplicates, 0);

  const KCalendarCore::Event::List events =
      store.events(QStringLiteral("personal"));
  QCOMPARE(events.size(), 4);

  const KCalendarCore::Event::Ptr allDay =
      findByUid(events, QStringLiteral("google-allday-1@google.com"));
  QVERIFY(allDay);
  QVERIFY(allDay->allDay());
  QCOMPARE(allDay->dtStart().date(), QDate(2026, 9, 14));

  const KCalendarCore::Event::Ptr weekly =
      findByUid(events, QStringLiteral("google-weekly-1@google.com"));
  QVERIFY(weekly);
  QVERIFY(weekly->recurs());
  // The EXDATE on 2026-09-21 must remove that one occurrence.
  const QList<Occurrence> occurrences = expandOccurrences(
      *store.calendar(QStringLiteral("personal")),
      QStringLiteral("personal"),
      QDateTime(QDate(2026, 9, 1), QTime(0, 0), QTimeZone("Europe/Berlin")),
      QDateTime(QDate(2026, 10, 1), QTime(0, 0), QTimeZone("Europe/Berlin")));
  QList<QDate> weeklyDates;
  for (const Occurrence &occurrence : occurrences) {
    if (occurrence.eventUid == weekly->uid()) {
      weeklyDates.append(occurrence.start.date());
    }
  }
  QCOMPARE(weeklyDates, (QList<QDate>{QDate(2026, 9, 7), QDate(2026, 9, 14),
                                      QDate(2026, 9, 28)}));

  const KCalendarCore::Event::Ptr alarmed =
      findByUid(events, QStringLiteral("google-alarm-1@google.com"));
  QVERIFY(alarmed);
  QCOMPARE(alarmed->alarms().size(), 1);
  QCOMPARE(alarmed->alarms().constFirst()->startOffset().asSeconds(), -900);

  const KCalendarCore::Event::Ptr newYork =
      findByUid(events, QStringLiteral("google-ny-1@google.com"));
  QVERIFY(newYork);
  // 2026-09-16 09:00 America/New_York is EDT, four hours behind UTC.
  QCOMPARE(newYork->dtStart().offsetFromUtc(), -4 * 3600);
  QCOMPARE(newYork->dtStart().toUTC(),
           QDateTime(QDate(2026, 9, 16), QTime(13, 0), QTimeZone::UTC));
}

void TestIcsImportExport::importsAppleExport() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());

  const IcsImportResult result =
      importFile(fixturePath(QStringLiteral("apple_basic.ics")),
                 QStringLiteral("personal"), store);
  QVERIFY2(result.ok(), qPrintable(result.error));
  QCOMPARE(result.imported, 2);
  QCOMPARE(result.skippedDuplicates, 0);

  const KCalendarCore::Event::List events =
      store.events(QStringLiteral("personal"));
  const KCalendarCore::Event::Ptr timed =
      findByUid(events, QStringLiteral("apple-timed-1@icloud.com"));
  QVERIFY(timed);
  QCOMPARE(timed->alarms().size(), 1);
  QCOMPARE(timed->alarms().constFirst()->startOffset().asSeconds(), -600);

  const KCalendarCore::Event::Ptr monthly =
      findByUid(events, QStringLiteral("apple-monthly-1@icloud.com"));
  QVERIFY(monthly);
  QVERIFY(monthly->recurs());
}

void TestIcsImportExport::reimportSkipsDuplicateUids() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());

  const IcsImportResult first =
      importFile(fixturePath(QStringLiteral("google_basic.ics")),
                 QStringLiteral("personal"), store);
  QVERIFY(first.ok());
  const IcsImportResult second =
      importFile(fixturePath(QStringLiteral("google_basic.ics")),
                 QStringLiteral("personal"), store);
  QVERIFY2(second.ok(), qPrintable(second.error));
  QCOMPARE(second.imported, 0);
  QCOMPARE(second.skippedDuplicates, 4);
  QCOMPARE(store.events(QStringLiteral("personal")).size(), 4);
}

void TestIcsImportExport::malformedFileErrorsWithoutMutatingTheStore() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());

  const IcsImportResult result =
      importFile(fixturePath(QStringLiteral("malformed.ics")),
                 QStringLiteral("personal"), store);
  QVERIFY(!result.ok());
  QVERIFY(!result.error.isEmpty());
  QCOMPARE(result.imported, 0);
  QVERIFY2(store.events(QStringLiteral("personal")).isEmpty(),
           "a malformed import must leave the store untouched");
}

void TestIcsImportExport::exportImportRoundTripPreservesEvents() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());
  QVERIFY(importFile(fixturePath(QStringLiteral("google_basic.ics")),
                     QStringLiteral("personal"), store)
              .ok());

  const QString exportedPath =
      directory.filePath(QStringLiteral("exported.ics"));
  const IcsExportResult exported =
      exportCalendar(QStringLiteral("personal"), exportedPath, store);
  QVERIFY2(exported.ok(), qPrintable(exported.error));
  QCOMPARE(exported.exported, 4);
  QVERIFY(QFileInfo::exists(exportedPath));

  EventStore target(directory.filePath(QStringLiteral("target")));
  QVERIFY(target.addCalendar(infoFor(QStringLiteral("work"))).ok());
  const IcsImportResult reimported =
      importFile(exportedPath, QStringLiteral("work"), target);
  QVERIFY2(reimported.ok(), qPrintable(reimported.error));
  QCOMPARE(reimported.imported, 4);
  QCOMPARE(reimported.skippedDuplicates, 0);

  const KCalendarCore::Event::List original =
      store.events(QStringLiteral("personal"));
  const KCalendarCore::Event::List copy =
      target.events(QStringLiteral("work"));
  for (const KCalendarCore::Event::Ptr &event : original) {
    const KCalendarCore::Event::Ptr twin = findByUid(copy, event->uid());
    QVERIFY2(twin, qPrintable(event->uid()));
    QCOMPARE(twin->summary(), event->summary());
    QCOMPARE(twin->dtStart(), event->dtStart());
    QCOMPARE(twin->dtEnd(), event->dtEnd());
    QCOMPARE(twin->allDay(), event->allDay());
    QCOMPARE(twin->recurs(), event->recurs());
    QCOMPARE(twin->alarms().size(), event->alarms().size());
  }
}

void TestIcsImportExport::exportAllCombinesCalendars() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("work"))).ok());
  QVERIFY(importFile(fixturePath(QStringLiteral("google_basic.ics")),
                     QStringLiteral("personal"), store)
              .ok());
  QVERIFY(importFile(fixturePath(QStringLiteral("apple_basic.ics")),
                     QStringLiteral("work"), store)
              .ok());

  const QString path = directory.filePath(QStringLiteral("all.ics"));
  const IcsExportResult result = exportAll(path, store);
  QVERIFY2(result.ok(), qPrintable(result.error));
  QCOMPARE(result.exported, 6);

  EventStore target(directory.filePath(QStringLiteral("target")));
  QVERIFY(target.addCalendar(infoFor(QStringLiteral("merged"))).ok());
  const IcsImportResult reimported =
      importFile(path, QStringLiteral("merged"), target);
  QVERIFY2(reimported.ok(), qPrintable(reimported.error));
  QCOMPARE(reimported.imported, 6);
}

void TestIcsImportExport::unknownCalendarAndMissingFileAreReported() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  EventStore store(directory.path());
  QVERIFY(store.addCalendar(infoFor(QStringLiteral("personal"))).ok());

  QVERIFY(!importFile(fixturePath(QStringLiteral("google_basic.ics")),
                      QStringLiteral("missing"), store)
               .ok());
  QVERIFY(!importFile(fixturePath(QStringLiteral("does-not-exist.ics")),
                      QStringLiteral("personal"), store)
               .ok());
  QVERIFY(!exportCalendar(QStringLiteral("missing"),
                          directory.filePath(QStringLiteral("out.ics")), store)
               .ok());
}

QTEST_GUILESS_MAIN(TestIcsImportExport)
#include "tst_ics_import_export.moc"
