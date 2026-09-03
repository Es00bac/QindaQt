// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitypopup.h"
#include "panelvisibilitytimer.h"

#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QGuiApplication>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

using namespace QindaQt;

namespace {

using Identity = ShellVisibility::PanelSurfaceIdentity;

class FakeTimer final : public Shell::PanelVisibilityTimerPort {
public:
    quint64 schedule(int delay, std::function<void()> callback) override
    {
        lastDelay = delay;
        const quint64 token = nextToken++;
        callbacks.emplace(token, std::move(callback));
        return token;
    }

    void cancel(quint64 token) override { callbacks.erase(token); }

    void fireAll()
    {
        auto pending = std::move(callbacks);
        callbacks.clear();
        for (auto &[token, callback] : pending) {
            Q_UNUSED(token)
            callback();
        }
    }

    std::map<quint64, std::function<void()>> callbacks;
    quint64 nextToken = 1;
    int lastDelay = -1;
};

Identity identity(const QString &panel, const QString &output = QStringLiteral("main"))
{
    return {panel, output};
}

bool held(const ShellOrchestration::PanelInteractionStore &store)
{
    const auto snapshot = store.snapshot();
    return std::any_of(snapshot.cbegin(), snapshot.cend(),
                       [](const auto &item) { return item.visibilityHeld; });
}

} // namespace

class PanelVisibilityPopupBoundsTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void boundsSourcesAndLeases();
    void fencesOwnerAndMaximumLifetime();
};

void PanelVisibilityPopupBoundsTests::boundsSourcesAndLeases()
{
    // AGENT-NOTE: P1-2 rejected the former unrestricted source map. Keep both
    // independent source and aggregate lease limits mutation-sensitive.
    ShellOrchestration::PanelInteractionStore store;
    QVector<Identity> identities;
    for (int index = 0; index < 5; ++index) {
        identities.append(identity(QStringLiteral("panel-%1").arg(index)));
    }
    QVERIFY(store.setIdentities(identities));
    FakeTimer timer;
    Shell::PanelVisibilityPopupProducer producer(*qGuiApp, store, timer);
    producer.setIdentities(identities);

    std::vector<std::unique_ptr<QObject>> owners;
    for (qsizetype index = 0;
         index < Shell::PanelVisibilityPopupProducer::MaximumLeases / 5;
         ++index) {
        owners.push_back(std::make_unique<QObject>());
        QVERIFY(producer.setPopupVisible(
            owners.back().get(), QStringLiteral("source-%1").arg(index), {}, true));
    }
    auto overflowOwner = std::make_unique<QObject>();
    QVERIFY(!producer.setPopupVisible(
        overflowOwner.get(), QStringLiteral("lease-overflow"), {}, true));
    QVERIFY(held(store));

    const QString oversized(
        Shell::PanelVisibilityPopupProducer::MaximumSourceIdLength + 1,
        QLatin1Char('x'));
    QVERIFY(!producer.setPopupVisible(overflowOwner.get(), oversized,
                                      QStringLiteral("main"), true));
    QVERIFY(!producer.setPopupVisible(nullptr, QStringLiteral("null-owner"),
                                      QStringLiteral("main"), true));

    const QVector<Identity> single{identity(QStringLiteral("single"))};
    producer.setIdentities(single);
    QVERIFY(store.setIdentities(single));
    owners.clear();
    for (qsizetype index = 0;
         index < Shell::PanelVisibilityPopupProducer::MaximumSources;
         ++index) {
        owners.push_back(std::make_unique<QObject>());
        QVERIFY(producer.setPopupVisible(
            owners.back().get(), QStringLiteral("bounded-%1").arg(index),
            QStringLiteral("main"), true));
    }
    QVERIFY(!producer.setPopupVisible(
        overflowOwner.get(), QStringLiteral("source-overflow"),
        QStringLiteral("main"), true));
}

void PanelVisibilityPopupBoundsTests::fencesOwnerAndMaximumLifetime()
{
    // AGENT-NOTE: P1-2 also requires owner loss, destruction without Close,
    // and one bounded uninterrupted hold to release every acquired lease.
    ShellOrchestration::PanelInteractionStore store;
    const Identity panel = identity(QStringLiteral("dock"));
    QVERIFY(store.setIdentities({panel}));
    FakeTimer timer;
    Shell::PanelVisibilityPopupProducer producer(*qGuiApp, store, timer);
    producer.setIdentities({panel});

    auto owner = std::make_unique<QObject>();
    QVERIFY(producer.setPopupVisible(owner.get(), QStringLiteral("launcher"),
                                     QStringLiteral("main"), true));
    QCOMPARE(timer.lastDelay,
             Shell::PanelVisibilityPopupProducer::MaximumHoldMilliseconds);
    QVERIFY(held(store));
    QObject impostor;
    QVERIFY(!producer.setPopupVisible(&impostor, QStringLiteral("launcher"),
                                      QStringLiteral("main"), false));
    QVERIFY(held(store));
    owner.reset();
    QVERIFY(!held(store));

    QObject orphanedOwner;
    QVERIFY(producer.setPopupVisible(&orphanedOwner, QStringLiteral("center"),
                                     QStringLiteral("main"), true));
    QVERIFY(producer.setPopupVisible(&orphanedOwner, QStringLiteral("center"),
                                     QStringLiteral("main"), true));
    QCOMPARE(timer.callbacks.size(), std::size_t(1));
    timer.fireAll();
    QVERIFY(!held(store));
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    PanelVisibilityPopupBoundsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_panelvisibilitypopupbounds.moc"
