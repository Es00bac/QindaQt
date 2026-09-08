// SPDX-License-Identifier: GPL-3.0-or-later

#include "sample_collector.h"

#include <QtTest>

#include <unistd.h>

using namespace QindaQt::SystemMonitor;

namespace {

RawSample sampleAt(qint64 nanoseconds, quint64 scale,
                   quint64 processStart = 50) {
  RawSample sample;
  sample.monotonicNanoseconds = nanoseconds;
  sample.timestampMilliseconds = nanoseconds / 1'000'000;
  sample.cpu = {QStringLiteral("cpu"), 100 * scale, 40 * scale};
  sample.cores = {{QStringLiteral("0"), 100 * scale, 40 * scale},
                  {QStringLiteral("1"), 100 * scale, 60 * scale}};
  sample.memoryTotal = 1000;
  sample.memoryAvailable = 250;
  sample.memoryCached = 100;
  sample.swapTotal = 500;
  sample.swapFree = 300;
  sample.disks = {{QStringLiteral("8:0"), QStringLiteral("sda"), 1000 * scale,
                   2000 * scale, 100 * scale}};
  sample.network = {{QStringLiteral("eth0"), 3000 * scale, 5000 * scale}};
  ProcessCounters process;
  process.pid = 7;
  process.startTicks = processStart;
  process.name = QStringLiteral("job");
  process.cpuTicks = 10 * scale;
  process.memoryBytes = 4096;
  process.memoryAvailable = true;
  process.readBytes = 100 * scale;
  process.writeBytes = 200 * scale;
  process.readBytesAvailable = true;
  process.writeBytesAvailable = true;
  process.user = QStringLiteral("tester");
  process.state = QStringLiteral("R");
  process.threads = 2;
  process.command = QStringLiteral("job --run");
  sample.processes = {process};
  return sample;
}

QVariantMap firstMap(const QVariantMap &snapshot, const QString &key) {
  return snapshot.value(key).toList().constFirst().toMap();
}

} // namespace

class SampleCollectorTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void computesDeltasAndResetsInvalidBaselines();
  void boundsHistory();
};

void SampleCollectorTest::computesDeltasAndResetsInvalidBaselines() {
  SampleCollector collector(QStringLiteral("/does/not/matter"));
  const PublishedSample first = collector.publish(sampleAt(1'000'000'000, 1));
  QCOMPARE(first.snapshot.value(QStringLiteral("cpu")).toDouble(), 60.0);
  QVERIFY(firstMap(first.snapshot, QStringLiteral("disks"))
              .value(QStringLiteral("readRate"))
              .isNull());
  QVERIFY(!first.processes[0].readRate);

  const PublishedSample second = collector.publish(sampleAt(2'000'000'000, 2));
  QCOMPARE(firstMap(second.snapshot, QStringLiteral("disks"))
               .value(QStringLiteral("readRate"))
               .toDouble(),
           1000.0);
  QCOMPARE(firstMap(second.snapshot, QStringLiteral("disks"))
               .value(QStringLiteral("busy"))
               .toDouble(),
           10.0);
  QCOMPARE(firstMap(second.snapshot, QStringLiteral("network"))
               .value(QStringLiteral("rxRate"))
               .toDouble(),
           3000.0);
  QCOMPARE(*second.processes[0].readRate, 100.0);
  const double expectedCpu = 100.0 * 10.0 / double(sysconf(_SC_CLK_TCK) * 2);
  QCOMPARE(*second.processes[0].cpuPercent, expectedCpu);

  RawSample unavailable = sampleAt(3'000'000'000, 3);
  unavailable.processes[0].readBytesAvailable = false;
  const PublishedSample third = collector.publish(std::move(unavailable));
  QVERIFY(!third.processes[0].readRate);
  QCOMPARE(*third.processes[0].writeRate, 200.0);

  RawSample reset = sampleAt(4'000'000'000, 1, 51);
  const PublishedSample fourth = collector.publish(std::move(reset));
  QVERIFY(firstMap(fourth.snapshot, QStringLiteral("disks"))
              .value(QStringLiteral("readRate"))
              .isNull());
  QVERIFY(firstMap(fourth.snapshot, QStringLiteral("network"))
              .value(QStringLiteral("rxRate"))
              .isNull());
  QVERIFY(!fourth.processes[0].cpuPercent);
  QCOMPARE(fourth.snapshot.value(QStringLiteral("history")).toList().size(), 4);
}

void SampleCollectorTest::boundsHistory() {
  SampleCollector collector(QStringLiteral("/does/not/matter"));
  for (int i = 1; i <= 305; ++i) {
    const auto ignored =
        collector.publish(sampleAt(qint64(i) * 1'000'000'000, quint64(i)));
    Q_UNUSED(ignored)
  }
  const auto final = collector.publish(sampleAt(306'000'000'000, 306));
  const QVariantList history =
      final.snapshot.value(QStringLiteral("history")).toList();
  QCOMPARE(history.size(), 300);
  QCOMPARE(history.constLast()
               .toMap()
               .value(QStringLiteral("timestamp"))
               .toLongLong(),
           qint64(306000));
  collector.reset();
  const auto reset = collector.publish(sampleAt(400'000'000'000, 1));
  QCOMPARE(reset.snapshot.value(QStringLiteral("history")).toList().size(), 1);
}

QTEST_GUILESS_MAIN(SampleCollectorTest)
#include "tst_sample_collector.moc"
