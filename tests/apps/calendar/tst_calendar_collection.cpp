// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/calendar_collection.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::Calendar;

namespace {

[[nodiscard]] CalendarInfo workInfo() {
  return {.id = QStringLiteral("work"),
          .displayName = QStringLiteral("Work"),
          .colorToken = QStringLiteral("accent-alt"),
          .enabled = true};
}

} // namespace

class TestCalendarCollection final : public QObject {
  Q_OBJECT

private slots:
  void firstRunCreatesDefaultPersonalCalendar();
  void mutationsPersistAcrossInstances();
  void removeAndValidationRules();
  void malformedFileIsRejectedWithoutTouchingState();
};

void TestCalendarCollection::firstRunCreatesDefaultPersonalCalendar() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());

  CalendarCollection collection(directory.filePath(QStringLiteral("cal")));
  const CalendarCollectionResult result = collection.load();
  QCOMPARE(result.error, CalendarCollectionError::Absent);
  QVERIFY(!result.ok());
  QVERIFY2(result.diagnostic.isEmpty(),
           "first run must not surface an error to the user");

  QCOMPARE(collection.calendars().size(), 1);
  const CalendarInfo personal = collection.calendars().constFirst();
  QCOMPARE(personal.id, QStringLiteral("personal"));
  QCOMPARE(personal.displayName, QStringLiteral("Personal"));
  QVERIFY(personal.enabled);
  QCOMPARE(collection.defaultCalendarId(), QStringLiteral("personal"));
}

void TestCalendarCollection::mutationsPersistAcrossInstances() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString root = directory.filePath(QStringLiteral("cal"));

  CalendarCollection collection(root);
  QCOMPARE(collection.load().error, CalendarCollectionError::Absent);
  QVERIFY(collection.addCalendar(workInfo()).ok());
  QVERIFY(collection
              .renameCalendar(QStringLiteral("work"), QStringLiteral("Day Job"))
              .ok());
  QVERIFY(collection.setEnabled(QStringLiteral("work"), false).ok());
  QVERIFY(collection.setDefaultCalendarId(QStringLiteral("work")).ok());

  CalendarCollection reloaded(root);
  const CalendarCollectionResult result = reloaded.load();
  QVERIFY2(result.ok(), qPrintable(result.diagnostic));
  QCOMPARE(reloaded.calendars(), collection.calendars());
  QCOMPARE(reloaded.defaultCalendarId(), QStringLiteral("work"));
  QCOMPARE(reloaded.calendars().at(1).displayName, QStringLiteral("Day Job"));
  QVERIFY(!reloaded.calendars().at(1).enabled);
}

void TestCalendarCollection::removeAndValidationRules() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  CalendarCollection collection(directory.path());
  QCOMPARE(collection.load().error, CalendarCollectionError::Absent);

  QCOMPARE(collection.addCalendar(workInfo()).error,
           CalendarCollectionError::None);
  QCOMPARE(collection.addCalendar(workInfo()).error,
           CalendarCollectionError::DuplicateCalendar);
  QCOMPARE(collection
               .addCalendar({.id = QStringLiteral("a/b"),
                             .displayName = QStringLiteral("Bad"),
                             .colorToken = QString(),
                             .enabled = true})
               .error,
           CalendarCollectionError::InvalidCalendarId);
  QCOMPARE(collection.renameCalendar(QStringLiteral("work"), QString()).error,
           CalendarCollectionError::Malformed);

  QCOMPARE(collection.removeCalendar(QStringLiteral("personal")).error,
           CalendarCollectionError::CannotRemoveDefaultCalendar);
  QCOMPARE(collection.removeCalendar(QStringLiteral("missing")).error,
           CalendarCollectionError::UnknownCalendar);
  QCOMPARE(collection.setDefaultCalendarId(QStringLiteral("missing")).error,
           CalendarCollectionError::UnknownCalendar);
  QCOMPARE(collection.setEnabled(QStringLiteral("missing"), true).error,
           CalendarCollectionError::UnknownCalendar);

  QVERIFY(collection.removeCalendar(QStringLiteral("work")).ok());
  QCOMPARE(collection.calendars().size(), 1);
  QCOMPARE(collection.defaultCalendarId(), QStringLiteral("personal"));
}

void TestCalendarCollection::malformedFileIsRejectedWithoutTouchingState() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  CalendarCollection collection(directory.path());
  QCOMPARE(collection.load().error, CalendarCollectionError::Absent);
  QVERIFY(collection.addCalendar(workInfo()).ok());

  QFile file(collection.filePath());
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write("{not json");
  file.close();

  const CalendarCollectionResult result = collection.load();
  QCOMPARE(result.error, CalendarCollectionError::Malformed);
  QVERIFY(!result.diagnostic.isEmpty());
  QCOMPARE(collection.calendars().size(), 2);
  QCOMPARE(collection.defaultCalendarId(), QStringLiteral("personal"));
}

QTEST_GUILESS_MAIN(TestCalendarCollection)
#include "tst_calendar_collection.moc"
