// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>

#include "monitor_facade.h"

using namespace QindaQt::SystemMonitor;

// The formatting contract every panel and every table cell depends on. The
// rule that carries the weight here is that an unavailable reading formats
// as a dash: a zero would assert something about the machine that procfs or
// the driver specifically declined to say.
class TestMonitorFacade : public QObject {
  Q_OBJECT

private slots:
  void knownRejectsMissingReadings();
  void bytesUsesBinaryPrefixes();
  void ratesCarryTheirUnit();
  void unavailableReadingsAreNeverZero();
  void durationsReadAsUptime();
};

void TestMonitorFacade::knownRejectsMissingReadings() {
  QVERIFY(!MonitorFacade::known(QVariant()));
  QVERIFY(!MonitorFacade::known(QVariant::fromValue(nullptr)));
  QVERIFY(!MonitorFacade::known(QVariant(qQNaN())));
  QVERIFY(MonitorFacade::known(QVariant(0.0)));
  QVERIFY(MonitorFacade::known(QVariant(0)));
}

void TestMonitorFacade::bytesUsesBinaryPrefixes() {
  // Binary, because every counter behind these came from the kernel in
  // binary units; 1 MiB must not print as 1.05 MB.
  QCOMPARE(MonitorFacade::bytes(512), QStringLiteral("512 B"));
  QCOMPARE(MonitorFacade::bytes(1024), QStringLiteral("1.0 KiB"));
  QCOMPARE(MonitorFacade::bytes(1024 * 1024), QStringLiteral("1.0 MiB"));
  QCOMPARE(MonitorFacade::bytes(qint64(3) * 1024 * 1024 * 1024),
           QStringLiteral("3.0 GiB"));
  // Whole bytes have no fraction worth showing.
  QVERIFY(!MonitorFacade::bytes(999).contains(QLatin1Char('.')));
}

void TestMonitorFacade::ratesCarryTheirUnit() {
  QCOMPARE(MonitorFacade::rate(0), QStringLiteral("0 B/s"));
  QCOMPARE(MonitorFacade::rate(2048), QStringLiteral("2.0 KiB/s"));
}

void TestMonitorFacade::unavailableReadingsAreNeverZero() {
  const QString dash = MonitorFacade::unavailable();
  QVERIFY(!dash.isEmpty());
  QCOMPARE(MonitorFacade::bytes(QVariant()), dash);
  QCOMPARE(MonitorFacade::rate(QVariant()), dash);
  QCOMPARE(MonitorFacade::percent(QVariant()), dash);
  QCOMPARE(MonitorFacade::number(QVariant()), dash);
  QCOMPARE(MonitorFacade::duration(QVariant()), dash);
  // A NaN that slipped through arithmetic must read the same way.
  QCOMPARE(MonitorFacade::rate(QVariant(qQNaN())), dash);
}

void TestMonitorFacade::durationsReadAsUptime() {
  QCOMPARE(MonitorFacade::duration(59), QStringLiteral("0:59"));
  QCOMPARE(MonitorFacade::duration(61), QStringLiteral("1:01"));
  QCOMPARE(MonitorFacade::duration(3661), QStringLiteral("1:01:01"));
  QCOMPARE(MonitorFacade::duration(90061), QStringLiteral("1d 01:01:01"));
}

QTEST_MAIN(TestMonitorFacade)
#include "tst_monitor_facade.moc"
