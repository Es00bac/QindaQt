// SPDX-License-Identifier: LGPL-3.0-or-later

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

OwnerKey key(const QString &owner, const QString &path, quint32 generation)
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
// events through the public registry API and never touches a bus.
class FakeStatusNotifierTransport final : public StatusNotifierTransport
{
public:
    void attach(StatusNotifierRegistry *sink) override
    {
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
        m_sink->ownerLost(owner);
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

    [[nodiscard]] quint32 generation(const QString &owner) const
    {
        return m_allocatedGenerations.value(owner);
    }

private:
    StatusNotifierRegistry *m_sink = nullptr;
    QHash<QString, quint32> m_allocatedGenerations;
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

    void showsDegradedWhenWatcherIsUnavailable()
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
        // A watcher loss presents no items: stale tray entries would invite
        // activation intents against owners that may no longer exist.
        QVERIFY(presentation.items.isEmpty());
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
                    .registerItem(key(kPrimaryOwner, kPrimaryPath, 1),
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

    void degradedRegistryKeepsLastKnownGoodItems()
    {
        StatusNotifierRegistry registry;
        QVERIFY(registry.beginOwnerGeneration(kPrimaryOwner) != 0);
        const OwnerKey owner = key(kPrimaryOwner,
                                   kPrimaryPath,
                                   1);
        QVERIFY(registry.registerItem(owner,
                                      descriptorWith(QStringLiteral("id"),
                                                     QStringLiteral("Title"),
                                                     ItemStatus::Active))
                    .accepted());
        registry.markInitialPopulationComplete();

        ItemDescriptor malformed = descriptorWith(QStringLiteral("id"), QStringLiteral("Title"),
                                                  ItemStatus::Active);
        malformed.identity.clear();
        QVERIFY(!registry.registerItem(owner, malformed).accepted());

        const TrayPresentation presentation =
            projectPresentation(registry, {.transportLive = true});
        QCOMPARE(presentation.state, PresentationState::Degraded);
        QCOMPARE(presentation.diagnostic, QStringLiteral("malformed-item-replacement"));
        QCOMPARE(presentation.items.size(), 1);
        QCOMPARE(presentation.items.at(0).accessibleName, QStringLiteral("Title"));
    }

    void fakeTransportDrivesFullLifecycle()
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
            QCOMPARE(registry.evaluateRequest(live, kind).status, RegistryStatus::Accepted);
        }
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Ready);

        QVERIFY(transport.emitItemRemoved(kPrimaryOwner, kPrimaryPath).accepted());
        QCOMPARE(registry.count(), qsizetype(0));
        QCOMPARE(registry.evaluateRequest(live, RequestKind::Activate).status,
                 RegistryStatus::UnknownItem);
        QCOMPARE(projectPresentation(registry, {.transportLive = true}).state,
                 PresentationState::Empty);

        transport.emitOwnerAppeared(kPrimaryOwner);
        QVERIFY(transport.generation(kPrimaryOwner) > quint32(1));

        // An intent stamped with the pre-restart generation must be refused.
        QCOMPARE(registry.evaluateRequest(live, RequestKind::ContextMenu).status,
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
    }
};

QTEST_GUILESS_MAIN(StatusNotifierPresentationTests)
#include "tst_status_notifier_presentation.moc"
