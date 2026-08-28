// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "status_notifier_registry_test_support.h"

namespace QindaQt::StatusNotifier::TestSupport
{

inline void verifyAtomicIdentityHandovers()
{
    const auto sameOwnerHandover = [] {
        StatusNotifierRegistry registry;
        const quint64 epoch1 = registry.beginWatcherEpoch();
        const quint64 generation =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
        const OwnerKey oldKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/old"), generation);
        const OwnerKey newKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/new"), generation);
        const ItemDescriptor descriptor = descriptorWithIdentifier("stable.identity");
        QVERIFY(registry.registerItem(epoch1, oldKey, descriptor).accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

        const quint64 epoch2 = registry.beginWatcherEpoch();
        QVERIFY(registry.registerItem(epoch2, newKey, descriptor).accepted());
        QVERIFY(registry.contains(oldKey));
        QVERIFY(!registry.contains(newKey));
        QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
        QVERIFY(!registry.contains(oldKey));
        QVERIFY(registry.contains(newKey));
        QCOMPARE(registry.count(), qsizetype(1));
    };
    sameOwnerHandover();

    const auto crossOwnerHandover = [] {
        StatusNotifierRegistry registry;
        const quint64 epoch1 = registry.beginWatcherEpoch();
        const quint64 oldGeneration =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
        const quint64 newGeneration =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.11"));
        const OwnerKey oldKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/item"), oldGeneration);
        const OwnerKey newKey = ownerKey(QStringLiteral(":1.11"),
                                         QStringLiteral("/item"), newGeneration);
        const ItemDescriptor descriptor = descriptorWithIdentifier("stable.identity");
        QVERIFY(registry.registerItem(epoch1, oldKey, descriptor).accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

        const quint64 epoch2 = registry.beginWatcherEpoch();
        QVERIFY(registry.registerItem(epoch2, newKey, descriptor).accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
        QVERIFY(!registry.contains(oldKey));
        QVERIFY(registry.contains(newKey));
    };
    crossOwnerHandover();

    const auto malformedNewItemDoesNotPoisonTarget = [] {
        StatusNotifierRegistry registry;
        const quint64 epoch1 = registry.beginWatcherEpoch();
        const quint64 generation =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
        QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

        const quint64 epoch2 = registry.beginWatcherEpoch();
        const OwnerKey newKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/new"), generation);
        QCOMPARE(registry.registerItem(
                     epoch2, newKey, descriptorWithIdentifier(QStringLiteral("   "))).status,
                 RegistryStatus::InvalidDescriptor);
        QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QVERIFY(!registry.isDegraded());
    };
    malformedNewItemDoesNotPoisonTarget();

    const auto conflictingPopulationRetainsLkg = [](const bool oldFirst) {
        StatusNotifierRegistry registry;
        const quint64 epoch1 = registry.beginWatcherEpoch();
        const quint64 generation =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
        const OwnerKey oldKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/old"), generation);
        const OwnerKey newKey = ownerKey(QStringLiteral(":1.10"),
                                         QStringLiteral("/new"), generation);
        const ItemDescriptor descriptor = descriptorWithIdentifier("stable.identity");
        QVERIFY(registry.registerItem(epoch1, oldKey, descriptor).accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

        const quint64 epoch2 = registry.beginWatcherEpoch();
        const OwnerKey first = oldFirst ? oldKey : newKey;
        const OwnerKey second = oldFirst ? newKey : oldKey;
        QVERIFY(registry.registerItem(epoch2, first, descriptor).accepted());
        QCOMPARE(registry.registerItem(epoch2, second, descriptor).status,
                 RegistryStatus::DuplicateIdentity);
        QCOMPARE(registry.markInitialPopulationComplete(epoch2).status,
                 RegistryStatus::DuplicateIdentity);
        QVERIFY(!registry.initialPopulationComplete());
        QVERIFY(registry.contains(oldKey));
        QVERIFY(!registry.contains(newKey));
        QCOMPARE(registry.count(), qsizetype(1));
    };
    conflictingPopulationRetainsLkg(true);
    conflictingPopulationRetainsLkg(false);
}

inline void verifyCapacityBoundAtomicReplacement()
{
    const auto runPermutation = [](const bool replacementFirst) {
        StatusNotifierRegistry registry;
        const quint64 epoch1 = registry.beginWatcherEpoch();
        const quint64 generation =
            registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
        for (qsizetype index = 0; index < kMaxItems; ++index) {
            QVERIFY(registry
                        .registerItem(epoch1,
                                      ownerKey(QStringLiteral(":1.10"),
                                               QStringLiteral("/item/%1").arg(index), generation),
                                      descriptorWithIdentifier(
                                          QStringLiteral("item.%1").arg(index)))
                        .accepted());
        }
        QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

        const OwnerKey displaced = ownerKey(
            QStringLiteral(":1.10"), QStringLiteral("/item/%1").arg(kMaxItems - 1), generation);
        const OwnerKey replacement = ownerKey(QStringLiteral(":1.10"),
                                              QStringLiteral("/replacement"), generation);
        const ItemDescriptor replacementDescriptor =
            descriptorWithIdentifier(QStringLiteral("item.%1").arg(kMaxItems - 1));
        const quint64 epoch2 = registry.beginWatcherEpoch();
        if (replacementFirst) {
            QVERIFY(registry.registerItem(epoch2, replacement, replacementDescriptor).accepted());
        }
        for (qsizetype index = 0; index < kMaxItems - 1; ++index) {
            QVERIFY(registry
                        .registerItem(epoch2,
                                      ownerKey(QStringLiteral(":1.10"),
                                               QStringLiteral("/item/%1").arg(index), generation),
                                      descriptorWithIdentifier(
                                          QStringLiteral("item.%1").arg(index)))
                        .accepted());
        }
        if (!replacementFirst) {
            QVERIFY(registry.registerItem(epoch2, replacement, replacementDescriptor).accepted());
        }

        QCOMPARE(registry.count(), kMaxItems);
        QVERIFY(registry.contains(displaced));
        QVERIFY(!registry.contains(replacement));
        QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
        QCOMPARE(registry.count(), kMaxItems);
        QVERIFY(!registry.contains(displaced));
        QVERIFY(registry.contains(replacement));
    };
    runPermutation(true);
    runPermutation(false);
}

} // namespace QindaQt::StatusNotifier::TestSupport
