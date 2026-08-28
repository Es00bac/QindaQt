// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include <QtTest>

namespace QindaQt::StatusNotifier::TestSupport
{

inline OwnerKey ownerKey(const QString &uniqueName, const QString &path, quint64 generation)
{
    OwnerKey key;
    key.uniqueName = uniqueName;
    key.objectPath = path;
    key.generation = generation;
    return key;
}

inline OwnerKey primaryItemKey(quint64 generation = 1)
{
    return ownerKey(QStringLiteral(":1.10"),
                    QStringLiteral("/org/qindaqt/StatusNotifierItem"),
                    generation);
}

inline ItemDescriptor descriptorWithIdentifier(const QString &identity)
{
    ItemDescriptor descriptor;
    descriptor.category = ItemCategory::SystemServices;
    descriptor.identity = identity;
    descriptor.title = QStringLiteral("Sample item");
    descriptor.status = ItemStatus::Active;
    return descriptor;
}

inline ItemDescriptor validDescriptor()
{
    return descriptorWithIdentifier(QStringLiteral("org.qindaqt.SampleTray"));
}

inline ItemDescriptor descriptorWithHostileMenu()
{
    // An otherwise valid descriptor whose menu exceeds the depth budget:
    // exactly the composed-admission case from the review matrix.
    ItemDescriptor descriptor = validDescriptor();
    MenuEntry level = MenuEntry{};
    level.kind = MenuEntry::Kind::SubMenu;
    level.parentId = -1;
    level.label = QStringLiteral("Level 0");
    descriptor.menu.entries.append(level);
    for (int depth = 1; depth <= kMaxMenuDepth; ++depth) {
        MenuEntry deeper = MenuEntry{};
        deeper.kind = MenuEntry::Kind::SubMenu;
        deeper.parentId = depth - 1;
        deeper.label = QStringLiteral("Level %1").arg(depth);
        descriptor.menu.entries.append(deeper);
    }
    MenuEntry leaf = MenuEntry{};
    leaf.kind = MenuEntry::Kind::Item;
    leaf.parentId = kMaxMenuDepth;
    leaf.label = QStringLiteral("Too deep");
    descriptor.menu.entries.append(leaf);
    return descriptor;
}

inline ItemDescriptor descriptorWithInvalidParentMenu()
{
    ItemDescriptor descriptor = validDescriptor();
    MenuEntry item = MenuEntry{};
    item.kind = MenuEntry::Kind::Item;
    item.parentId = -1;
    item.label = QStringLiteral("Plain");
    descriptor.menu.entries.append(item);

    MenuEntry child = MenuEntry{};
    child.kind = MenuEntry::Kind::Item;
    child.parentId = 0;
    child.label = QStringLiteral("Child under item");
    descriptor.menu.entries.append(child);
    return descriptor;
}

inline ItemDescriptor descriptorWithExceededNodesMenu()
{
    ItemDescriptor descriptor = validDescriptor();
    for (qsizetype index = 0; index <= kMaxMenuNodes; ++index) {
        MenuEntry entry = MenuEntry{};
        entry.kind = MenuEntry::Kind::Item;
        entry.parentId = -1;
        entry.label = QStringLiteral("Item %1").arg(index);
        descriptor.menu.entries.append(entry);
    }
    return descriptor;
}

inline void verifyItemCapacityBoundary()
{
    StatusNotifierRegistry registry;
    const quint64 epoch = registry.beginWatcherEpoch();
    const quint64 generation =
        registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));

    for (qsizetype index = 0; index < kMaxItems; ++index) {
        const OwnerKey key = ownerKey(QStringLiteral(":1.10"),
                                      QStringLiteral("/item/%1").arg(index),
                                      generation);
        QVERIFY(registry
                    .registerItem(epoch, key,
                                  descriptorWithIdentifier(QStringLiteral("item.%1").arg(index)))
                    .accepted());
    }
    QCOMPARE(registry.count(), kMaxItems);

    const OwnerKey existing = ownerKey(QStringLiteral(":1.10"),
                                       QStringLiteral("/item/0"), generation);
    ItemDescriptor replacement = descriptorWithIdentifier(QStringLiteral("item.0"));
    replacement.title = QStringLiteral("Updated at capacity");
    QVERIFY(registry.registerItem(epoch, existing, replacement).accepted());

    const OwnerKey overflow = ownerKey(QStringLiteral(":1.10"),
                                       QStringLiteral("/item/overflow"), generation);
    const RegistryOutcome refused = registry.registerItem(
        epoch, overflow, descriptorWithIdentifier(QStringLiteral("item.overflow")));
    QCOMPARE(refused.status, RegistryStatus::CapacityExceeded);
    QCOMPARE(refused.reasonCode, QStringLiteral("item-capacity-exceeded"));
    QCOMPARE(registry.count(), kMaxItems);
    QVERIFY(!registry.contains(overflow));
    QVERIFY(registry.isDegraded());
}

inline void verifyWatcherEpochReconciliation()
{
    StatusNotifierRegistry registry;
    const quint64 epoch1 = registry.beginWatcherEpoch();
    QCOMPARE(epoch1, quint64(1));

    const quint64 generationA =
        registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10"));
    const quint64 generationB =
        registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.11"));
    const OwnerKey keyA = primaryItemKey(generationA);
    const OwnerKey keyB = ownerKey(QStringLiteral(":1.11"),
                                   QStringLiteral("/org/example/ItemB"),
                                   generationB);

    QVERIFY(registry.registerItem(epoch1, keyA, descriptorWithIdentifier("itemA")).accepted());
    QVERIFY(registry.registerItem(epoch1, keyB, descriptorWithIdentifier("itemB")).accepted());
    QVERIFY(registry.markInitialPopulationComplete(epoch1).accepted());
    QCOMPARE(registry.count(), qsizetype(2));

    const quint64 epoch2 = registry.beginWatcherEpoch();
    QCOMPARE(epoch2, quint64(2));
    QVERIFY(!registry.initialPopulationComplete());

    // Every asynchronous event from the superseded watcher is fenced. None
    // may complete the new population, change owner lineage, or mutate items.
    QCOMPARE(registry.markInitialPopulationComplete(epoch1).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.beginOwnerGeneration(epoch1, QStringLiteral(":1.10")), quint64(0));
    QCOMPARE(registry.currentGeneration(QStringLiteral(":1.10")), generationA);
    QCOMPARE(registry.registerItem(epoch1, keyA, descriptorWithIdentifier("staleA")).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.removeItem(epoch1, keyA).status, RegistryStatus::StaleOwner);
    QCOMPARE(registry.ownerLost(epoch1, QStringLiteral(":1.10"), generationA).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.removeAllForOwner(epoch1, QStringLiteral(":1.10"), generationA).status,
             RegistryStatus::StaleOwner);
    QCOMPARE(registry.count(), qsizetype(2));
    QVERIFY(!registry.initialPopulationComplete());

    // A partial replacement population observed A with a malformed update.
    // Admission rejects the payload, but membership observation retains A's
    // last-known-good descriptor while unseen B is pruned at completion.
    ItemDescriptor malformedA = descriptorWithIdentifier(QStringLiteral("itemA"));
    malformedA.title = QStringLiteral("   ");
    QCOMPARE(registry.registerItem(epoch2, keyA, malformedA).status,
             RegistryStatus::InvalidDescriptor);
    QVERIFY(registry.isDegraded());
    QCOMPARE(registry.removeItem(epoch1, keyA).status, RegistryStatus::StaleOwner);
    QVERIFY(registry.contains(keyA));
    QVERIFY(registry.markInitialPopulationComplete(epoch2).accepted());
    QCOMPARE(registry.count(), qsizetype(1));
    QVERIFY(registry.contains(keyA));
    QVERIFY(!registry.contains(keyB));
    QCOMPARE(registry.find(keyA)->identity, QStringLiteral("itemA"));
    registry.acknowledgeDegraded();

    // A full replacement population can re-observe every live key, including
    // one pruned from the preceding epoch, without changing owner generation.
    const quint64 epoch3 = registry.beginWatcherEpoch();
    QVERIFY(registry.registerItem(epoch3, keyA, descriptorWithIdentifier("itemA")).accepted());
    QVERIFY(registry.registerItem(epoch3, keyB, descriptorWithIdentifier("itemB")).accepted());
    QVERIFY(registry.markInitialPopulationComplete(epoch3).accepted());
    QCOMPARE(registry.count(), qsizetype(2));
    QVERIFY(registry.contains(keyA));
    QVERIFY(registry.contains(keyB));

    // A policy-rejected duplicate-identity update also observes its existing
    // exact key while retaining the prior identity.
    const quint64 epoch4 = registry.beginWatcherEpoch();
    QVERIFY(registry.registerItem(epoch4, keyA, descriptorWithIdentifier("itemA")).accepted());
    QCOMPARE(registry.registerItem(epoch4, keyB, descriptorWithIdentifier("itemA")).status,
             RegistryStatus::DuplicateIdentity);
    QVERIFY(registry.markInitialPopulationComplete(epoch4).accepted());
    QCOMPARE(registry.count(), qsizetype(2));
    QCOMPARE(registry.find(keyB)->identity, QStringLiteral("itemB"));

    // An empty replacement population reconciles every unseen key away.
    const quint64 epoch5 = registry.beginWatcherEpoch();
    QVERIFY(registry.markInitialPopulationComplete(epoch5).accepted());
    QVERIFY(registry.initialPopulationComplete());
    QCOMPARE(registry.count(), qsizetype(0));
}

} // namespace QindaQt::StatusNotifier::TestSupport
