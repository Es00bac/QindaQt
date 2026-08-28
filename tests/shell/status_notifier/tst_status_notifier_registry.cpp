// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_event_sink.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include <QtTest>

using namespace QindaQt::StatusNotifier;

namespace
{

OwnerKey ownerKey(const QString &uniqueName, const QString &path, quint64 generation)
{
    OwnerKey key;
    key.uniqueName = uniqueName;
    key.objectPath = path;
    key.generation = generation;
    return key;
}

OwnerKey primaryItemKey(quint64 generation = 1)
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

ItemDescriptor descriptorWithHostileMenu()
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

} // namespace

class StatusNotifierRegistryTests final : public QObject
{
    Q_OBJECT

private slots:
    void keysItemsByExactOwner()
    {
        StatusNotifierRegistry registry;
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint64(1));

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

    void registryIsReachableOnlyThroughTheNarrowSink()
    {
        // The transport seam hands out StatusNotifierEventSink, not the
        // concrete registry: event authority only, no observation or request
        // evaluation through this pointer.
        StatusNotifierRegistry registry;
        StatusNotifierEventSink &sink = registry;
        QCOMPARE(sink.beginOwnerGeneration(QStringLiteral(":1.10")), quint64(1));
        QVERIFY(sink.registerItem(primaryItemKey(), validDescriptor()).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(sink.removeItem(primaryItemKey()).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
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
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             2);
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
                                             2);
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
                                         2);
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
        const quint64 firstGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());

        QVERIFY(registry.ownerLost(QStringLiteral(":1.10"), firstGeneration).accepted());
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
        const auto staleRequest = registry.evaluateRequest(primaryItemKey(firstGeneration),
                                                           RequestKind::Activate);
        QCOMPARE(staleRequest.outcome.status, RegistryStatus::StaleOwner);
        QVERIFY(!staleRequest.outcome.accepted());
    }

    void ownerLostWithWrongGenerationCannotRemoveLaterItems()
    {
        StatusNotifierRegistry registry;
        const quint64 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());

        // A delayed loss event carrying an old generation must not retire the
        // current owner: items stay visible and actionable.
        const RegistryOutcome staleLoss = registry.ownerLost(QStringLiteral(":1.10"),
                                                             generation + 1);
        QCOMPARE(staleLoss.status, RegistryStatus::StaleOwner);
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(registry.isOwnerLive(QStringLiteral(":1.10")));
        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation), RequestKind::Activate)
                     .outcome.status,
                 RegistryStatus::Accepted);

        const RegistryOutcome neverSeenLoss = registry.ownerLost(QStringLiteral(":1.77"), 9);
        QCOMPARE(neverSeenLoss.status, RegistryStatus::StaleOwner);

        // The correctly stamped loss retires the owner exactly once.
        QVERIFY(registry.ownerLost(QStringLiteral(":1.10"), generation).accepted());
        QCOMPARE(registry.ownerLost(QStringLiteral(":1.10"), generation).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void restartAdvancesGenerationAndReacceptsOwner()
    {
        StatusNotifierRegistry registry;
        const quint64 firstGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());
        QVERIFY(registry.ownerLost(QStringLiteral(":1.10"), firstGeneration).accepted());

        // A restart reuses the same unique-name string on some buses, but the
        // registry must treat it as a brand-new owner with a fresh generation.
        const quint64 secondGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(secondGeneration > firstGeneration);
        QVERIFY(registry.registerItem(primaryItemKey(secondGeneration), validDescriptor())
                    .accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.currentGeneration(QStringLiteral(":1.10")), secondGeneration);

        // The pre-restart event generation stays fenced forever: with a
        // globally monotonic seed it can never be reissued.
        QCOMPARE(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor()).status,
                 RegistryStatus::StaleOwner);

        // ownerLost of a name that never appeared must be harmless.
        QCOMPARE(registry.ownerLost(QStringLiteral(":2.1"), 1).status,
                 RegistryStatus::StaleOwner);
    }

    void rebasingALiveOwnerDropsStaleItemsAndFreesIdentity()
    {
        StatusNotifierRegistry registry;
        const quint64 firstGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());

        // A duplicate begin for a still-live name (watcher rebaseline) must
        // not leave old-generation items presented but unactionable: the
        // rebase drops them and frees their identity claims.
        const quint64 rebasedGeneration = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(rebasedGeneration > firstGeneration);
        QCOMPARE(registry.count(), qsizetype(0));
        QVERIFY(!registry.contains(primaryItemKey(firstGeneration)));

        // The freed identity can be claimed immediately under the new key.
        QVERIFY(registry.registerItem(primaryItemKey(rebasedGeneration), validDescriptor())
                    .accepted());
        QCOMPARE(registry.evaluateRequest(primaryItemKey(rebasedGeneration),
                                          RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);

        // Old-generation traffic stays fenced after the rebase.
        QCOMPARE(registry.registerItem(primaryItemKey(firstGeneration), validDescriptor()).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.evaluateRequest(primaryItemKey(firstGeneration),
                                          RequestKind::Activate).outcome.status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.removeAllForOwner(QStringLiteral(":1.10"), firstGeneration).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.ownerLost(QStringLiteral(":1.10"), firstGeneration).status,
                 RegistryStatus::StaleOwner);
    }

    void generationsAreGloballyUniqueAcrossOwners()
    {
        StatusNotifierRegistry registry;
        const quint64 first = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        const quint64 second = registry.beginOwnerGeneration(QStringLiteral(":1.11"));
        QVERIFY(second > first);
        QVERIFY(registry.ownerLost(QStringLiteral(":1.10"), first).accepted());

        // After the loss and another owner's allocation, the restarted name
        // receives a strictly larger generation than any previously issued
        // value, so a stale event can never collide with the new one.
        const quint64 restarted = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(restarted > second);
        QVERIFY(restarted > first);
    }

    void ownerHistoryIsBoundedAndFailsClosed()
    {
        StatusNotifierRegistry registry;
        for (qsizetype index = 0; index < kMaxTrackedOwners; ++index) {
            const QString name = QStringLiteral(":1.%1").arg(index + 1);
            QVERIFY2(registry.beginOwnerGeneration(name) != 0,
                     "owners below the bound must be admitted");
        }

        // The bounded table refuses a new live owner beyond capacity instead
        // of evicting or growing without limit.
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":2.1")), quint64(0));

        // An existing live owner can still rebase: the rebase consumes no new
        // slot and keeps the registry usable.
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.1")) != 0);

        // Retiring an owner frees its slot for a fresh name.
        QVERIFY(registry.ownerLost(QStringLiteral(":1.1"), registry.currentGeneration(
                                      QStringLiteral(":1.1"))).accepted());
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":2.1")) != 0);
    }

    void removeAllForOwnerIsGenerationFenced()
    {
        StatusNotifierRegistry registry;
        const quint64 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.11")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             2);
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
        QVERIFY(registry.removeAllForOwner(QStringLiteral(":1.11"), 2).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void malformedReplacementIncludingMenuDegradesAndKeepsLastKnownGood()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(primaryItemKey(), validDescriptor()).accepted());
        QVERIFY(!registry.isDegraded());

        // A composed hostile menu in a replacement is caught at admission.
        const RegistryOutcome hostileMenu =
            registry.registerItem(primaryItemKey(), descriptorWithHostileMenu());
        QCOMPARE(hostileMenu.status, RegistryStatus::InvalidDescriptor);
        QVERIFY(registry.isDegraded());
        QCOMPARE(registry.degradedReason(), QStringLiteral("malformed-item-replacement"));
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.find(primaryItemKey()).value().identity,
                 QStringLiteral("org.qindaqt.SampleTray"));
        QCOMPARE(registry.evaluateRequest(primaryItemKey(), RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);

        registry.acknowledgeDegraded();
        QVERIFY(!registry.isDegraded());

        // A malformed item from a brand-new key must not degrade the tray;
        // only losing presented data does.
        StatusNotifierRegistry freshRegistry;
        QVERIFY(freshRegistry.beginOwnerGeneration(QStringLiteral(":1.10")) != 0);
        QCOMPARE(freshRegistry.registerItem(primaryItemKey(),
                                            descriptorWithIdentifier(QString()))
                     .status,
                 RegistryStatus::InvalidDescriptor);
        QVERIFY(!freshRegistry.isDegraded());
    }

    void watcherEpochRebaselineReturnsPresentationToLoading()
    {
        StatusNotifierRegistry registry;
        const quint64 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());
        registry.markInitialPopulationComplete();
        QVERIFY(registry.initialPopulationComplete());

        // A watcher reconnect restarts the epoch: the population bit resets
        // so the tray falls back to fail-closed Loading until the replacement
        // watcher's population is observed.
        registry.beginWatcherEpoch();
        QVERIFY(!registry.initialPopulationComplete());

        registry.markInitialPopulationComplete();
        QVERIFY(registry.initialPopulationComplete());
        QCOMPARE(registry.count(), qsizetype(1));
    }

    void evaluateRequestBindsTypedIntentToExactOwnership()
    {
        StatusNotifierRegistry registry;
        const quint64 generation = registry.beginOwnerGeneration(QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(primaryItemKey(generation), validDescriptor()).accepted());

        for (const RequestKind kind :
             {RequestKind::Activate, RequestKind::ContextMenu, RequestKind::SecondaryActivate}) {
            const auto evaluation =
                registry.evaluateRequest(primaryItemKey(generation), kind);
            QCOMPARE(evaluation.outcome.status, RegistryStatus::Accepted);
            // The accepted intent carries the exact owner key with its live
            // generation plus the identity snapshot at acceptance time.
            QCOMPARE(evaluation.intent.target, primaryItemKey(generation));
            QCOMPARE(evaluation.intent.identity, QStringLiteral("org.qindaqt.SampleTray"));
            QCOMPARE(evaluation.intent.kind, kind);
        }

        const auto missing = registry.evaluateRequest(
            ownerKey(QStringLiteral(":1.10"), QStringLiteral("/org/qindaqt/Missing"), generation),
            RequestKind::Activate);
        QCOMPARE(missing.outcome.status, RegistryStatus::UnknownItem);
        QCOMPARE(missing.intent.target.generation, quint64(0));

        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation),
                                          static_cast<RequestKind>(42)).outcome.status,
                 RegistryStatus::InvalidRequest);

        QVERIFY(registry.ownerLost(QStringLiteral(":1.10"), generation).accepted());
        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation), RequestKind::Activate)
                     .outcome.status,
                 RegistryStatus::StaleOwner);

        const OwnerKey spoofedWellKnown =
            ownerKey(QStringLiteral("org.example.Tray"),
                     QStringLiteral("/org/example/StatusNotifierItem"),
                     generation);
        QCOMPARE(registry.evaluateRequest(spoofedWellKnown, RequestKind::Activate).outcome.status,
                 RegistryStatus::InvalidOwner);
    }

    void beginOwnerGenerationRejectsNonOwners()
    {
        StatusNotifierRegistry registry;
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral("org.example.Tray")), quint64(0));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral("not-a-bus-name")), quint64(0));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint64(1));
        QCOMPARE(registry.beginOwnerGeneration(QStringLiteral(":1.10")), quint64(2));
    }
};

QTEST_GUILESS_MAIN(StatusNotifierRegistryTests)
#include "tst_status_notifier_registry.moc"
