// SPDX-License-Identifier: GPL-3.0-or-later
#include "globalshortcutregistrar.h"
#include "panelvisibilityanimation.h"
#include "panelvisibilitypointer.h"
#include "panelvisibilitypopup.h"
#include "panelvisibilityruntime.h"
#include "panelvisibilityshortcut.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/shell_orchestration/panel_interaction_store.h"

#include <QAction>
#include <QGuiApplication>
#include <QWindow>
#include <QtTest>

#include <functional>
#include <map>
#include <utility>

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

class FakeRegistrar final : public Shell::GlobalShortcutRegistrar {
public:
    Shell::GlobalShortcutRegistration registerShortcut(
        QAction &candidate, const QKeySequence &shortcut, QObject &,
        std::function<void(bool)> callback) override
    {
        action = &candidate;
        observedShortcut = shortcut;
        bindingChanged = std::move(callback);
        return result;
    }

    Shell::GlobalShortcutRegistration result{true, true};
    QAction *action = nullptr;
    QKeySequence observedShortcut;
    std::function<void(bool)> bindingChanged;
};

class FakeAnimation final : public Shell::PanelVisibilityAnimationPort {
public:
    void animate(QWindow &, qreal fromValue, qreal toValue, int duration,
                 std::function<void()> callback) override
    {
        from = fromValue;
        to = toValue;
        milliseconds = duration;
        completed = std::move(callback);
        ++starts;
    }

    void cancel(QWindow &) override
    {
        completed = {};
        ++cancels;
    }

    void finish()
    {
        auto callback = std::move(completed);
        if (callback) {
            callback();
        }
    }

    std::function<void()> completed;
    qreal from = -1.0;
    qreal to = -1.0;
    int milliseconds = -1;
    int starts = 0;
    int cancels = 0;
};

class NullSettingsTransport final
    : public Services::SettingsClient::SettingsTransport {
public:
    using SettingsTransport::SettingsTransport;
    bool start(QString *error) override
    {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}
    void requestSnapshot(quint64, const QString &, const QStringList &) override {}
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override {}
    void requestActivation() override {}
};

Identity identity(const char *panel = "dock", const char *output = "main")
{
    return {QString::fromLatin1(panel), QString::fromLatin1(output)};
}

bool reveal(const ShellOrchestration::PanelInteractionStore &store)
{
    return store.snapshot().constFirst().revealRequested;
}

bool held(const ShellOrchestration::PanelInteractionStore &store)
{
    return store.snapshot().constFirst().visibilityHeld;
}

ShellSurface::PanelSurfacePlan plan(
    ShellSurface::PanelSurfaceMapping mapping)
{
    ShellSurface::PanelSurfaceConfiguration surface;
    surface.identity = {QStringLiteral("dock"), QStringLiteral("main")};
    surface.outputGeometry = {0, 0, 1920, 1080};
    surface.geometry = {0, 1032, 1920, 48};
    surface.desiredSize = surface.geometry.size();
    surface.edge = Profiles::Edge::Bottom;
    surface.mapping = mapping;
    return {{surface}, {}};
}

} // namespace

class PanelVisibilityProducerTests final : public QObject {
    Q_OBJECT
private slots:
    void pointer();
    void popup();
    void shortcut();
    void animation();
    void reducedMotion();
    void boundaryPoison();
};

void PanelVisibilityProducerTests::pointer()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    FakeTimer timer;
    Shell::PanelVisibilityPointerProducer producer(
        *qGuiApp, store, timer);
    producer.setIdentities({identity()});
    producer.setLeaveDelayMilliseconds(375);

    producer.pointerEntered(identity("unknown"));
    QVERIFY(!reveal(store));
    producer.pointerEntered(identity());
    QVERIFY(reveal(store));
    producer.pointerLeft(identity());
    QVERIFY(reveal(store));
    QCOMPARE(timer.lastDelay, 375);
    timer.fireAll();
    QVERIFY(!reveal(store));
}

void PanelVisibilityProducerTests::popup()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity(), identity("dock", "other")}));
    FakeTimer timer;
    Shell::PanelVisibilityPopupProducer producer(*qGuiApp, store, timer);
    producer.setIdentities({identity(), identity("dock", "other")});
    QObject owner;

    QVERIFY(producer.setPopupVisible(&owner, QStringLiteral("launcher"),
                                     QStringLiteral("main"), true));
    const auto opened = store.snapshot();
    QVERIFY(opened[0].visibilityHeld);
    QVERIFY(!opened[1].visibilityHeld);
    QVERIFY(producer.setPopupVisible(&owner, QStringLiteral("launcher"),
                                     QStringLiteral("main"), false));
    for (const auto &item : store.snapshot()) {
        QVERIFY(!item.visibilityHeld);
    }
}

void PanelVisibilityProducerTests::shortcut()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    FakeTimer timer;
    FakeRegistrar registrar;
    Shell::PanelVisibilityShortcutProducer producer(registrar, store, timer);
    producer.setIdentities({identity()});
    producer.setHoldMilliseconds(2'125);

    QCOMPARE(registrar.action, producer.action());
    QCOMPARE(registrar.action->objectName(),
             QStringLiteral("qindaqt_reveal_panels"));
    QCOMPARE(registrar.observedShortcut,
             QKeySequence(Qt::META | Qt::Key_Space));
    registrar.action->trigger();
    QVERIFY(reveal(store));
    QCOMPARE(timer.lastDelay, 2'125);
    timer.fireAll();
    QVERIFY(!reveal(store));
}

void PanelVisibilityProducerTests::animation()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    FakeAnimation animationPort;
    QWindow panel;
    panel.setObjectName(QStringLiteral("qindaqt-panel-dock@main"));
    Shell::PanelVisibilityAnimationProducer producer(
        *qGuiApp, store, animationPort);

    QVERIFY(!producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Mapped),
                                  {identity()}, true, 240));
    QVERIFY(producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Unmapped),
                                 {identity()}, true, 240));
    QCOMPARE(animationPort.starts, 1);
    QCOMPARE(animationPort.from, 1.0);
    QCOMPARE(animationPort.to, 0.0);
    QCOMPARE(animationPort.milliseconds, 240);
    QVERIFY(held(store));
    animationPort.finish();
    QVERIFY(!held(store));
    QVERIFY(!producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Unmapped),
                                  {identity()}, true, 240));

    panel.setOpacity(0.25);
    QVERIFY(!producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Unmapped),
                                  {identity()}, false, 240));
    QCOMPARE(panel.opacity(), 1.0);
}

void PanelVisibilityProducerTests::reducedMotion()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    NullSettingsTransport transport;
    Services::SettingsClient::SettingsClient settings(
        transport, {QStringLiteral("accessibility.reducedMotion"),
                    QStringLiteral("panels.autoHideDelayMs")});
    FakeRegistrar registrar;
    Profiles::LayoutProfile profile;
    Profiles::PanelSpec panel;
    panel.id = QStringLiteral("dock");
    panel.hideMode = Profiles::HideMode::Always;
    profile.panels.append(panel);
    Shell::PanelVisibilityRuntime runtime(
        *qGuiApp, store, settings, registrar, profile, 320);

    QVERIFY(runtime.reducedMotion());
    QCOMPARE(runtime.animationDurationMilliseconds(), 80);
    // AGENT-NOTE: P1-1 requires the canonical Settings1 signed 64-bit value;
    // an `int` literal would repeat the rejected candidate's vacuous fixture.
    runtime.applySettings({{QStringLiteral("accessibility.reducedMotion"), false},
                           {QStringLiteral("panels.autoHideDelayMs"), qint64(400)}});
    QVERIFY(!runtime.reducedMotion());
    QCOMPARE(runtime.animationDurationMilliseconds(), 320);
    runtime.applySettings({{QStringLiteral("accessibility.reducedMotion"),
                            QStringLiteral("not-a-bool")},
                           {QStringLiteral("panels.autoHideDelayMs"), -1}});
    QVERIFY(!runtime.reducedMotion());
    QCOMPARE(runtime.animationDurationMilliseconds(), 320);
}

void PanelVisibilityProducerTests::boundaryPoison()
{
    const auto *meta = &Shell::PanelVisibilityRuntime::staticMetaObject;
    QCOMPARE(QString::fromLatin1(meta->className()),
             QStringLiteral("QindaQt::Shell::PanelVisibilityRuntime"));
    QCOMPARE(meta->indexOfMethod("setPanelVisible(QString,bool)"), -1);
    QCOMPARE(meta->indexOfMethod("setReservation(QString,bool)"), -1);
    QCOMPARE(meta->indexOfProperty("compositorWindowInventory"), -1);
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    PanelVisibilityProducerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_panelvisibilityproducers.moc"
