// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>
#include <qindaqt/shell/status_notifier/status_notifier_registry.h>
#include <qindaqt/shell/status_notifier/status_notifier_transport.h>

#include <QtTest>

using namespace QindaQt::StatusNotifier;

namespace
{

const QString kPrimaryOwner = QStringLiteral(":1.10");
const QString kSecondaryOwner = QStringLiteral(":1.11");
const QString kPrimaryPath = QStringLiteral("/org/qindaqt/StatusNotifierItem");
const QString kSecondaryPath = QStringLiteral("/org/example/StatusNotifierItem");

OwnerKey key(const QString &owner, const QString &path, quint64 generation)
{
    OwnerKey value;
    value.uniqueName = owner;
    value.objectPath = path;
    value.generation = generation;
    return value;
}

ItemDescriptor descriptorWith(const QString &identity, const QString &title, ItemStatus status)
{
    ItemDescriptor descriptor;
    descriptor.category = ItemCategory::Application;
    descriptor.identity = identity;
    descriptor.title = title;
    descriptor.status = status;
    descriptor.toolTip.description = QStringLiteral("sample tooltip body");
    return descriptor;
}

// AGENT-NOTE: The only allowed transport implementations are test fakes and a
// future exact-owner QtDBus adapter in its own module. This fake scripts
// events through the narrow StatusNotifierEventSink interface — it cannot
// reach observation, request evaluation, or degradation acknowledgement — and
// never touches a bus.
class FakeStatusNotifierTransport final : public StatusNotifierTransport
{
public:
    explicit FakeStatusNotifierTransport(int *detachCount = nullptr)
        : m_detachCount(detachCount)
    {
    }

    ~FakeStatusNotifierTransport() override
    {
        detach();
    }

    void attach(StatusNotifierEventSink *sink) override
    {
        // The interface contract refuses null and re-attachment; the fake
        // mirrors both rules so the lifetime rules stay exercised.
        if (sink == nullptr || m_sink != nullptr) {
            return;
        }
        m_sink = sink;
    }

    void detach() override
    {
        if (m_sink != nullptr && m_detachCount != nullptr) {
            ++(*m_detachCount);
        }
        m_sink = nullptr;
        m_currentEpoch = 0;
        m_allocatedGenerations.clear();
    }

    [[nodiscard]] bool isAttached() const override { return m_sink != nullptr; }

    [[nodiscard]] quint64 emitWatcherEpoch()
    {
        if (!m_sink) {
            return 0;
        }
        m_currentEpoch = m_sink->beginWatcherEpoch();
        return m_currentEpoch;
    }

    [[nodiscard]] RegistryOutcome emitInitialPopulationComplete(quint64 epoch = 0)
    {
        if (!m_sink) {
            return RegistryOutcome{RegistryStatus::UnknownItem, {}};
        }
        const quint64 targetEpoch = (epoch != 0) ? epoch : m_currentEpoch;
        return m_sink->markInitialPopulationComplete(targetEpoch);
    }

    quint64 emitOwnerAppeared(const QString &owner, quint64 epoch = 0)
    {
        if (!m_sink) {
            return 0;
        }
        const quint64 targetEpoch = (epoch != 0) ? epoch : m_currentEpoch;
        const quint64 gen = m_sink->beginOwnerGeneration(targetEpoch, owner);
        if (gen != 0) {
            m_allocatedGenerations.insert(owner, gen);
        }
        return gen;
    }

    [[nodiscard]] RegistryOutcome emitOwnerLost(const QString &owner, quint64 epoch = 0, quint64 gen = 0)
    {
        if (!m_sink) {
            return RegistryOutcome{RegistryStatus::UnknownItem, {}};
        }
        const quint64 targetEpoch = (epoch != 0) ? epoch : m_currentEpoch;
        const quint64 targetGen = (gen != 0) ? gen : m_allocatedGenerations.value(owner);
        const RegistryOutcome outcome = m_sink->ownerLost(targetEpoch, owner, targetGen);
        if (outcome.accepted()) {
            m_allocatedGenerations.remove(owner);
        }
        return outcome;
    }

    [[nodiscard]] RegistryOutcome emitItemRegistered(const QString &owner, const QString &path,
                                                     const ItemDescriptor &descriptor,
                                                     quint64 epoch = 0, quint64 gen = 0)
    {
        if (!m_sink) {
            return RegistryOutcome{RegistryStatus::UnknownItem, {}};
        }
        const quint64 targetEpoch = (epoch != 0) ? epoch : m_currentEpoch;
        const quint64 targetGen = (gen != 0) ? gen : m_allocatedGenerations.value(owner);
        return m_sink->registerItem(targetEpoch, key(owner, path, targetGen), descriptor);
    }

    [[nodiscard]] RegistryOutcome emitItemRemoved(const QString &owner, const QString &path,
                                                  quint64 epoch = 0, quint64 gen = 0)
    {
        if (!m_sink) {
            return RegistryOutcome{RegistryStatus::UnknownItem, {}};
        }
        const quint64 targetEpoch = (epoch != 0) ? epoch : m_currentEpoch;
        const quint64 targetGen = (gen != 0) ? gen : m_allocatedGenerations.value(owner);
        return m_sink->removeItem(targetEpoch, key(owner, path, targetGen));
    }

    [[nodiscard]] quint64 currentEpoch() const { return m_currentEpoch; }
    [[nodiscard]] quint64 generation(const QString &owner) const
    {
        return m_allocatedGenerations.value(owner);
    }
    [[nodiscard]] StatusNotifierEventSink *sink() const { return m_sink; }

private:
    StatusNotifierEventSink *m_sink = nullptr;
    quint64 m_currentEpoch = 0;
    QHash<QString, quint64> m_allocatedGenerations;
    int *m_detachCount = nullptr;
};

} // namespace

class StatusNotifierPresentationTests final : public QObject
{
    Q_OBJECT

private slots:
    void showsLoadingBeforeInitialPopulation()
    {
        StatusNotifierRegistry registry;
        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Loading);
        QVERIFY(presentation.items.isEmpty());
        QVERIFY(presentation.diagnostic.isEmpty());
    }

    void showsEmptyAfterPopulationWithNoItems()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.markInitialPopulationComplete(epoch).accepted());
        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Empty);
        QCOMPARE(presentation.diagnostic, QStringLiteral("no-status-notifier-items"));
        QVERIFY(presentation.items.isEmpty());
    }

    void watcherLossDegradesButRetainsLastKnownGoodItems()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, kPrimaryOwner) != 0);
        QVERIFY(registry
                    .registerItem(epoch,
                                  key(kPrimaryOwner, kPrimaryPath, 1),
                                  descriptorWith(QStringLiteral("id"),
                                                 QStringLiteral("Title"),
                                                 ItemStatus::Active))
                    .accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch).accepted());

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = false});
        QCOMPARE(presentation.state, PresentationState::Degraded);
        QCOMPARE(presentation.diagnostic,
                 QStringLiteral("status-notifier-watcher-unavailable"));
        // AGENT-GUARD: The accepted contract keeps last-known-good items
        // visible and actionable across watcher loss; blanking them here
        // would contradict ADR-0032 and leave users with an empty tray.
        QCOMPARE(presentation.items.size(), 1);
        QCOMPARE(presentation.items.at(0).accessibleName, QStringLiteral("Title"));
        // Items remain registered, so request intents stay acceptable.
        QCOMPARE(registry.evaluateRequest(key(kPrimaryOwner, kPrimaryPath, 1),
                                          RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);
    }

    void projectsReadyItemsWithStableOrderAndIdentities()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, kSecondaryOwner) != 0);
        QVERIFY(registry.beginOwnerGeneration(epoch, kPrimaryOwner) != 0);
        QVERIFY(registry
                    .registerItem(epoch,
                                  key(kSecondaryOwner, kSecondaryPath, 1),
                                  descriptorWith(QStringLiteral("zeta.identity"),
                                                 QStringLiteral("Zeta app"),
                                                 ItemStatus::NeedsAttention))
                    .accepted());
        QVERIFY(registry
                    .registerItem(epoch,
                                  key(kPrimaryOwner, kPrimaryPath, 2),
                                  descriptorWith(QStringLiteral("alpha.identity"),
                                                 QString(),
                                                 ItemStatus::Active))
                    .accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch).accepted());

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Ready);
        QCOMPARE(presentation.items.size(), 2);

        // Order is owner-name based, not registration order, so QML never
        // observes a phantom reorder between projections.
        QCOMPARE(presentation.items.at(0).identity, QStringLiteral("alpha.identity"));
        QCOMPARE(presentation.items.at(0).owner.uniqueName, kPrimaryOwner);
        QCOMPARE(presentation.items.at(1).identity, QStringLiteral("zeta.identity"));

        // A missing title falls back to the identity for the accessible name.
        QCOMPARE(presentation.items.at(0).accessibleName, QStringLiteral("alpha.identity"));
        QCOMPARE(presentation.items.at(0).accessibleDescription,
                 QStringLiteral("sample tooltip body"));
        QCOMPARE(presentation.items.at(0).accessibleStatusText, QStringLiteral("active"));
        QCOMPARE(presentation.items.at(1).accessibleName, QStringLiteral("Zeta app"));
        QCOMPARE(presentation.items.at(1).accessibleStatusText,
                 QStringLiteral("needs attention"));

        const QList<KeyboardAction> &actions = presentation.items.at(0).keyboardActions;
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions.at(0).kind, RequestKind::Activate);
        QCOMPARE(actions.at(0).keyboardDescription, QStringLiteral("Enter or Space"));
        QCOMPARE(actions.at(1).kind, RequestKind::ContextMenu);
        QCOMPARE(actions.at(1).keyboardDescription,
                 QStringLiteral("Shift+F10 or Menu key"));
        QCOMPARE(actions.at(2).kind, RequestKind::SecondaryActivate);
        QVERIFY(actions.at(2).keyboardDescription.isEmpty());

        const TrayPresentation repeat = projectPresentation(registry, {.transportLive = true});
        QCOMPARE(repeat, presentation);
    }

    void injectedTextsFormTheLocalizationBoundary()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, kPrimaryOwner) != 0);
        QVERIFY(registry
                    .registerItem(epoch,
                                  key(kPrimaryOwner, kPrimaryPath, 1),
                                  descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                 ItemStatus::NeedsAttention))
                    .accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch).accepted());

        PresentationTexts localized;
        localized.statusNeedsAttention = QStringLiteral("benötigt Aufmerksamkeit");
        localized.keyboardActivate = QStringLiteral("Eingabetaste oder Leertaste");
        localized.keyboardContextMenu = QStringLiteral("Umschalt+F10");

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true}, localized);
        QCOMPARE(presentation.items.at(0).accessibleStatusText,
                 QStringLiteral("benötigt Aufmerksamkeit"));
        QCOMPARE(presentation.items.at(0).keyboardActions.at(0).keyboardDescription,
                 QStringLiteral("Eingabetaste oder Leertaste"));
        QCOMPARE(presentation.items.at(0).keyboardActions.at(1).keyboardDescription,
                 QStringLiteral("Umschalt+F10"));

        // Defaults stay deterministic and independent of injected instances.
        const TrayPresentation fallback =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(fallback.items.at(0).accessibleStatusText, QStringLiteral("needs attention"));
    }

    void degradedRegistryKeepsLastKnownGoodItems()
    {
        StatusNotifierRegistry registry;
        const quint64 epoch = registry.beginWatcherEpoch();
        QVERIFY(registry.beginOwnerGeneration(epoch, kPrimaryOwner) != 0);
        const OwnerKey owner = key(kPrimaryOwner, kPrimaryPath, 1);
        QVERIFY(registry
                    .registerItem(epoch,
                                  owner,
                                  descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                 ItemStatus::Active))
                    .accepted());
        QVERIFY(registry.markInitialPopulationComplete(epoch).accepted());

        ItemDescriptor malformed = descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                   ItemStatus::Active);
        malformed.identity = QStringLiteral("   ");
        QVERIFY(!registry.registerItem(epoch, owner, malformed).accepted());

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Degraded);
        QCOMPARE(presentation.diagnostic, QStringLiteral("malformed-item-replacement"));
        QCOMPARE(presentation.items.size(), 1);
        QCOMPARE(presentation.items.at(0).accessibleName, QStringLiteral("Title"));
    }

    void fakeTransportDrivesLifecycleIncludingRebaselineAndReconciliation()
    {
        StatusNotifierRegistry registry;
        FakeStatusNotifierTransport transport;

        // Null-first attach test
        transport.attach(nullptr);
        QVERIFY(!transport.isAttached());

        transport.attach(&registry);
        QVERIFY(transport.isAttached());
        QCOMPARE(transport.sink(), &registry);

        // Second attach while attached is refused
        StatusNotifierRegistry secondRegistry;
        transport.attach(&secondRegistry);
        QVERIFY(transport.isAttached());
        QCOMPARE(transport.sink(), &registry);

        // Epoch 1 start
        const quint64 epoch1 = transport.emitWatcherEpoch();
        QCOMPARE(epoch1, quint64(1));

        transport.emitOwnerAppeared(kPrimaryOwner);
        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity"),
                                                       QStringLiteral("Title"),
                                                       ItemStatus::Active))
                    .accepted());
        QVERIFY(transport.emitInitialPopulationComplete().accepted());

        const OwnerKey live = key(kPrimaryOwner,
                                  kPrimaryPath,
                                  transport.generation(kPrimaryOwner));
        for (const RequestKind kind :
             {RequestKind::Activate, RequestKind::ContextMenu, RequestKind::SecondaryActivate}) {
            QCOMPARE(registry.evaluateRequest(live, kind).outcome.status,
                     RegistryStatus::Accepted);
            // A typed accepted intent binds owner, generation, and identity.
            QCOMPARE(registry.evaluateRequest(live, kind).intent.target, live);
            QCOMPARE(registry.evaluateRequest(live, kind).intent.identity,
                     QStringLiteral("identity"));
        }
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Ready);

        // Watcher loss: Degraded, items retained and actionable.
        QCOMPARE(projectPresentation(registry, {.transportLive = false}).state,
                 PresentationState::Degraded);

        // Watcher reconnect opens a new epoch: fail-closed Loading until the
        // replacement population is observed.
        const quint64 epoch2 = transport.emitWatcherEpoch();
        QCOMPARE(epoch2, quint64(2));
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Loading);

        // Re-register item in epoch 2 and complete population
        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity"),
                                                       QStringLiteral("Title"),
                                                       ItemStatus::Active))
                    .accepted());
        QVERIFY(transport.emitInitialPopulationComplete().accepted());
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Ready);
        // The item remains actionable under the same generation.
        QCOMPARE(registry.evaluateRequest(live, RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);

        // Watcher reconnect with empty replacement reconciles to Empty
        const quint64 epoch3 = transport.emitWatcherEpoch();
        QCOMPARE(epoch3, quint64(3));
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Loading);
        QVERIFY(transport.emitInitialPopulationComplete().accepted());
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Empty);
        QCOMPARE(registry.count(), qsizetype(0));

        // Start epoch 4, register item, then remove it
        const quint64 epoch4 = transport.emitWatcherEpoch();
        QCOMPARE(epoch4, quint64(4));
        transport.emitOwnerAppeared(kPrimaryOwner);
        const quint64 gen4 = transport.generation(kPrimaryOwner);
        const OwnerKey key4 = key(kPrimaryOwner, kPrimaryPath, gen4);
        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity4"),
                                                       QStringLiteral("Title4"),
                                                       ItemStatus::Active))
                    .accepted());
        QVERIFY(transport.emitInitialPopulationComplete().accepted());
        QCOMPARE(registry.count(), qsizetype(1));

        QVERIFY(transport.emitItemRemoved(kPrimaryOwner, kPrimaryPath).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QCOMPARE(registry.evaluateRequest(key4, RequestKind::Activate).outcome.status,
                 RegistryStatus::UnknownItem);
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Empty);

        // Source restart under same unique name: rebase allocation
        transport.emitOwnerAppeared(kPrimaryOwner);
        QVERIFY(transport.generation(kPrimaryOwner) > gen4);
        QCOMPARE(registry.evaluateRequest(key4, RequestKind::ContextMenu).outcome.status,
                 RegistryStatus::StaleOwner);

        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity5"),
                                                       QStringLiteral("Title5"),
                                                       ItemStatus::Passive))
                    .accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        QVERIFY(transport.emitOwnerLost(kPrimaryOwner).accepted());
        QCOMPARE(registry.count(), qsizetype(0));

        // Detach and re-attach to a different sink
        transport.detach();
        QVERIFY(!transport.isAttached());
        QCOMPARE(transport.currentEpoch(), quint64(0));
        QCOMPARE(transport.generation(kPrimaryOwner), quint64(0));
        transport.attach(&secondRegistry);
        QVERIFY(transport.isAttached());
        QCOMPARE(transport.sink(), &secondRegistry);
        transport.detach();
        QVERIFY(!transport.isAttached());

        // Destructor detach is observed after the fake itself is gone, so the
        // assertion proves the destructor performed the required transition.
        int destructorDetachCount = 0;
        {
            FakeStatusNotifierTransport scopedTransport(&destructorDetachCount);
            scopedTransport.attach(&registry);
            QVERIFY(scopedTransport.isAttached());
        }
        QCOMPARE(destructorDetachCount, 1);
    }
};

QTEST_GUILESS_MAIN(StatusNotifierPresentationTests)
#include "tst_status_notifier_presentation.moc"
