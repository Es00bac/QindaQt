// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/session_activation_policy.h"
#include "src/session_supervisor/src/systemd_manager_port.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
using namespace QindaQt::SessionSupervisor;

class SessionActivationPolicyTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void physicalWitness_data() {
        QTest::addColumn<QString>("executable");
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<bool>("physical");
        QTest::newRow("physical") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--drm", "--socket", "qindaqt-0", "--exit-with-session", "qindaqt-session"} << true;
        QTest::newRow("virtual") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--virtual"} << false;
        QTest::newRow("launcher-windowed") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--wayland-display", "wayland-0", "--socket", "test", "--exit-with-session", "qindaqt-session"} << false;
        QTest::newRow("conflicting-wayland-display") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--drm", "--wayland-display=wayland-0"} << false;
        QTest::newRow("windowed") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--windowed"} << false;
        QTest::newRow("implicit-backend") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin"} << false;
        QTest::newRow("conflicting-backends") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--drm", "--wayland"} << false;
        QTest::newRow("x11") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--x11", "--drm"} << false;
        QTest::newRow("payload-drm") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--exit-with-session", "command", "--drm"} << false;
        QTest::newRow("payload-nested") << "/usr/bin/qindaqt-kwin"
            << QStringList{"qindaqt-kwin", "--drm", "--exit-with-session=command --virtual"} << true;
        // ADR-0291: a stock KWin never starts the physical QindaQt session.
        QTest::newRow("stock-kwin") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--drm", "--socket", "qindaqt-0", "--exit-with-session", "qindaqt-session"} << false;
        QTest::newRow("wrong-executable") << "/usr/bin/bash"
            << QStringList{"qindaqt-kwin", "--drm"} << false;
    }
    void physicalWitness() {
        QFETCH(QString, executable);
        QFETCH(QStringList, arguments);
        QFETCH(bool, physical);
        QCOMPARE(activationScopeForCompositor(executable, arguments),
                 physical ? SessionActivationScope::PhysicalDesktop : SessionActivationScope::Private);
    }
    void nondumpableKwinRequiresBothRemainingIdentityWitnesses() {
        const QStringList physical{"qindaqt-kwin", "--drm", "--exit-with-session", "qindaqt-session"};
        QCOMPARE(activationScopeForCompositor({}, physical, QStringLiteral("qindaqt-kwin")),
                 SessionActivationScope::PhysicalDesktop);
        QCOMPARE(activationScopeForCompositor({}, physical, QStringLiteral("bash")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, physical), SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, {"bash", "--drm"}, QStringLiteral("qindaqt-kwin")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor("/usr/bin/bash", physical, QStringLiteral("qindaqt-kwin")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, {"qindaqt-kwin", "--virtual"}, QStringLiteral("qindaqt-kwin")),
                 SessionActivationScope::Private);
    }
    void unavailableWitnessFailsClosed() {
        QCOMPARE(witnessedSessionActivationScope(1), SessionActivationScope::Private);
        QCOMPARE(witnessedSessionActivationScope(QCoreApplication::applicationPid()),
                 SessionActivationScope::Private);
    }
    void privateScopeDeniesBothManagerRoutes() {
        const auto bus = QDBusConnection::sessionBus();
        QCOMPARE(resolveSystemdManagerRoute(bus).kind, SystemdManagerRoute::Kind::Unavailable);
        QCOMPARE(resolveSystemdManagerRoute(bus, QStringLiteral("/nonexistent")).kind,
                 SystemdManagerRoute::Kind::Unavailable);
    }
    void hermeticOverrideAndPhysicalNativeRouteArePreserved() {
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        QDir().mkpath(runtime.path() + QStringLiteral("/systemd"));
        const QString socket = runtime.path() + QStringLiteral("/systemd/private");
        QFile marker(socket);
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        qputenv("XDG_RUNTIME_DIR", runtime.path().toLocal8Bit());
        const auto bus = QDBusConnection::sessionBus();
        QCOMPARE(resolveSystemdManagerRoute(bus).kind, SystemdManagerRoute::Kind::Unavailable);
        const auto physical = resolveSystemdManagerRoute(bus, {}, SessionActivationScope::PhysicalDesktop);
        QCOMPARE(physical.kind, SystemdManagerRoute::Kind::Native);
        QCOMPARE(physical.address, QStringLiteral("unix:path=") + socket);
        QVERIFY(!physical.requiresBusHello);
        const auto fake = resolveSystemdManagerRoute(bus, socket);
        QCOMPARE(fake.kind, SystemdManagerRoute::Kind::Native);
        QVERIFY(fake.requiresBusHello);
    }
};
QTEST_GUILESS_MAIN(SessionActivationPolicyTests)
#include "tst_session_activation_policy.moc"
