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

    void restore(QWindow &window) override
    {
        cancel(window);
        ++restores;
        // The production port fades the scene root, not the window, so the
        // double records the request rather than touching window opacity.
        restored = &window;
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
    int restores = 0;
    QWindow *restored = nullptr;
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
    void savedDelayChangesPointerLeaveTimingAcrossProfileSwitch();
    void foregroundTransitions();
    void foregroundReleasesPointerReveal();
    void boundaryPoison();
};

void PanelVisibilityProducerTests::pointer()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity(), identity("dock", "other")}));
    FakeTimer timer;
    Shell::PanelVisibilityPointerProducer producer(
        *qGuiApp, store, timer);
    producer.setIdentities({identity(), identity("dock", "other")});
    producer.setLeaveDelayMilliseconds(375);

    producer.pointerEntered(identity("unknown"));
    QVERIFY(!reveal(store));
    producer.pointerEntered(identity());
    QVERIFY(reveal(store));
    producer.pointerEntered(identity("dock", "other"));
    QVERIFY(store.snapshot()[1].revealRequested);
    producer.clearReveals(QStringLiteral("main"));
    QVERIFY(!reveal(store));
    QVERIFY(store.snapshot()[1].revealRequested);
    producer.clearReveals(QStringLiteral("other"));
    QVERIFY(!store.snapshot()[1].revealRequested);
    producer.pointerEntered(identity());
    QVERIFY(reveal(store));
    producer.pointerLeft(identity());
    QVERIFY(reveal(store));
    QCOMPARE(timer.lastDelay, 375);
    timer.fireAll();
    QVERIFY(!reveal(store));

    // Clearing a pending leave must cancel its timer and release immediately.
    producer.pointerEntered(identity());
    producer.pointerLeft(identity());
    producer.clearReveals(QStringLiteral("main"));
    QVERIFY(!reveal(store));
    QVERIFY(timer.callbacks.empty());
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

    // Popup discovery walks the whole object tree of every panel window, and
    // the shell calls it on every visibility synchronize. It must not repeat
    // that walk while nothing has been parented into any tree: the panel tree
    // grows over a session, so an unconditional walk makes every visibility
    // transition more expensive the longer the shell runs.
    producer.synchronizePopupObjects();
    const quint64 afterFirst = producer.discoveryPasses();
    QVERIFY(afterFirst > 0);
    producer.synchronizePopupObjects();
    producer.synchronizePopupObjects();
    QCOMPARE(producer.discoveryPasses(), afterFirst);

    // A new object parented anywhere in the process may have put a popup in a
    // panel tree, so the next pass must discover again.
    auto *const child = new QObject(&owner);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::ChildAdded);
    producer.synchronizePopupObjects();
    QCOMPARE(producer.discoveryPasses(), afterFirst + 1);
    delete child;
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
    // A finished hide fade must ask for a reconcile: the panel is still
    // mapped at zero opacity until the surface plan is reevaluated, so
    // without this it is invisible but still taking input.
    QSignalSpy reconciles(
        &producer, &Shell::PanelVisibilityAnimationProducer::reconcileRequested);
    animationPort.finish();
    QCOMPARE(reconciles.count(), 1);
    QVERIFY(!held(store));
    QVERIFY(!producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Unmapped),
                                  {identity()}, true, 240));

    // Losing compositor authority abandons the transition. The panel must be
    // returned to fully opaque through the port: the production port fades the
    // QQuickWindow scene root, because the Wayland platform implements no
    // window opacity, so a producer that reset QWindow::opacity itself would
    // leave the panel stranded part-faded (ADR-0236).
    animationPort.restores = 0;
    animationPort.restored = nullptr;
    QVERIFY(!producer.synchronize(plan(ShellSurface::PanelSurfaceMapping::Unmapped),
                                  {identity()}, false, 240));
    QCOMPARE(animationPort.restores, 1);
    QCOMPARE(animationPort.restored, &panel);
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

void PanelVisibilityProducerTests::savedDelayChangesPointerLeaveTimingAcrossProfileSwitch()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    NullSettingsTransport transport;
    Services::SettingsClient::SettingsClient settings(transport, {});
    FakeRegistrar registrar;
    Profiles::LayoutProfile profile;
    Profiles::PanelSpec panel;
    panel.id = QStringLiteral("dock");
    panel.hideMode = Profiles::HideMode::Intelligent;
    profile.panels.append(panel);
    Shell::PanelVisibilityRuntime runtime(
        *qGuiApp, store, settings, registrar, profile, 320);
    bool reconcile = false;
    QString error;
    QWindow panelWindow;
    panelWindow.setObjectName(QStringLiteral("qindaqt-panel-dock@main"));
    QEvent enter(QEvent::Enter);
    QEvent leave(QEvent::Leave);

    runtime.applySettings({{QStringLiteral("accessibility.reducedMotion"), true},
                           {QStringLiteral("panels.autoHideDelayMs"), qint64(50)}});
    (void)runtime.synchronize(plan(ShellSurface::PanelSurfaceMapping::Mapped),
                              false, &reconcile, &error);
    QCOMPARE(runtime.animationDurationMilliseconds(), 80);
    QCoreApplication::sendEvent(&panelWindow, &enter);
    QVERIFY(reveal(store));
    QCoreApplication::sendEvent(&panelWindow, &leave);
    QTRY_VERIFY_WITH_TIMEOUT(!reveal(store), 500);

    // The setting is global: adopting a new profile refreshes inventory, not
    // Settings1 delay. A longer saved value measurably extends the leave hold.
    Profiles::LayoutProfile alternate = profile;
    alternate.id = QStringLiteral("alternate");
    runtime.applySettings({{QStringLiteral("accessibility.reducedMotion"), false},
                           {QStringLiteral("panels.autoHideDelayMs"), qint64(500)}});
    runtime.applyProfile(alternate);
    (void)runtime.synchronize(plan(ShellSurface::PanelSurfaceMapping::Mapped),
                              false, &reconcile, &error);
    QCOMPARE(runtime.animationDurationMilliseconds(), 320);
    QCoreApplication::sendEvent(&panelWindow, &enter);
    QVERIFY(reveal(store));
    QCoreApplication::sendEvent(&panelWindow, &leave);
    QTest::qWait(100);
    QVERIFY(reveal(store));
    QTRY_VERIFY_WITH_TIMEOUT(!reveal(store), 1'000);
}

void PanelVisibilityProducerTests::foregroundTransitions()
{
    ShellOrchestration::PanelInteractionStore store;
    NullSettingsTransport transport;
    Services::SettingsClient::SettingsClient settings(transport, {});
    FakeRegistrar registrar;
    Profiles::LayoutProfile profile;
    Shell::PanelVisibilityRuntime runtime(
        *qGuiApp, store, settings, registrar, profile, 0);

    ShellVisibility::CompositorVisibilitySnapshot snapshot;
    snapshot.outputs = {
        {QStringLiteral("left"), QRect(0, 0, 1920, 1080)},
        {QStringLiteral("right"), QRect(1920, 0, 1920, 1080)}};
    ShellVisibility::LogicalWindowSnapshot active;
    active.id = QStringLiteral("first");
    active.outputId = QStringLiteral("left");
    active.frameGeometry = QRect(100, 100, 800, 600);
    active.active = true;
    snapshot.windows = {active};
    QVERIFY(runtime.observeForeground(snapshot).isEmpty());

    // The compositor assigns the spanning window to left, but its frame
    // crosses right. A focus change must invalidate reveals on both outputs.
    snapshot.windows[0].id = QStringLiteral("spanning");
    snapshot.windows[0].frameGeometry = QRect(100, 0, 3000, 1080);
    QCOMPARE(runtime.observeForeground(snapshot),
             (QStringList{QStringLiteral("left"), QStringLiteral("right")}));
    QVERIFY(runtime.observeForeground(snapshot).isEmpty());

    // An overlay-only layout lets ordinary maximized clients fill the whole
    // output. Entering fullscreen on that same client is a separate change.
    snapshot.windows[0].frameGeometry = QRect(0, 0, 1920, 1080);
    snapshot.windows[0].maximized = true;
    QCOMPARE(runtime.observeForeground(snapshot),
             (QStringList{QStringLiteral("left"), QStringLiteral("right")}));
    snapshot.windows[0].maximized = false;
    QCOMPARE(runtime.observeForeground(snapshot),
             (QStringList{QStringLiteral("left")}));
    QVERIFY(runtime.observeForeground(snapshot).isEmpty());

    // Some clients keep the maximize bit in fullscreen; the explicit KWin
    // state must independently invalidate the reveal for the same window.
    snapshot.windows[0].maximized = true;
    (void)runtime.observeForeground(snapshot);
    snapshot.windows[0].fullscreen = true;
    QCOMPARE(runtime.observeForeground(snapshot),
             (QStringList{QStringLiteral("left")}));
}

void PanelVisibilityProducerTests::foregroundReleasesPointerReveal()
{
    ShellOrchestration::PanelInteractionStore store;
    QVERIFY(store.setIdentities({identity()}));
    NullSettingsTransport transport;
    Services::SettingsClient::SettingsClient settings(transport, {});
    FakeRegistrar registrar;
    Profiles::LayoutProfile profile;
    Profiles::PanelSpec panelSpec;
    panelSpec.id = QStringLiteral("dock");
    panelSpec.hideMode = Profiles::HideMode::Intelligent;
    profile.panels.append(panelSpec);
    Shell::PanelVisibilityRuntime runtime(
        *qGuiApp, store, settings, registrar, profile, 0);
    bool immediateReconcile = false;
    QString error;
    // Even a platform without layer-shell support sets producer identities
    // before the edge backend attempts to create its one-pixel sensor.
    (void)runtime.synchronize(plan(ShellSurface::PanelSurfaceMapping::Mapped),
                              false, &immediateReconcile, &error);

    QWindow panelWindow;
    panelWindow.setObjectName(QStringLiteral("qindaqt-panel-dock@main"));
    QEvent enter(QEvent::Enter);
    QCoreApplication::sendEvent(&panelWindow, &enter);
    QVERIFY(reveal(store));

    ShellVisibility::CompositorVisibilitySnapshot snapshot;
    snapshot.outputs = {{QStringLiteral("main"), QRect(0, 0, 1920, 1080)}};
    ShellVisibility::LogicalWindowSnapshot active;
    active.id = QStringLiteral("first");
    active.outputId = QStringLiteral("main");
    active.frameGeometry = QRect(100, 100, 800, 600);
    active.active = true;
    snapshot.windows = {active};
    QVERIFY(runtime.observeForeground(snapshot).isEmpty());
    QVERIFY(reveal(store));

    snapshot.windows[0].id = QStringLiteral("second");
    snapshot.windows[0].maximized = true;
    QCOMPARE(runtime.observeForeground(snapshot),
             (QStringList{QStringLiteral("main")}));
    QVERIFY(!reveal(store));
}

void PanelVisibilityProducerTests::boundaryPoison()
{
    const auto *meta = &Shell::PanelVisibilityRuntime::staticMetaObject;
    QCOMPARE(QString::fromLatin1(meta->className()),
             QStringLiteral("QindaQt::Shell::PanelVisibilityRuntime"));
    QCOMPARE(meta->indexOfMethod("setPanelVisible(QString,bool)"), -1);
    QCOMPARE(meta->indexOfMethod("setReservation(QString,bool)"), -1);
    QCOMPARE(meta->indexOfProperty("compositorWindowInventory"), -1);
    // The forwarding signal for a finished hide fade. The shell runtime
    // connects it to the debounced reconcile; without it the hide path
    // depends on the visibility-hold lease release happening to emit
    // PanelInteractionStore::interactionsChanged.
    QVERIFY(meta->indexOfSignal("reconcileRequested()") >= 0);
}

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    PanelVisibilityProducerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_panelvisibilityproducers.moc"
