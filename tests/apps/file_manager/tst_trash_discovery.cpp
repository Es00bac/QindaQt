// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/trash_discovery.h"
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <qscopeguard.h>
using namespace QindaQt::Apps::FileManager;
class TrashDiscoveryTests final : public QObject {
  Q_OBJECT
private slots:
  void latestPendingRequestIsCoalescedAndOldTailSkipped() {
    QSemaphore entered, release;
    std::atomic<int> probes = 0;
    std::atomic_bool skippedWasProbed = false;
    TrashDiscovery discovery(nullptr, [&](const QString &root) {
      ++probes;
      if (root == QStringLiteral("/hold")) { entered.release(); release.acquire(); }
      if (root == QStringLiteral("/old-tail")) skippedWasProbed.store(true);
      return QStringList{root + QStringLiteral("/files")};
    });
    const auto cleanup = qScopeGuard([&] { release.release(); });
    QSignalSpy completed(&discovery, &TrashDiscovery::completed);
    discovery.request(1, {QStringLiteral("/hold"), QStringLiteral("/old-tail")});
    QVERIFY(entered.tryAcquire(1, 2000));
    for (quint64 generation = 2; generation <= 1001; ++generation)
      discovery.request(generation, {QStringLiteral("/latest")});
    release.release();
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
    QCOMPARE(completed[0][0].toULongLong(), quint64(1001));
    QCOMPARE(completed[0][1].toMap().value(QStringLiteral("/latest")).toStringList(),
             QStringList{QStringLiteral("/latest/files")});
    QCOMPARE(probes.load(), 2); QVERIFY(!skippedWasProbed.load());
  }
  void boundedRootsAndNonlocalRefusalNeverProbe() {
    std::atomic<int> probes = 0;
    TrashDiscovery discovery(nullptr, [&](const QString &) { ++probes; return QStringList{}; });
    QSignalSpy completed(&discovery, &TrashDiscovery::completed);
    QStringList roots;
    for (int i = 0; i < 257; ++i) roots.append(QStringLiteral("/root/%1").arg(i));
    discovery.request(1, roots); QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
    QCOMPARE(probes.load(), 0); QVERIFY(!completed[0][2].toString().isEmpty());
    completed.clear();
    discovery.request(2, {QStringLiteral("sftp://host/path"), QStringLiteral("/a/../b")});
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
    QCOMPARE(probes.load(), 0); QVERIFY(!completed[0][2].toString().isEmpty());
  }
  void duplicateRootsHaveOneReadOnlyProbe() {
    std::atomic<int> probes = 0;
    TrashDiscovery discovery(nullptr, [&](const QString &root) {
      ++probes; return QStringList{root + QStringLiteral("/.Trash/files")};
    });
    QSignalSpy completed(&discovery, &TrashDiscovery::completed);
    discovery.request(1, {QStringLiteral("/volume"), QStringLiteral("/volume")});
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
    QCOMPARE(probes.load(), 1); QCOMPARE(completed[0][1].toMap().size(), 1);
  }
};
QTEST_GUILESS_MAIN(TrashDiscoveryTests)
#include "tst_trash_discovery.moc"
