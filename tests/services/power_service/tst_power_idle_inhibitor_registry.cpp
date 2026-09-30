// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../src/services/power_service/src/idle_inhibitor_registry_p.h"

#include <QtTest/QTest>

using namespace QindaQt::Power;

class PowerIdleInhibitorRegistryTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void rejectsAnyUnconsumedScopeAtomically() {
    IdleInhibitorRegistry registry(9, IdleInhibitorScope::AutomaticLock |
                                          IdleInhibitorScope::DisplayOff);

    const auto result = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("video playback"),
        IdleInhibitorScope::AutomaticLock | IdleInhibitorScope::IdleSuspend);

    QCOMPARE(result.status, IdleInhibitorAcquireStatus::Unsupported);
    QVERIFY(!result.handle.isValid());
    QCOMPARE(registry.leaseCount(), 0);
    QVERIFY(!registry.isInhibited(IdleInhibitorScope::AutomaticLock));
  }

  void acquiresAndReleasesOnlyForSameUniqueOwner() {
    IdleInhibitorRegistry registry(9, IdleInhibitorScope::AutomaticLock |
                                          IdleInhibitorScope::DisplayOff);
    const auto acquired = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("video playback"),
        IdleInhibitorScope::AutomaticLock | IdleInhibitorScope::DisplayOff);

    QCOMPARE(acquired.status, IdleInhibitorAcquireStatus::Accepted);
    QVERIFY(acquired.handle.isValid());
    QVERIFY(registry.isInhibited(IdleInhibitorScope::AutomaticLock));
    QVERIFY(registry.isInhibited(IdleInhibitorScope::DisplayOff));
    QVERIFY(!registry.isInhibited(IdleInhibitorScope::IdleSuspend));
    QVERIFY(!registry.release(QStringLiteral(":1.21"), acquired.handle));
    QCOMPARE(registry.leaseCount(), 1);
    QVERIFY(registry.release(QStringLiteral(":1.20"), acquired.handle));
    QVERIFY(!registry.isInhibited(IdleInhibitorScope::AutomaticLock));
  }

  void ownerLossAndEpochReplacementRevokeLeases() {
    IdleInhibitorRegistry registry(9, IdleInhibitorScope::AutomaticLock);
    const auto first = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);
    QVERIFY(first.handle.isValid());
    registry.ownerVanished(QStringLiteral(":1.20"));
    QCOMPARE(registry.leaseCount(), 0);

    const auto second = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);
    QVERIFY(second.handle.isValid());
    registry.setEpoch(10);
    QCOMPARE(registry.leaseCount(), 0);
    QVERIFY(!registry.release(QStringLiteral(":1.20"), second.handle));
  }

  void validatesTextAndScopeBeforeMutation() {
    IdleInhibitorRegistry registry(9, IdleInhibitorScope::AutomaticLock);
    const auto emptyScopes =
        registry.acquire(QStringLiteral(":1.20"), QStringLiteral("browser"),
                         QStringLiteral("video playback"), {});
    const auto controlText = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("bad\nreason"), IdleInhibitorScope::AutomaticLock);
    const auto invalidOwner = registry.acquire(
        QStringLiteral("browser"), QStringLiteral("browser"),
        QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);

    QCOMPARE(emptyScopes.status, IdleInhibitorAcquireStatus::Invalid);
    QCOMPARE(controlText.status, IdleInhibitorAcquireStatus::Invalid);
    QCOMPARE(invalidOwner.status, IdleInhibitorAcquireStatus::Invalid);
    QCOMPARE(registry.leaseCount(), 0);
  }

  void enforcesGlobalAndPerOwnerBounds() {
    IdleInhibitorRegistry registry(9, IdleInhibitorScope::AutomaticLock);
    for (qsizetype index = 0; index < IdleInhibitorRegistry::MaxLeasesPerOwner;
         ++index) {
      const auto result = registry.acquire(
          QStringLiteral(":1.20"), QStringLiteral("browser"),
          QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);
      QCOMPARE(result.status, IdleInhibitorAcquireStatus::Accepted);
    }
    const auto ownerLimit = registry.acquire(
        QStringLiteral(":1.20"), QStringLiteral("browser"),
        QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);
    QCOMPARE(ownerLimit.status, IdleInhibitorAcquireStatus::Capacity);
    QCOMPARE(registry.leaseCount(), IdleInhibitorRegistry::MaxLeasesPerOwner);

    for (qsizetype index = IdleInhibitorRegistry::MaxLeasesPerOwner;
         index < IdleInhibitorRegistry::MaxLeases; ++index) {
      const QString owner = QStringLiteral(":1.%1").arg(20 + index);
      const auto result = registry.acquire(owner, QStringLiteral("browser"),
                                           QStringLiteral("video playback"),
                                           IdleInhibitorScope::AutomaticLock);
      QCOMPARE(result.status, IdleInhibitorAcquireStatus::Accepted);
    }
    const auto globalLimit = registry.acquire(
        QStringLiteral(":1.999"), QStringLiteral("browser"),
        QStringLiteral("video playback"), IdleInhibitorScope::AutomaticLock);
    QCOMPARE(globalLimit.status, IdleInhibitorAcquireStatus::Capacity);
    QCOMPARE(registry.leaseCount(), IdleInhibitorRegistry::MaxLeases);
  }
};

QTEST_GUILESS_MAIN(PowerIdleInhibitorRegistryTests)
#include "tst_power_idle_inhibitor_registry.moc"
