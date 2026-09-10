// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/reminder_delivery.h"

#include <QDBusConnection>
#include <QTest>

using namespace QindaQt::Apps::Calendar;

class TestReminderDelivery final : public QObject {
  Q_OBJECT

private slots:
  void disconnectedConnectionFallsBackWithoutWarning();
};

void TestReminderDelivery::disconnectedConnectionFallsBackWithoutWarning() {
  // A never-connected named connection models the offscreen/no-bus rows:
  // delivery must report failure so the caller shows the in-window banner,
  // and must not qWarning (offscreen rows run with QT_FATAL_WARNINGS=1).
  QDBusConnection invalid(QStringLiteral("qindaqt-calendar-test-invalid"));
  QVERIFY(!invalid.isConnected());
  ReminderDelivery delivery(invalid);
  QVERIFY(!delivery.deliver(QStringLiteral("Standup"),
                            QDateTime(QDate(2026, 9, 7), QTime(9, 0),
                                      QTimeZone::UTC)));
}

QTEST_GUILESS_MAIN(TestReminderDelivery)
#include "tst_reminder_delivery.moc"
