// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_protocol_test_data.h"

#include <qindaqt/services/network_model/network_scan_lease.h>

#include <QtTest>

#include <limits>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Model;
using namespace QindaQt::Network::TestData;

class NetworkScanLeaseTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void adoptsLeaseFromMatchingEpoch();
  void refusesLeaseFromForeignEpoch();
  void expiresAtDeadline();
  void reportsRemainingTime();
  void rejectsUnboundedOrOverflowingDuration();
  void releaseClearsLease();

private:
  static qint64 fixed() { return 0; }
};

void NetworkScanLeaseTests::adoptsLeaseFromMatchingEpoch() {
  ScanLeaseTracker tracker;
  QVERIFY(!tracker.lease().has_value());
  const ScanLease lease{QStringLiteral("lease-1"), 41, 7, 1'000};
  QVERIFY(tracker.adopt(lease, 41, MonotonicClock([] { return 10'000; })));
  QVERIFY(tracker.lease().has_value());
  QCOMPARE(*tracker.lease(), lease);
}

void NetworkScanLeaseTests::refusesLeaseFromForeignEpoch() {
  ScanLeaseTracker tracker;
  const ScanLease staleLease{QStringLiteral("lease-1"), 40, 7, 1'000};
  QVERIFY(!tracker.adopt(staleLease, 41, MonotonicClock([] { return 0; })));
  QVERIFY(!tracker.lease().has_value());
}

void NetworkScanLeaseTests::expiresAtDeadline() {
  ScanLeaseTracker tracker;
  QVERIFY(tracker.adopt(ScanLease{QStringLiteral("lease-1"), 41, 7, 5'000},
                        41, MonotonicClock([] { return 0; })));
  QVERIFY(tracker.active(MonotonicClock([] { return 4'999; })));
  QVERIFY(!tracker.expired(MonotonicClock([] { return 4'999; })));
  // The deadline itself is expired: a lease valid "until" ms 5000 is not
  // usable at 5000.
  QVERIFY(tracker.expired(MonotonicClock([] { return 5'000; })));
  QVERIFY(!tracker.active(MonotonicClock([] { return 5'000; })));
  QVERIFY(tracker.expired(MonotonicClock([] { return 9'999; })));
}

void NetworkScanLeaseTests::reportsRemainingTime() {
  ScanLeaseTracker tracker;
  QVERIFY(tracker.adopt(ScanLease{QStringLiteral("lease-1"), 41, 7, 5'000},
                        41, MonotonicClock([] { return 0; })));
  QCOMPARE(tracker.remainingMs(MonotonicClock([] { return 4'000; })), 1'000);
  QCOMPARE(tracker.remainingMs(MonotonicClock([] { return 5'000; })), 0);
  QCOMPARE(tracker.remainingMs(MonotonicClock([] { return 6'000; })), 0);

  ScanLeaseTracker empty;
  QCOMPARE(empty.remainingMs(MonotonicClock(&fixed)), 0);
  QVERIFY(!empty.active(MonotonicClock(&fixed)));
}

void NetworkScanLeaseTests::rejectsUnboundedOrOverflowingDuration() {
  ScanLeaseTracker tracker;
  const MonotonicClock zero([] { return 0; });
  QVERIFY(!tracker.adopt(
      ScanLease{QStringLiteral("short"), 41, 7,
                kMinimumScanDeadlineMilliseconds - 1},
      41, zero));
  QVERIFY(!tracker.adopt(
      ScanLease{QStringLiteral("long"), 41, 7,
                kMaximumScanDeadlineMilliseconds + 1},
      41, zero));
  QVERIFY(!tracker.adopt(
      ScanLease{QStringLiteral("max"), 41, 7,
                std::numeric_limits<qint64>::max()},
      41, zero));
  QVERIFY(!tracker.adopt(
      ScanLease{QStringLiteral("overflow"), 41, 7,
                kMinimumScanDeadlineMilliseconds},
      41, MonotonicClock([] {
        return std::numeric_limits<qint64>::max() - 100;
      })));
  QVERIFY(!tracker.lease().has_value());
}

void NetworkScanLeaseTests::releaseClearsLease() {
  ScanLeaseTracker tracker;
  QVERIFY(tracker.adopt(ScanLease{QStringLiteral("lease-1"), 41, 7, 5'000},
                        41, MonotonicClock([] { return 0; })));
  tracker.release();
  QVERIFY(!tracker.lease().has_value());
  QVERIFY(!tracker.active(MonotonicClock([] { return 0; })));
  tracker.release();
  QVERIFY(!tracker.lease().has_value());
}

QTEST_MAIN(NetworkScanLeaseTests)
#include "tst_network_scan_lease.moc"
