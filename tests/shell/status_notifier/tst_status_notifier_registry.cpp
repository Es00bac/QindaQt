// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include <QtTest>

using namespace QindaQt::StatusNotifier;

namespace
{

OwnerKey ownerKey(const QString &uniqueName, const QString &path, quint32 generation)
{
    OwnerKey key;
    key.uniqueName = uniqueName;
    key.objectPath = path;
    key.generation = generation;
    return key;
}

OwnerKey primaryItemKey(quint32 generation = 1)
{
    return ownerKey(QStringLiteral(":1.10"),
                    QStringLiteral("/org/qindaqt/StatusNotifierItem"),
                    generation);
}

ItemDescriptor descriptorWithIdentifier(const QString &identity)
{
    ItemDescriptor descriptor;
    descriptor.category = ItemCategory::SystemServices;
    descriptor.identity = identity;
    descriptor.title = QStringLiteral("Sample item");
    descriptor.status = ItemStatus::Active;
    return descriptor;
}

ItemDescriptor validDescriptor()
{
    return descriptorWithIdentifier(QStringLiteral("org.qindaqt.SampleTray"));
}

} // namespace

class StatusNotifierRegistryTests final : public QObject
{
    Q_OBJECT

private slots:
    void keysItemsByExactOwner()
    {
        StatusNotifierRegistry registry;
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint32(1));

        const RegistryOutcome outcome = registry.registerItem(primaryItemKey(), validDescriptor());
        QVERIFY(outcome.accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(registry.contains(primaryItemKey()));
        QCOMPARE(registry.find(primaryItemKey()).value().identity,
                 QStringLiteral("org.qindaqt.SampleTray"));

        // A different path on the same owner is a distinct item; a different
        // owner on the same path is equally distinct. Only the exact
        // (uniqueName, objectPath) pair addresses one item.
        const OwnerKey secondPath =
            ownerKey(QStringLiteral(":1.10"), QStringLiteral("/org/qindaqt/SecondItem"), 1);
        QVERIFY(registry.registerItem(secondPath, descriptorWithIdentifier("second")).accepted());
        QCOMPARE(registry.count(), qsizetype(2));
    }

    void replacesSameOwnerItemAndKeepsIdentityIndexConsistent()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(), validDescriptor()).accepted());

        // Replacement by the live owner is the supported update path.
        ItemDescriptor replacement = descriptorWithIdentifier(QStringLiteral("renamed.identity"));
        replacement.status = ItemStatus::NeedsAttention;
        QVERIFY(registry.registerItem(primaryItemKey(), replacement).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.find(primaryItemKey()).value().status, ItemStatus::NeedsAttention);

        // The old identity must be free; a second owner may now claim it.
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.11")) != 0);
        const OwnerKey otherOwner =
            ownerKey(QStringLiteral(":1.11"),
                     QStringLiteral("/org/example/StatusNotifierItem"),
                     1);
        QVERIFY(registry.registerItem(otherOwner, validDescriptor()).accepted());
        QCOMPARE(registry.count(), qsizetype(2));
    }

    void removesItems()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(), validDescriptor()).accepted());
        QVERIFY(registry.removeItem(primaryItemKey()).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QCOMPARE(registry.removeItem(primaryItemKey()).status, RegistryStatus::UnknownItem);

        // The freed identity can be claimed by another live owner.
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.11")) != 0);
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             1);
        QVERIFY(registry.registerItem(otherOwner, validDescriptor()).accepted());
    }

    void rejectsSpoofedOwnerNames()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);

        // A well-known name masquerading as the owner is rejected outright.
        const OwnerKey spoofedWellKnown = ownerKey(QStringLiteral("org.example.Tray"),
                                                   QStringLiteral("/org/example/StatusNotifierItem"),
                                                   1);
        QCOMPARE(registry.registerItem(spoofedWellKnown, validDescriptor()).status,
                 RegistryStatus::InvalidOwner);
        QCOMPARE(registry.count(), qsizetype(0));

        // An event that never observed a generation for its owner is stale,
        // not registrable: keying is meaningless without the transport first
        // reporting the name's arrival.
        const OwnerKey neverSeen = ownerKey(QStringLiteral(":1.99"),
                                            QStringLiteral("/org/qindaqt/StatusNotifierItem"),
                                            3);
        QCOMPARE(registry.registerItem(neverSeen, validDescriptor()).status,
                 RegistryStatus::StaleOwner);
    }

    void rejectsDuplicateIdentityAcrossLiveOwners()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.11")) != 0);
        const OwnerKey first = primaryItemKey();
        const OwnerKey second = ownerKey(QStringLiteral(":1.11"),
                                         QStringLiteral("/org/example/StatusNotifierItem"),
                                         1);
        QVERIFY(registry.registerItem(first, validDescriptor()).accepted());

        const RegistryOutcome duplicate = registry.registerItem(second, validDescriptor());
        QCOMPARE(duplicate.status, RegistryStatus::DuplicateIdentity);
        QCOMPARE(duplicate.reasonCode, QStringLiteral("identity-claimed"));
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(!registry.contains(second));
        QCOMPARE(registry.find(first).value().identity,
                 QStringLiteral("org.qindaqt.SampleTray"));
    }

    void rejectsStaleReplyAfterOwnerLost()
    {
        StatusNotifierRegistry registry;
        const quint32 firstGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());

        registry.ownerLost(QStringLiteral(":1.10"));
        QCOMPARE(registry.count(), qsizetype(0));

        // A late in-flight reply from before the disconnect must not
        // resurrect the removed item.
        const RegistryOutcome stale = registry.registerItem(primaryItemKey(firstGeneration),
                                                            validDescriptor());
        QCOMPARE(stale.status, RegistryStatus::StaleOwner);
        QCOMPARE(stale.reasonCode, QStringLiteral("generation-not-current"));
        QCOMPARE(registry.count(), qsizetype(0));

        const RegistryOutcome staleRemoval = registry.removeItem(primaryItemKey(firstGeneration));
        QCOMPARE(staleRemoval.status, RegistryStatus::StaleOwner);
        const RegistryOutcome staleRequest =
            registry.evaluateRequest(primaryItemKey(firstGeneration), RequestKind::Activate);
        QCOMPARE(staleRequest.status, RegistryStatus::StaleOwner);
    }

    void restartAdvancesGenerationAndReacceptsOwner()
    {
        StatusNotifierRegistry registry;
        const quint32 firstGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());
        registry.ownerLost(QStringLiteral(":1.10"));

        // A restart reuses the same unique-name string on some buses, but the
        // registry must treat it as a brand-new owner with a fresh generation.
        const quint32 secondGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QCOMPARE(secondGeneration, firstGeneration + 1);
        QVERIFY(registry.registerItem(primaryItemKey(secondGeneration), validDescriptor())
                    .accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.currentGeneration(QStringLiteral(":1.10")), secondGeneration);

        // The pre-restart event generation stays fenced forever.
        QCOMPARE(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor()).status,
                 RegistryStatus::StaleOwner);

        // ownerLost of a name that never appeared must be harmless.
        registry.ownerLost(QStringLiteral(":2.1"));
    }

    void removeAllForOwnerIsGenerationFenced()
    {
        StatusNotifierRegistry registry;
        const quint32 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.11")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             1);
        QVERIFY(registry.registerItem(otherOwner, descriptorWithIdentifier("other")).accepted());

        QCOMPARE(registry.removeAllForOwner(QStringLiteral(":1.10"), 99).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.removeAllForOwner(QStringLiteral("org.example.Tray"), generation).status,
                 RegistryStatus::InvalidOwner);
        QCOMPARE(registry.count(), qsizetype(2));

        QVERIFY(registry.removeAllForOwner(QStringLiteral(":1.10"), generation).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(registry.contains(otherOwner));

        // Removing the final owner frees every identity for future claims.
        QVERIFY(registry.removeAllForOwner(QStringLiteral(":1.11"), 1).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void rejectsCapacityOverflow()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);

        for (qsizetype index = 0; index < kMaxItems; ++index) {
            const OwnerKey key =
                ownerKey(QStringLiteral(":1.10"),
                         QStringLiteral("/org/qindaqt/Item%1").arg(index),
                         1);
            const RegistryOutcome outcome =
                registry.registerItem(key, descriptorWithIdentifier(QStringLiteral("id%1").arg(index)));
            QVERIFY2(outcome.accepted(), "registration below capacity must be accepted");
        }

        const OwnerKey overflow = ownerKey(QStringLiteral(":1.10"),
                                           QStringLiteral("/org/qindaqt/Overflow"),
                                           1);
        const RegistryOutcome outcome = registry.registerItem(overflow, validDescriptor());
        QCOMPARE(outcome.status, RegistryStatus::CapacityExceeded);
        QVERIFY(registry.isDegraded());
        QCOMPARE(registry.degradedReason(), QStringLiteral("item-capacity-exceeded"));
        QCOMPARE(registry.count(), kMaxItems);
        QVERIFY(!registry.contains(overflow));
    }

    void malformedReplacementDegradesAndKeepsLastKnownGood()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(), validDescriptor()).accepted());
        QVERIFY(!registry.isDegraded());

        ItemDescriptor malformed = validDescriptor();
        malformed.identity.clear();
        const RegistryOutcome outcome = registry.registerItem(primaryItemKey(), malformed);
        QCOMPARE(outcome.status, RegistryStatus::InvalidDescriptor);
        QVERIFY(registry.isDegraded());
        QCOMPARE(registry.degradedReason(), QStringLiteral("malformed-item-replacement"));

        // The tray keeps presenting the last-known-good descriptor.
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.find(primaryItemKey()).value().identity,
                 QStringLiteral("org.qindaqt.SampleTray"));

        // A malformed item from a brand-new key must not degrade the tray;
        // only losing presented data does.
        StatusNotifierRegistry freshRegistry;
        QVERIFY(freshRegistry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QCOMPARE(freshRegistry.registerItem(primaryItemKey(), malformed).status,
                 RegistryStatus::InvalidDescriptor);
        QVERIFY(!freshRegistry.isDegraded());
    }

    void evaluateRequestValidatesExactOwnership()
    {
        StatusNotifierRegistry registry;
        const quint32 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());

        for (const RequestKind kind :
             {RequestKind::Activate, RequestKind::ContextMenu, RequestKind::SecondaryActivate}) {
            QCOMPARE(registry.evaluateRequest(primaryItemKey(generation), kind).status,
                     RegistryStatus::Accepted);
        }

        const OwnerKey missingPath = ownerKey(QStringLiteral(":1.10"),
                                              QStringLiteral("/org/qindaqt/Missing"),
                                              generation);
        QCOMPARE(registry.evaluateRequest(missingPath, RequestKind::Activate).status,
                 RegistryStatus::UnknownItem);

        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation),
                                          static_cast<RequestKind>(42)).status,
                 RegistryStatus::InvalidRequest);

        registry.ownerLost(QStringLiteral(":1.10"));
        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation), RequestKind::Activate)
                     .status,
                 RegistryStatus::StaleOwner);

        const OwnerKey spoofedWellKnown =
            ownerKey(QStringLiteral("org.example.Tray"),
                     QStringLiteral("/org/example/StatusNotifierItem"),
                     generation);
        QCOMPARE(registry.evaluateRequest(spoofedWellKnown, RequestKind::Activate).status,
                 RegistryStatus::InvalidOwner);
    }

    void beginOwnerGenerationRejectsNonOwners()
    {
        StatusNotifierRegistry registry;
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral("org.example.Tray")), quint32(0));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral("not-a-bus-name")), quint32(0));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint32(1));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint32(2));
    }
};

QTEST_GUILESS_MAIN(StatusNotifierRegistryTests)
#include "tst_status_notifier_registry.moc"
