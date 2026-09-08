// SPDX-License-Identifier: GPL-3.0-or-later

#include "procfs_reader.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

using namespace QindaQt::SystemMonitor;

namespace {

void writeFile(const QString &path, const QByteArray &contents) {
  QDir().mkpath(QFileInfo(path).path());
  QFile file(path);
  QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
  QCOMPARE(file.write(contents), contents.size());
}

QByteArray mountField(const QString &path) {
  QByteArray value = QFile::encodeName(path);
  value.replace("\\", "\\134");
  value.replace(" ", "\\040");
  return value;
}

} // namespace

class ProcfsReaderTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void readsHostAndRichProcessFixture();
  void rejectsMalformedProcessIdentity();
};

void ProcfsReaderTest::readsHostAndRichProcessFixture() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  const QString root = fixture.path();
  writeFile(root + "/stat", "cpu 100 20 30 400 10 5 5 2 99 77\n"
                            "cpu0 50 10 15 200 5 2 3 1 40 20\n");
  writeFile(root + "/meminfo",
            "MemTotal: 1000 kB\nMemAvailable: 600 kB\nCached: 120 kB\n"
            "SReclaimable: 30 kB\nSwapTotal: 500 kB\nSwapFree: 300 kB\n");
  writeFile(root + "/uptime", "42.5 20.0\n");
  writeFile(root + "/loadavg", "1.25 2.50 3.75 1/100 123\n");
  writeFile(root + "/diskstats",
            "8 0 sda 10 0 20 4 30 0 40 5 0 600 0 0 0 0 0\n"
            "8 1 sda1 5 0 10 2 15 0 20 3 0 300 0 0 0 0 0\n");
  writeFile(root + "/net/dev",
            "Inter-| Receive | Transmit\n face |bytes packets errs drop fifo "
            "frame compressed multicast|bytes packets errs drop fifo colls "
            "carrier compressed\n"
            " eth0: 1000 1 0 0 0 0 0 0 2000 2 0 0 0 0 0 0\n");
  writeFile(root + "/mounts", "tmpfs " + mountField(root) +
                                  " tmpfs rw 0 0\n"
                                  "server:/slow /remote nfs rw 0 0\n");

  const QByteArray statLine =
      "123 (worker name) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 5 3 0 999 "
      "100000 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 4\n";
  writeFile(root + "/123/stat", statLine);
  writeFile(root + "/123/status", "Name:\tworker name\nUid:\t" +
                                      QByteArray::number(getuid()) +
                                      "\t0\t0\t0\nVmRSS:\t64 kB\n");
  writeFile(root + "/123/io", "read_bytes: 4096\nwrite_bytes: 8192\n");
  writeFile(root + "/123/cmdline", QByteArray("worker\0--task\0", 14));

  QString error;
  const RawSample sample = ProcfsReader(root).read(&error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(sample.cpu.total, quint64(572));
  QCOMPARE(sample.cpu.idle, quint64(410));
  QCOMPARE(sample.cores.size(), 1);
  QCOMPARE(sample.cores[0].id, QStringLiteral("0"));
  QCOMPARE(sample.memoryTotal, quint64(1000 * 1024));
  QCOMPARE(sample.memoryAvailable, quint64(600 * 1024));
  QCOMPARE(sample.memoryCached, quint64(150 * 1024));
  QCOMPARE(sample.swapFree, quint64(300 * 1024));
  QCOMPARE(sample.uptimeSeconds, 42.5);
  QCOMPARE(sample.load5, 2.5);
  QCOMPARE(sample.disks.size(), 1);
  QCOMPARE(sample.disks[0].name, QStringLiteral("sda"));
  QCOMPARE(sample.disks[0].readBytes, quint64(20 * 512));
  QCOMPARE(sample.network.size(), 1);
  QCOMPARE(sample.network[0].txBytes, quint64(2000));
  QCOMPARE(sample.filesystems.size(), 1);
  QCOMPARE(sample.filesystems[0].path, root);

  QCOMPARE(sample.processes.size(), 1);
  const auto &process = sample.processes[0];
  QCOMPARE(process.pid, qint64(123));
  QCOMPARE(process.name, QStringLiteral("worker name"));
  QCOMPARE(process.state, QStringLiteral("S"));
  QCOMPARE(process.cpuTicks, quint64(19));
  QCOMPARE(process.startTicks, quint64(999));
  QCOMPARE(process.threads, 3);
  QCOMPARE(process.nice, 5);
  QCOMPARE(process.memoryBytes, quint64(64 * 1024));
  QVERIFY(process.memoryAvailable);
  QVERIFY(process.readBytesAvailable);
  QVERIFY(process.writeBytesAvailable);
  QCOMPARE(process.readBytes, quint64(4096));
  QCOMPARE(process.command, QStringLiteral("worker --task"));

  writeFile(root + "/123/io", "read_bytes: 12288\n");
  const auto partial = ProcfsReader(root).readProcess(123, &error);
  QVERIFY2(partial, qPrintable(error));
  QVERIFY(partial->readBytesAvailable);
  QVERIFY(!partial->writeBytesAvailable);
  QCOMPARE(partial->readBytes, quint64(12288));
}

void ProcfsReaderTest::rejectsMalformedProcessIdentity() {
  QTemporaryDir fixture;
  QVERIFY(fixture.isValid());
  writeFile(fixture.path() + "/7/stat", "7 missing-fields\n");
  QString error;
  QVERIFY(!ProcfsReader(fixture.path()).readProcess(7, &error));
  QVERIFY(error.contains(QStringLiteral("Malformed")));
}

QTEST_GUILESS_MAIN(ProcfsReaderTest)
#include "tst_procfs_reader.moc"
