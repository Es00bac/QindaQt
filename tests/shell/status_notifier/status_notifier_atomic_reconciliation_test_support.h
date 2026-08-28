// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "status_notifier_registry_test_support.h"

#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>

namespace QindaQt::StatusNotifier::TestSupport
{

inline OwnerKey indexedItemKey(const qsizetype index, const quint64 generation)
{
    return ownerKey(QStringLiteral(":1.10"),
                    QStringLiteral("/item/%1").arg(index), generation);
}

inline ItemDescriptor indexedItemDescriptor(const qsizetype index)
{
    return descriptorWithIdentifier(QStringLiteral("item.%1").arg(index));
}

inline void registerIndexedItems(StatusNotifierRegistry &registry, const quint64 epoch,
                                 const quint64 generation, const qsizetype count)
{
    for (qsizetype index = 0; index < count; ++index) {
        QVERIFY(registry.registerItem(epoch, indexedItemKey(index, generation),
                                      indexedItemDescriptor(index)).accepted());
    }
}

inline void verifyInterruptedInitialIdentityHandover()
{
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
    QVERIFY(!registry.initialPopulationComplete());

    const quint64 epoch2 = registry.beginWatcherEpoch();
    QCOMPARE(registry.count(), qsizetype(0));
    QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
             PresentationState::Loading);
    QCOMPARE(registry.registerItem(epoch1, oldKey, descriptor).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.markInitialPopulationComplete(epoch1).status,
             RegistryStatus::StaleOwner);
    QVERIFY(registry.registerItem(epoch2, newKey, descriptor).accepted());
    QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
             PresentationState::Loading);
    QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
    QVERIFY(!registry.contains(oldKey));
    QVERIFY(registry.contains(newKey));
    QCOMPARE(registry.count(), qsizetype(1));
}

inline void verifyCompletedSameOwnerHandover()
{
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
}

inline void verifyCompletedCrossOwnerHandover()
{
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
}

inline void verifyMalformedNewItemDoesNotPoisonTarget()
{
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
}

inline void verifyConflictingPopulationRetainsLkg(const bool oldFirst)
{
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

    const quint64 recoveryEpoch = registry.beginWatcherEpoch();
    QVERIFY(registry.registerItem(recoveryEpoch, newKey, descriptor).accepted());
    QVERIFY(registry.markInitialPopulationComplete(recoveryEpoch).accepted());
    QVERIFY(!registry.contains(oldKey));
    QVERIFY(registry.contains(newKey));
}

inline void verifyAtomicIdentityHandovers()
{
    verifyInterruptedInitialIdentityHandover();
    verifyCompletedSameOwnerHandover();
    verifyCompletedCrossOwnerHandover();
    verifyMalformedNewItemDoesNotPoisonTarget();
    verifyConflictingPopulationRetainsLkg(true);
    verifyConflictingPopulationRetainsLkg(false);
}

inline void verifyInterruptedInitialCapacityReplacement(const bool replacementFirst)
{
    StatusNotifierRegistry registry;
    const quint64 epoch1 = registry.beginWatcherEpoch();
    const quint64 generation =
        registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
    registerIndexedItems(registry, epoch1, generation, kMaxItems);
    const OwnerKey abandonedOverflow = ownerKey(QStringLiteral(":1.10"),
                                                QStringLiteral("/abandoned_overflow"), generation);
    QCOMPARE(registry.registerItem(epoch1, abandonedOverflow,
                                   descriptorWithIdentifier("abandoned.overflow")).status,
             RegistryStatus::CapacityExceeded);
    QVERIFY(registry.isDegraded());

    const OwnerKey displaced = indexedItemKey(kMaxItems - 1, generation);
    const OwnerKey replacement = ownerKey(QStringLiteral(":1.10"),
                                          QStringLiteral("/replacement"), generation);
    const ItemDescriptor replacementDescriptor = indexedItemDescriptor(kMaxItems - 1);
    const quint64 epoch2 = registry.beginWatcherEpoch();
    QCOMPARE(registry.count(), qsizetype(0));
    QVERIFY(!registry.isDegraded());
    QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
             PresentationState::Loading);
    if (replacementFirst) {
        QVERIFY(registry.registerItem(epoch2, replacement, replacementDescriptor).accepted());
    }
    registerIndexedItems(registry, epoch2, generation, kMaxItems - 1);
    if (!replacementFirst) {
        QVERIFY(registry.registerItem(epoch2, replacement, replacementDescriptor).accepted());
    }
    QCOMPARE(registry.registerItem(epoch1, displaced,
                                   indexedItemDescriptor(kMaxItems - 1)).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.count(), kMaxItems);
    QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
    QCOMPARE(registry.count(), kMaxItems);
    QVERIFY(!registry.contains(displaced));
    QVERIFY(registry.contains(replacement));
}

inline void verifyCompletedCapacityReplacement(const bool replacementFirst)
{
    StatusNotifierRegistry registry;
    const quint64 epoch1 = registry.beginWatcherEpoch();
    const quint64 generation =
        registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
    registerIndexedItems(registry, epoch1, generation, kMaxItems);
    QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

    const OwnerKey displaced = indexedItemKey(kMaxItems - 1, generation);
    const OwnerKey replacement = ownerKey(QStringLiteral(":1.10"),
                                          QStringLiteral("/replacement"), generation);
    const ItemDescriptor replacementDescriptor = indexedItemDescriptor(kMaxItems - 1);
    const quint64 epoch2 = registry.beginWatcherEpoch();
    if (replacementFirst) {
        QVERIFY(registry.registerItem(epoch2, replacement, replacementDescriptor).accepted());
    }
    registerIndexedItems(registry, epoch2, generation, kMaxItems - 1);
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
}

inline void verifyInvalidCapacityTargetRetainsLkgAndRecovers()
{
    StatusNotifierRegistry registry;
    const quint64 epoch1 = registry.beginWatcherEpoch();
    const quint64 generation =
        registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
    registerIndexedItems(registry, epoch1, generation, kMaxItems);
    QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());

    const OwnerKey overflow = ownerKey(QStringLiteral(":1.10"),
                                       QStringLiteral("/overflow"), generation);
    const quint64 invalidEpoch = registry.beginWatcherEpoch();
    registerIndexedItems(registry, invalidEpoch, generation, kMaxItems);
    QCOMPARE(registry.registerItem(invalidEpoch, overflow,
                                   descriptorWithIdentifier("item.overflow")).status,
             RegistryStatus::CapacityExceeded);
    QCOMPARE(registry.markInitialPopulationComplete(invalidEpoch).status,
             RegistryStatus::CapacityExceeded);
    QCOMPARE(registry.count(), kMaxItems);
    QVERIFY(!registry.contains(overflow));

    const quint64 recoveryEpoch = registry.beginWatcherEpoch();
    registerIndexedItems(registry, recoveryEpoch, generation, kMaxItems - 1);
    QVERIFY(registry.registerItem(recoveryEpoch, overflow,
                                  descriptorWithIdentifier("item.overflow")).accepted());
    QVERIFY(registry.markInitialPopulationComplete(recoveryEpoch).accepted());
    QCOMPARE(registry.count(), kMaxItems);
    QVERIFY(registry.contains(overflow));
}

inline void verifyCapacityBoundAtomicReplacement()
{
    verifyInterruptedInitialCapacityReplacement(true);
    verifyInterruptedInitialCapacityReplacement(false);
    verifyCompletedCapacityReplacement(true);
    verifyCompletedCapacityReplacement(false);
    verifyInvalidCapacityTargetRetainsLkgAndRecovers();
}

} // namespace QindaQt::StatusNotifier::TestSupport
