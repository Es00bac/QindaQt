// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitor_engine.h"
#include "procfs_reader.h"

#include <QAbstractItemModel>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

#include <signal.h>

using namespace QindaQt::SystemMonitor;

class MonitorEngineTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void publishesLiveSnapshotAndRoles();
  void controlsIntervalAndPause();
  void validatesIdentityAndActsOnDisposableChildren();
};

void MonitorEngineTest::publishesLiveSnapshotAndRoles() {
  MonitorEngine engine;
  QSignalSpy updated(&engine, &MonitorEngine::updated);
  QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 5000);
  const QVariantMap snapshot = engine.snapshot();
  const QStringList keys = {
      QStringLiteral("cpu"),         QStringLiteral("cores"),
      QStringLiteral("memory"),      QStringLiteral("disks"),
      QStringLiteral("filesystems"), QStringLiteral("network"),
      QStringLiteral("uptime"),      QStringLiteral("load1"),
      QStringLiteral("load5"),       QStringLiteral("load15"),
      QStringLiteral("timestamp"),   QStringLiteral("history")};
  for (const auto &key : keys) {
    QVERIFY2(snapshot.contains(key), qPrintable(key));
  }
  QVERIFY(snapshot.value(QStringLiteral("cpu")).toDouble() >= 0.0);
  QVERIFY(snapshot.value(QStringLiteral("cpu")).toDouble() <= 100.0);
  QVERIFY(engine.processes()->rowCount() > 0);
  const auto roles = engine.processes()->roleNames();
  const QList<QByteArray> expectedRoles = {
      "pid",       "startTicks", "name",  "cpu",     "memory", "readRate",
      "writeRate", "user",       "state", "threads", "nice",   "command",
  };
  for (const auto &name : expectedRoles) {
    QVERIFY2(roles.values().contains(name), name.constData());
  }
}

void MonitorEngineTest::controlsIntervalAndPause() {
  MonitorEngine engine;
  QCOMPARE(engine.interval(), 1000);
  engine.setInterval(1);
  QCOMPARE(engine.interval(), 250);
  engine.setInterval(50000);
  QCOMPARE(engine.interval(), 10000);
  engine.setPaused(true);
  QVERIFY(engine.paused());
  QSignalSpy updated(&engine, &MonitorEngine::updated);
  engine.requestSample();
  QTRY_VERIFY_WITH_TIMEOUT(updated.count() > 0, 5000);
  engine.setPaused(false);
  QVERIFY(!engine.paused());
}

void MonitorEngineTest::validatesIdentityAndActsOnDisposableChildren() {
  MonitorEngine engine;
  QProcess child;
  child.start(QStringLiteral("/bin/sleep"), {QStringLiteral("30")});
  QVERIFY(child.waitForStarted());
  const qint64 pid = child.processId();
  QString readError;
  auto process = ProcfsReader().readProcess(pid, &readError);
  QVERIFY2(process, qPrintable(readError));

  QString error = engine.processAction(pid, process->startTicks + 1,
                                       QStringLiteral("terminate"));
  QVERIFY(error.contains(QStringLiteral("identity changed")));
  QCOMPARE(child.state(), QProcess::Running);

  QVERIFY(engine.processAction(pid, process->startTicks, QStringLiteral("stop"))
              .isEmpty());
  QTRY_COMPARE_WITH_TIMEOUT(ProcfsReader().readProcess(pid)->state,
                            QStringLiteral("T"), 2000);
  QVERIFY(
      engine.processAction(pid, process->startTicks, QStringLiteral("continue"))
          .isEmpty());
  QTRY_VERIFY_WITH_TIMEOUT(
      ProcfsReader().readProcess(pid)->state != QStringLiteral("T"), 2000);
  QVERIFY(
      engine.processAction(pid, process->startTicks, QStringLiteral("nice"), 10)
          .isEmpty());
  QTRY_COMPARE_WITH_TIMEOUT(ProcfsReader().readProcess(pid)->nice, 10, 2000);
  QVERIFY(
      engine
          .processAction(pid, process->startTicks, QStringLiteral("terminate"))
          .isEmpty());
  QVERIFY(child.waitForFinished(3000));

  QProcess killed;
  killed.start(QStringLiteral("/bin/sleep"), {QStringLiteral("30")});
  QVERIFY(killed.waitForStarted());
  process = ProcfsReader().readProcess(killed.processId(), &readError);
  QVERIFY(process);
  QVERIFY(engine
              .processAction(killed.processId(), process->startTicks,
                             QStringLiteral("kill"))
              .isEmpty());
  QVERIFY(killed.waitForFinished(3000));
  QVERIFY(killed.exitStatus() == QProcess::CrashExit);

  QVERIFY(
      engine.processAction(pid, process->startTicks, QStringLiteral("dance"))
          .contains(QStringLiteral("Unsupported")));
  QVERIFY(engine
              .processAction(std::numeric_limits<qint64>::max(), 1,
                             QStringLiteral("terminate"))
              .contains(QStringLiteral("positive PID")));
}

QTEST_GUILESS_MAIN(MonitorEngineTest)
#include "tst_monitor_engine.moc"
