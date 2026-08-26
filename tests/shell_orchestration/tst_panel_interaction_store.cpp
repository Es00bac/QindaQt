// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <utility>

using namespace QindaQt;

namespace {

ShellVisibility::PanelSurfaceIdentity identity(const char *panel,
                                               const char *output = "main")
{
    return {QString::fromLatin1(panel), QString::fromLatin1(output)};
}

} // namespace

class PanelInteractionStoreTests final : public QObject {
    Q_OBJECT

private slots:
    void publishesEveryKnownIdentityInDeterministicOrder();
    void independentLeasesDoNotClearEachOther();
    void moveTransfersExactlyOneReleaseResponsibility();
    void identityReplacementInvalidatesRemovedLeasesAtomically();
    void rejectsMalformedDuplicateAndUnknownInputsWithoutMutation();
    void leasesBecomeHarmlessWhenTheStoreIsDestroyed();
};

void PanelInteractionStoreTests::
    publishesEveryKnownIdentityInDeterministicOrder()
{
    ShellOrchestration::PanelInteractionStore store;
    QSignalSpy changed(&store,
                       &ShellOrchestration::PanelInteractionStore::interactionsChanged);
    QVERIFY(store.setIdentities({identity("z", "secondary"),
                                 identity("b"), identity("a")}));

    const auto snapshot = store.snapshot();
    QCOMPARE(snapshot.size(), 3);
    QCOMPARE(snapshot[0].identity, identity("a"));
    QCOMPARE(snapshot[1].identity, identity("b"));
    QCOMPARE(snapshot[2].identity, identity("z", "secondary"));
    for (const auto &entry : snapshot) {
        QVERIFY(!entry.revealRequested);
        QVERIFY(!entry.visibilityHeld);
    }
    QCOMPARE(changed.size(), 1);

    QVERIFY(store.setIdentities({identity("b"), identity("a"),
                                 identity("z", "secondary")}));
    QCOMPARE(changed.size(), 1);
}

void PanelInteractionStoreTests::independentLeasesDoNotClearEachOther()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity("dock")}));
    QSignalSpy changed(&store,
                       &ShellOrchestration::PanelInteractionStore::interactionsChanged);

    auto firstReveal = store.acquire(
        identity("dock"), ShellOrchestration::PanelInteractionKind::Reveal);
    QVERIFY(firstReveal.has_value());
    QCOMPARE(changed.size(), 1);
    auto secondReveal = store.acquire(
        identity("dock"), ShellOrchestration::PanelInteractionKind::Reveal);
    QVERIFY(secondReveal.has_value());
    QCOMPARE(changed.size(), 1);
    auto hold = store.acquire(
        identity("dock"), ShellOrchestration::PanelInteractionKind::VisibilityHold);
    QVERIFY(hold.has_value());
    QCOMPARE(changed.size(), 2);
    QVERIFY(store.snapshot().constFirst().revealRequested);
    QVERIFY(store.snapshot().constFirst().visibilityHeld);

    firstReveal->reset();
    QCOMPARE(changed.size(), 2);
    QVERIFY(store.snapshot().constFirst().revealRequested);
    secondReveal->reset();
    QCOMPARE(changed.size(), 3);
    QVERIFY(!store.snapshot().constFirst().revealRequested);
    QVERIFY(store.snapshot().constFirst().visibilityHeld);
    hold->reset();
    QCOMPARE(changed.size(), 4);
    QVERIFY(!store.snapshot().constFirst().visibilityHeld);
}

void PanelInteractionStoreTests::moveTransfersExactlyOneReleaseResponsibility()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity("dock")}));
    auto original = store.acquire(
        identity("dock"), ShellOrchestration::PanelInteractionKind::Reveal);
    QVERIFY(original.has_value());

    ShellOrchestration::PanelInteractionLease moved(std::move(*original));
    QVERIFY(!original->valid());
    QVERIFY(moved.valid());
    ShellOrchestration::PanelInteractionLease assigned;
    assigned = std::move(moved);
    QVERIFY(!moved.valid());
    QVERIFY(assigned.valid());
    QVERIFY(store.snapshot().constFirst().revealRequested);

    assigned.reset();
    QVERIFY(!store.snapshot().constFirst().revealRequested);
    assigned.reset();
    QVERIFY(!assigned.valid());
}

void PanelInteractionStoreTests::
    identityReplacementInvalidatesRemovedLeasesAtomically()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity("one"), identity("two")}));
    auto removed = store.acquire(
        identity("one"), ShellOrchestration::PanelInteractionKind::Reveal);
    auto retained = store.acquire(
        identity("two"), ShellOrchestration::PanelInteractionKind::VisibilityHold);
    QVERIFY(removed.has_value());
    QVERIFY(retained.has_value());
    QSignalSpy changed(&store,
                       &ShellOrchestration::PanelInteractionStore::interactionsChanged);

    QVERIFY(store.setIdentities({identity("two"), identity("three")}));

    QVERIFY(!removed->valid());
    QVERIFY(retained->valid());
    const auto snapshot = store.snapshot();
    QCOMPARE(snapshot.size(), 2);
    QCOMPARE(snapshot[0].identity, identity("three"));
    QCOMPARE(snapshot[1].identity, identity("two"));
    QVERIFY(snapshot[1].visibilityHeld);
    QCOMPARE(changed.size(), 1);
    removed->reset();
    QCOMPARE(changed.size(), 1);
}

void PanelInteractionStoreTests::
    rejectsMalformedDuplicateAndUnknownInputsWithoutMutation()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity("dock")}));
    const auto retained = store.snapshot();
    QString error;

    QVERIFY(!store.setIdentities({identity("dock"), identity("dock")}, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.snapshot(), retained);

    error.clear();
    auto lease = store.acquire(
        identity("missing"), ShellOrchestration::PanelInteractionKind::Reveal,
        &error);
    QVERIFY(!lease.has_value());
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.snapshot(), retained);

    error.clear();
    lease = store.acquire(
        identity("dock"),
        static_cast<ShellOrchestration::PanelInteractionKind>(99), &error);
    QVERIFY(!lease.has_value());
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.snapshot(), retained);
}

void PanelInteractionStoreTests::leasesBecomeHarmlessWhenTheStoreIsDestroyed()
{
    auto store = std::make_unique<ShellOrchestration::PanelInteractionStore>();
    QVERIFY(store->setIdentities({identity("dock")}));
    auto lease = store->acquire(
        identity("dock"), ShellOrchestration::PanelInteractionKind::VisibilityHold);
    QVERIFY(lease.has_value());
    QVERIFY(lease->valid());

    store.reset();

    QVERIFY(!lease->valid());
    lease->reset();
    QVERIFY(!lease->valid());
}

QTEST_GUILESS_MAIN(PanelInteractionStoreTests)
#include "tst_panel_interaction_store.moc"
