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
        m_sink = nullptr;
    }

    [[nodiscard]] bool isAttached() const override { return m_sink != nullptr; }

    void emitOwnerAppeared(const QString &owner)
    {
        m_allocatedGenerations.insert(owner, m_sink->beginOwnerGeneration(owner));
    }

    void emitOwnerLost(const QString &owner)
    {
        const RegistryOutcome outcome =
            m_sink->ownerLost(owner, m_allocatedGenerations.value(owner));
        if (outcome.accepted()) {
            m_allocatedGenerations.remove(owner);
        }
    }

    [[nodiscard]] RegistryOutcome emitItemRegistered(const QString &owner, const QString &path,
                                                     const ItemDescriptor &descriptor)
    {
        return m_sink->registerItem(key(owner, path, m_allocatedGenerations.value(owner)),
                                    descriptor);
    }

    [[nodiscard]] RegistryOutcome emitItemRemoved(const QString &owner, const QString &path)
    {
        return m_sink->removeItem(key(owner, path, m_allocatedGenerations.value(owner)));
    }

    void emitInitialPopulationComplete()
    {
        m_sink->markInitialPopulationComplete();
    }

    void emitWatcherEpoch()
    {
        m_sink->beginWatcherEpoch();
    }

    [[nodiscard]] quint64 generation(const QString &owner) const
    {
        return m_allocatedGenerations.value(owner);
    }

private:
    StatusNotifierEventSink *m_sink = nullptr;
    QHash<QString, quint64> m_allocatedGenerations;
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
        registry.markInitialPopulationComplete();
        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Empty);
        QCOMPARE(presentation.diagnostic, QStringLiteral("no-status-notifier-items"));
        QVERIFY(presentation.items.isEmpty());
    }

    void watcherLossDegradesButRetainsLastKnownGoodItems()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(kPrimaryOwner) != 0);
        registry.markInitialPopulationComplete();
        QVERIFY(registry
                    .registerItem(key(kPrimaryOwner, kPrimaryPath, 1),
                                  descriptorWith(QStringLiteral("id"),
                                                 QStringLiteral("Title"),
                                                 ItemStatus::Active))
                    .accepted());
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
        QVERIFY(registry.beginOwnerGeneration(kSecondaryOwner) != 0);
        QVERIFY(registry.beginOwnerGeneration(kPrimaryOwner) != 0);
        QVERIFY(registry
                    .registerItem(key(kSecondaryOwner, kSecondaryPath, 1),
                                  descriptorWith(QStringLiteral("zeta.identity"),
                                                 QStringLiteral("Zeta app"),
                                                 ItemStatus::NeedsAttention))
                    .accepted());
        QVERIFY(registry
                    .registerItem(key(kPrimaryOwner, kPrimaryPath, 2),
                                  descriptorWith(QStringLiteral("alpha.identity"),
                                                 QString(),
                                                 ItemStatus::Active))
                    .accepted());
        registry.markInitialPopulationComplete();

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
        QVERIFY(registry.beginOwnerGeneration(kPrimaryOwner) != 0);
        QVERIFY(registry
                    .registerItem(key(kPrimaryOwner, kPrimaryPath, 1),
                                  descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                 ItemStatus::NeedsAttention))
                    .accepted());
        registry.markInitialPopulationComplete();

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
        QVERIFY(registry.beginOwnerGeneration(kPrimaryOwner) != 0);
        const OwnerKey owner = key(kPrimaryOwner, kPrimaryPath, 1);
        QVERIFY(registry
                    .registerItem(owner,
                                  descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                 ItemStatus::Active))
                    .accepted());
        registry.markInitialPopulationComplete();

        ItemDescriptor malformed = descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                  ItemStatus::Active);
        malformed.identity = QStringLiteral("   ");
        QVERIFY(!registry.registerItem(owner, malformed).accepted());

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Degraded);
        QCOMPARE(presentation.diagnostic, QStringLiteral("malformed-item-replacement"));
        QCOMPARE(presentation.items.size(), 1);
        QCOMPARE(presentation.items.at(0).accessibleName, QStringLiteral("Title"));
    }

    void fakeTransportDrivesLifecycleIncludingRebaseline()
    {
        StatusNotifierRegistry registry;
        FakeStatusNotifierTransport transport;
        transport.attach(&registry);
        QVERIFY(transport.isAttached());

        transport.emitOwnerAppeared(kPrimaryOwner);
        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity"),
                                                       QStringLiteral("Title"),
                                                       ItemStatus::Active))
                    .accepted());
        transport.emitInitialPopulationComplete();

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
        // replacement population is observed, then Ready again.
        transport.emitWatcherEpoch();
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Loading);
        transport.emitInitialPopulationComplete();
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Ready);
        // The pre-reconnect item survived the watcher swap (its owner never
        // left the bus) and remains actionable under the same generation.
        QCOMPARE(registry.evaluateRequest(live, RequestKind::Activate).outcome.status,
                 RegistryStatus::Accepted);

        QVERIFY(transport.emitItemRemoved(kPrimaryOwner, kPrimaryPath).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QCOMPARE(registry.evaluateRequest(live, RequestKind::Activate).outcome.status,
                 RegistryStatus::UnknownItem);
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Empty);

        // A source restart under the same unique name must be rebased: new
        // generation, stale pre-restart intents refused, fresh item accepted.
        transport.emitOwnerAppeared(kPrimaryOwner);
        QVERIFY(transport.generation(kPrimaryOwner) > live.generation);
        QCOMPARE(registry.evaluateRequest(live, RequestKind::ContextMenu).outcome.status,
                 RegistryStatus::StaleOwner);

        QVERIFY(transport
                    .emitItemRegistered(kPrimaryOwner,
                                        kPrimaryPath,
                                        descriptorWith(QStringLiteral("identity"),
                                                       QStringLiteral("Title"),
                                                       ItemStatus::Passive))
                    .accepted());
        QCOMPARE(registry.count(), qsizetype(1));
        transport.emitOwnerLost(kPrimaryOwner);
        QCOMPARE(registry.count(), qsizetype(0));

        transport.detach();
        QVERIFY(!transport.isAttached());
        // Re-attachment after detach works; attaching twice or a null sink is
        // refused by the contract the fake mirrors.
        transport.attach(&registry);
        QVERIFY(transport.isAttached());
        StatusNotifierEventSink *nullSink = nullptr;
        transport.attach(nullSink);
        QVERIFY(transport.isAttached());
        transport.detach();
        QVERIFY(!transport.isAttached());
    }
};

QTEST_GUILESS_MAIN(StatusNotifierPresentationTests)
#include "tst_status_notifier_presentation.moc"
