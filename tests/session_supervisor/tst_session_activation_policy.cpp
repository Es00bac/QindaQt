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
        QTest::newRow("physical") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--drm", "--socket", "qindaqt-0", "--exit-with-session", "qindaqt-session"} << true;
        QTest::newRow("virtual") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--virtual"} << false;
        QTest::newRow("launcher-windowed") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--wayland-display", "wayland-0", "--socket", "test", "--exit-with-session", "qindaqt-session"} << false;
        QTest::newRow("conflicting-wayland-display") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--drm", "--wayland-display=wayland-0"} << false;
        QTest::newRow("windowed") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--windowed"} << false;
        QTest::newRow("implicit-backend") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland"} << false;
        QTest::newRow("conflicting-backends") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--drm", "--wayland"} << false;
        QTest::newRow("x11") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--x11", "--drm"} << false;
        QTest::newRow("payload-drm") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--exit-with-session", "command", "--drm"} << false;
        QTest::newRow("payload-nested") << "/usr/bin/kwin_wayland"
            << QStringList{"kwin_wayland", "--drm", "--exit-with-session=command --virtual"} << true;
        QTest::newRow("wrong-executable") << "/usr/bin/bash"
            << QStringList{"kwin_wayland", "--drm"} << false;
    }
    void physicalWitness() {
        QFETCH(QString, executable);
        QFETCH(QStringList, arguments);
        QFETCH(bool, physical);
        QCOMPARE(activationScopeForCompositor(executable, arguments),
                 physical ? SessionActivationScope::PhysicalDesktop : SessionActivationScope::Private);
    }
    void nondumpableKwinRequiresBothRemainingIdentityWitnesses() {
        const QStringList physical{"kwin_wayland", "--drm", "--exit-with-session", "qindaqt-session"};
        QCOMPARE(activationScopeForCompositor({}, physical, QStringLiteral("kwin_wayland")),
                 SessionActivationScope::PhysicalDesktop);
        QCOMPARE(activationScopeForCompositor({}, physical, QStringLiteral("bash")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, physical), SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, {"bash", "--drm"}, QStringLiteral("kwin_wayland")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor("/usr/bin/bash", physical, QStringLiteral("kwin_wayland")),
                 SessionActivationScope::Private);
        QCOMPARE(activationScopeForCompositor({}, {"kwin_wayland", "--virtual"}, QStringLiteral("kwin_wayland")),
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
