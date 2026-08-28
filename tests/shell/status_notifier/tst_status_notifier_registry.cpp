// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_event_sink.h>
#include <qindaqt/shell/status_notifier/status_notifier_limits.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>

#include "status_notifier_registry_test_support.h"
#include "status_notifier_atomic_reconciliation_test_support.h"

#include <QtTest>

#include <limits>
#include <type_traits>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;

class StatusNotifierRegistryTests final : public QObject
{
    Q_OBJECT

private slots:
    void typeTraitsEnforceNonCopyableAndNonMovable()
    {
        static_assert(!std::is_copy_constructible_v<StatusNotifierRegistry>);
        static_assert(!std::is_copy_assignable_v<StatusNotifierRegistry>);
        static_assert(!std::is_move_constructible_v<StatusNotifierRegistry>);
        static_assert(!std::is_move_assignable_v<StatusNotifierRegistry>);
        static_assert(!std::is_copy_constructible_v<StatusNotifierEventSink>);
        static_assert(!std::is_copy_assignable_v<StatusNotifierEventSink>);
        static_assert(!std::is_move_constructible_v<StatusNotifierEventSink>);
        static_assert(!std::is_move_assignable_v<StatusNotifierEventSink>);
        QVERIFY(true);
    }

    void keysItemsByExactOwner()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")), quint64(1));

        const RegistryOutcome outcome =
            registry.registerItem(epoch, primaryItemKey(), validDescriptor());
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
        QVERIFY(registry.registerItem(epoch, secondPath, descriptorWithIdentifier("second")).accepted());
        QCOMPARE(registry.count(), qsizetype(2));
    }

    void registryIsReachableOnlyThroughTheNarrowSink()
    {
        // The transport seam hands out StatusNotifierEventSink, not the
        // concrete registry: event authority only, no observation or request
        // evaluation through this pointer.
        StatusNotifierRegistry registry;
        StatusNotifierEventSink &sink = registry;
        const quint64 epoch = sink.beginWatcherEpoch();
        QCOMPARE(sink.beginOwnerGeneration(epoch, QStringLiteral(":1.10")), quint64(1));
        QVERIFY(sink.registerItem(epoch, primaryItemKey(), validDescriptor()).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(sink.removeItem(epoch, primaryItemKey()).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void replacesSameOwnerItemAndKeepsIdentityIndexConsistent()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(epoch, primaryItemKey(), validDescriptor()).accepted());

        // Replacement by the live owner is the supported update path.
        ItemDescriptor replacement = descriptorWithIdentifier(QStringLiteral("renamed.identity"));
        replacement.status = ItemStatus::NeedsAttention;
        QVERIFY(registry.registerItem(epoch, primaryItemKey(), replacement).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.find(primaryItemKey()).value().status, ItemStatus::NeedsAttention);

        // The old identity must be free; a second owner may now claim it.
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.11")) != 0);
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             2);
        QVERIFY(registry.registerItem(epoch, otherOwner, validDescriptor()).accepted());
        QCOMPARE(registry.count(), qsizetype(2));
    }

    void removesItems()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(epoch, primaryItemKey(), validDescriptor()).accepted());
        QVERIFY(registry.removeItem(epoch, primaryItemKey()).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QCOMPARE(registry.removeItem(epoch, primaryItemKey()).status, RegistryStatus::UnknownItem);

        // The freed identity can be claimed by another live owner.
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.11")) != 0);
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             2);
        QVERIFY(registry.registerItem(epoch, otherOwner, validDescriptor()).accepted());
    }

    void rejectsSpoofedOwnerNames()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")) != 0);

        // A well-known name masquerading as the owner is rejected outright.
        const OwnerKey spoofedWellKnown = ownerKey(QStringLiteral("org.example.Tray"),
                                                   QStringLiteral("/org/example/StatusNotifierItem"),
                                                   1);
        QCOMPARE(registry.registerItem(epoch, spoofedWellKnown, validDescriptor()).status,
                 RegistryStatus::InvalidOwner);
        QCOMPARE(registry.count(), qsizetype(0));

        // An event that never observed a generation for its owner is stale,
        // not registrable: keying is meaningless without the transport first
        // reporting the name's arrival.
        const OwnerKey neverSeen = ownerKey(QStringLiteral(":1.99"),
                                             QStringLiteral("/org/qindaqt/StatusNotifierItem"),
                                             3);
        QCOMPARE(registry.registerItem(epoch, neverSeen, validDescriptor()).status,
                 RegistryStatus::StaleOwner);
    }

    void rejectsDuplicateIdentityAcrossLiveOwners()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.11")) != 0);
        const OwnerKey first = primaryItemKey();
        const OwnerKey second = ownerKey(QStringLiteral(":1.11"),
                                          QStringLiteral("/org/example/StatusNotifierItem"),
                                          2);
        QVERIFY(registry.registerItem(epoch, first, validDescriptor()).accepted());

        const RegistryOutcome duplicate = registry.registerItem(epoch, second, validDescriptor());
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
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 firstGeneration = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(epoch, primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());

        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), firstGeneration).accepted());
        QCOMPARE(registry.count(), qsizetype(0));

        // A late in-flight reply from before the disconnect must not
        // resurrect the removed item.
        const RegistryOutcome stale = registry.registerItem(epoch, primaryItemKey(firstGeneration),
                                                            validDescriptor());
        QCOMPARE(stale.status, RegistryStatus::StaleOwner);
        QCOMPARE(stale.reasonCode, QStringLiteral("generation-not-current"));
        QCOMPARE(registry.count(), qsizetype(0));

        const RegistryOutcome staleRemoval =
            registry.removeItem(epoch, primaryItemKey(firstGeneration));
        QCOMPARE(staleRemoval.status, RegistryStatus::StaleOwner);
        const auto staleRequest = registry.evaluateRequest(primaryItemKey(firstGeneration),
                                                           RequestKind::Activate);
        QCOMPARE(staleRequest.outcome.status, RegistryStatus::StaleOwner);
        QVERIFY(!staleRequest.outcome.accepted());
    }

    void ownerLostWithWrongGenerationCannotRemoveLaterItems()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 generation = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(epoch, primaryItemKey(generation), validDescriptor()).accepted());

        // A delayed loss event carrying an old generation must not retire the
        // current owner: items stay visible and actionable.
        const RegistryOutcome staleLoss = registry.ownerLost(epoch, QStringLiteral(":1.10"),
                                                              generation + 1);
        QCOMPARE(staleLoss.status, RegistryStatus::StaleOwner);
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(registry.isOwnerLive(QStringLiteral(":1.10")));
        QCOMPARE(registry.evaluateRequest(primaryItemKey(generation), RequestKind::Activate)
                     .outcome.status,
                 RegistryStatus::Accepted);

        const RegistryOutcome neverSeenLoss = registry.ownerLost(epoch, QStringLiteral(":1.77"), 9);
        QCOMPARE(neverSeenLoss.status, RegistryStatus::StaleOwner);

        // The correctly stamped loss retires the owner exactly once.
        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), generation).accepted());
        QCOMPARE(registry.ownerLost(epoch, QStringLiteral(":1.10"), generation).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void restartAdvancesGenerationAndReacceptsOwner()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 firstGeneration = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(epoch, primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());
        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), firstGeneration).accepted());

        // A restart reuses the same unique-name string on some buses, but the
        // registry must treat it as a brand-new owner with a fresh generation.
        const quint64 secondGeneration = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(secondGeneration > firstGeneration);
        QVERIFY(registry.registerItem(epoch, primaryItemKey(secondGeneration), validDescriptor())
                    .accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QCOMPARE(registry.currentGeneration(QStringLiteral(":1.10")), secondGeneration);

        // The pre-restart event generation stays fenced forever: with a
        // globally monotonic seed it can never be reissued.
        QCOMPARE(registry.registerItem(epoch, primaryItemKey(firstGeneration), validDescriptor()).status,
                 RegistryStatus::StaleOwner);

        // ownerLost of a name that never appeared must be harmless.
        QCOMPARE(registry.ownerLost(epoch, QStringLiteral(":2.1"), 1).status,
                 RegistryStatus::StaleOwner);
    }

    void rebasingALiveOwnerDropsStaleItemsAndFreesIdentity()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 firstGeneration = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(epoch, primaryItemKey(firstGeneration), validDescriptor())
                    .accepted());

        // A duplicate begin for a still-live name (watcher rebaseline) must
        // not leave old-generation items presented but unactionable: the
        // rebase drops them and frees their identity claims.
        const quint64 rebasedGeneration = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(rebasedGeneration > firstGeneration);
        QCOMPARE(registry.count(), qsizetype(0));
        QVERIFY(!registry.contains(primaryItemKey(firstGeneration)));

        // The freed identity can be claimed immediately under the new key.
        QVERIFY(registry.registerItem(epoch, primaryItemKey(rebasedGeneration), validDescriptor())
                    .accepted());
        QCOMPARE(registry.evaluateRequest(primaryItemKey(rebasedGeneration),
                                          RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);

        // Old-generation traffic stays fenced after the rebase.
        QCOMPARE(registry.registerItem(epoch, primaryItemKey(firstGeneration), validDescriptor()).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.evaluateRequest(primaryItemKey(firstGeneration),
                                          RequestKind::Activate).outcome.status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.removeAllForOwner(epoch, QStringLiteral(":1.10"), firstGeneration).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.ownerLost(epoch, QStringLiteral(":1.10"), firstGeneration).status,
                 RegistryStatus::StaleOwner);
    }

    void generationsAreGloballyUniqueAcrossOwners()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 first = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        const quint64 second = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.11"));
        QVERIFY(second > first);
        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), first).accepted());

        // After the loss and another owner's allocation, the restarted name
        // receives a strictly larger generation than any previously issued
        // value, so a stale event can never collide with the new one.
        const quint64 restarted = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(restarted > second);
        QVERIFY(restarted > first);
    }

    void ownerHistoryIsBoundedAndFailsClosed()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        for (qsizetype index = 0; index < kMaxTrackedOwners; ++index) {
            const QString name = QStringLiteral(":1.%1").arg(index + 1);
            QVERIFY2(registry.beginOwnerGeneration(epoch, name) != 0,
                     "owners below the bound must be admitted");
        }

        // The bounded table refuses a new live owner beyond capacity instead
        // of evicting or growing without limit.
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral(":2.1")), quint64(0));

        // An existing live owner can still rebase: the rebase consumes no new
        // slot and keeps the registry usable.
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.1")) != 0);

        // Retiring an owner frees its slot for a fresh name.
        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.1"), registry.currentGeneration(
                                      QStringLiteral(":1.1"))).accepted());
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":2.1")) != 0);
    }

    void removeAllForOwnerIsGenerationFenced()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 generation = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.11")) != 0);
        QVERIFY(registry.registerItem(epoch, primaryItemKey(generation), validDescriptor()).accepted());
        const OwnerKey otherOwner = ownerKey(QStringLiteral(":1.11"),
                                             QStringLiteral("/org/example/StatusNotifierItem"),
                                             2);
        QVERIFY(registry.registerItem(epoch, otherOwner, descriptorWithIdentifier("other")).accepted());

        QCOMPARE(registry.removeAllForOwner(epoch, QStringLiteral(":1.10"), 99).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(registry.removeAllForOwner(epoch, QStringLiteral("org.example.Tray"), generation).status,
                 RegistryStatus::InvalidOwner);
        QCOMPARE(registry.count(), qsizetype(2));

        QVERIFY(registry.removeAllForOwner(epoch, QStringLiteral(":1.10"), generation).accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(registry.contains(otherOwner));

        // Removing the final owner frees every identity for future claims.
        QVERIFY(registry.removeAllForOwner(epoch, QStringLiteral(":1.11"), 2).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
    }

    void malformedReplacementIncludingMenuDegradesAndKeepsLastKnownGood()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")) != 0);
        QVERIFY(registry.registerItem(epoch, primaryItemKey(), validDescriptor()).accepted());
        QVERIFY(!registry.isDegraded());

        // A composed hostile menu in a replacement is caught at admission.
        const RegistryOutcome hostileMenu =
            registry.registerItem(epoch, primaryItemKey(), descriptorWithHostileMenu());
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

        // Hostile invalid parent menu replacement
        const RegistryOutcome badParent =
            registry.registerItem(epoch, primaryItemKey(), descriptorWithInvalidParentMenu());
        QCOMPARE(badParent.status, RegistryStatus::InvalidDescriptor);
        QVERIFY(registry.isDegraded());
        registry.acknowledgeDegraded();

        // Hostile over-nodes menu replacement
        const RegistryOutcome overNodes =
            registry.registerItem(epoch, primaryItemKey(), descriptorWithExceededNodesMenu());
        QCOMPARE(overNodes.status, RegistryStatus::InvalidDescriptor);
        QVERIFY(registry.isDegraded());
        registry.acknowledgeDegraded();

        // A malformed item from a brand-new key must not degrade the tray;
        // only losing presented data does.
        StatusNotifierRegistry freshRegistry;
        const quint64 freshEpoch = freshRegistry.beginWatcherEpoch();
        QVERIFY(freshRegistry.beginOwnerGeneration(freshEpoch, QStringLiteral(":1.10")) != 0);
        QCOMPARE(freshRegistry.registerItem(freshEpoch, primaryItemKey(),
                                            descriptorWithIdentifier(QString()))
                     .status,
                 RegistryStatus::InvalidDescriptor);
        QVERIFY(!freshRegistry.isDegraded());
    }

    void itemCapacityRefusesOverflowButAllowsReplacement()
    {
        verifyItemCapacityBoundary();
    }

    void watcherEpochReconciliationPrunesUnseenMembership()
    {
        verifyWatcherEpochReconciliation();
    }

    void replacementPopulationAtomicallyHandsOverIdentity()
    {
        verifyAtomicIdentityHandovers();
    }

    void replacementPopulationUsesPostPruneCapacity()
    {
        verifyCapacityBoundAtomicReplacement();
    }

    void evaluateRequestBindsTypedIntentToExactOwnership()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 generation = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(registry.registerItem(epoch, primaryItemKey(generation), validDescriptor()).accepted());

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

        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), generation).accepted());
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

    void revalidateIntentGuardsAgainstReplacementRemovalRebaseAndOwnerLoss()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        const quint64 generation = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        const OwnerKey target = primaryItemKey(generation);
        QVERIFY(registry.registerItem(epoch, target, validDescriptor()).accepted());

        const auto eval = registry.evaluateRequest(target, RequestKind::Activate);
        QVERIFY(eval.outcome.accepted());
        const RequestIntent originalIntent = eval.intent;

        // Intent is initially valid
        QVERIFY(registry.revalidateIntent(originalIntent).accepted());

        // 1. Same-key identity replacement: replace identity A with identity B on same live key
        ItemDescriptor replacement = validDescriptor();
        replacement.identity = QStringLiteral("org.qindaqt.ReplacedIdentity");
        QVERIFY(registry.registerItem(epoch, target, replacement).accepted());

        // Original intent (stamped with identity A) MUST fail revalidation!
        const RegistryOutcome replacedOutcome = registry.revalidateIntent(originalIntent);
        QVERIFY(!replacedOutcome.accepted());
        QCOMPARE(replacedOutcome.status, RegistryStatus::InvalidRequest);
        QCOMPARE(replacedOutcome.reasonCode, QStringLiteral("identity-mismatch"));

        // Re-evaluating gives intent for identity B, which revalidates
        const auto newEval = registry.evaluateRequest(target, RequestKind::Activate);
        QVERIFY(newEval.outcome.accepted());
        QVERIFY(registry.revalidateIntent(newEval.intent).accepted());

        // 2. Item removal: revalidation fails with UnknownItem
        QVERIFY(registry.removeItem(epoch, target).accepted());
        const RegistryOutcome removedOutcome = registry.revalidateIntent(newEval.intent);
        QCOMPARE(removedOutcome.status, RegistryStatus::UnknownItem);

        // Re-register item for rebase test
        QVERIFY(registry.registerItem(epoch, target, replacement).accepted());
        QVERIFY(registry.revalidateIntent(newEval.intent).accepted());

        // 3. Owner rebase: drops old generation, allocating new generation
        const quint64 rebasedGen = registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QVERIFY(rebasedGen > generation);
        const RegistryOutcome rebasedOutcome = registry.revalidateIntent(newEval.intent);
        QCOMPARE(rebasedOutcome.status, RegistryStatus::StaleOwner);

        // 4. Owner loss
        QVERIFY(registry.ownerLost(epoch, QStringLiteral(":1.10"), rebasedGen).accepted());
        const RegistryOutcome lostOutcome = registry.revalidateIntent(newEval.intent);
        QCOMPARE(lostOutcome.status, RegistryStatus::StaleOwner);

        // 5. Malformed / invalid request kind in intent
        RequestIntent invalidIntent = newEval.intent;
        invalidIntent.kind = static_cast<RequestKind>(99);
        QCOMPARE(registry.revalidateIntent(invalidIntent).status, RegistryStatus::InvalidRequest);
    }

    void beginOwnerGenerationRejectsNonOwners()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral("org.example.Tray")), quint64(0));
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral("not-a-bus-name")), quint64(0));
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")), quint64(1));
        QCOMPARE(registry.beginOwnerGeneration(epoch, QStringLiteral(":1.10")), quint64(2));
    }

    void generationAndEpochCounterExhaustionFailsClosed()
    {
        constexpr quint64 kMax = std::numeric_limits<quint64>::max();

        // 1. Generation seed exhaustion
        StatusNotifierRegistry nearExhaustionRegistry(kMax - 1, 0);
        const quint64 epoch = nearExhaustionRegistry.beginWatcherEpoch();
        QCOMPARE(epoch, quint64(1));

        // Allocating next generation hits kMax
        const quint64 lastGen =
            nearExhaustionRegistry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QCOMPARE(lastGen, kMax);
        const OwnerKey lastKey = primaryItemKey(lastGen);
        QVERIFY(nearExhaustionRegistry.registerItem(epoch, lastKey, validDescriptor()).accepted());

        // Further generation allocation is refused without wrapping or
        // dropping the current owner's last-known-good item.
        const quint64 overflowGen =
            nearExhaustionRegistry.beginOwnerGeneration(epoch, QStringLiteral(":1.10"));
        QCOMPARE(overflowGen, quint64(0));
        QCOMPARE(nearExhaustionRegistry.currentGeneration(QStringLiteral(":1.10")), lastGen);
        QVERIFY(nearExhaustionRegistry.contains(lastKey));

        // 2. The final watcher epoch is usable once. A subsequent replacement
        // fails closed by invalidating the active epoch and returning Loading;
        // traffic from the old watcher cannot mutate retained membership.
        StatusNotifierRegistry epochExhaustedRegistry(0, kMax - 1);
        const quint64 lastEpoch = epochExhaustedRegistry.beginWatcherEpoch();
        QCOMPARE(lastEpoch, kMax);
        const quint64 generation =
            epochExhaustedRegistry.beginOwnerGeneration(lastEpoch, QStringLiteral(":1.10"));
        const OwnerKey epochItem = primaryItemKey(generation);
        QVERIFY(epochExhaustedRegistry
                    .registerItem(lastEpoch, epochItem, validDescriptor()).accepted());
        QVERIFY(epochExhaustedRegistry.markInitialPopulationComplete(lastEpoch).accepted());

        QCOMPARE(epochExhaustedRegistry.beginWatcherEpoch(), quint64(0));
        QCOMPARE(epochExhaustedRegistry.currentWatcherEpoch(), quint64(0));
        QVERIFY(!epochExhaustedRegistry.initialPopulationComplete());
        QCOMPARE(epochExhaustedRegistry.registerItem(lastEpoch, epochItem, validDescriptor()).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(epochExhaustedRegistry.removeItem(lastEpoch, epochItem).status,
                 RegistryStatus::StaleOwner);
        QCOMPARE(epochExhaustedRegistry.markInitialPopulationComplete(lastEpoch).status,
                 RegistryStatus::StaleOwner);
        QVERIFY(epochExhaustedRegistry.contains(epochItem));
    }
};

QTEST_GUILESS_MAIN(StatusNotifierRegistryTests)
#include "tst_status_notifier_registry.moc"
