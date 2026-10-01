// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/session_supervisor/src/portal_session_lifetime.h"
#include <QDBusContext>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QtTest>
using namespace QindaQt::SessionSupervisor;
class PortalEndpoint final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Portal1")
public: int calls = 0; QString caller, display; bool accept = true;
public Q_SLOTS:
    Q_SCRIPTABLE bool AttachSessionWithDisplay(const QString &name) {
        if (!calledFromDBus()) return false;
        ++calls; caller = message().service(); display = name; return accept;
    }
};
class PortalLifetimeTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void attachesSelectedDisplayReattachesReplacementAndDisconnectsOnStop() {
        auto first = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("portal-first"));
        PortalEndpoint endpoint;
        QVERIFY(first.registerObject(QStringLiteral("/org/qindaqt/Portal1"), &endpoint, QDBusConnection::ExportScriptableSlots));
        QVERIFY(first.registerService(QStringLiteral("org.qindaqt.Portal1")));
        PortalSessionLifetime session; session.start(QStringLiteral("/bin/true"), QStringLiteral("qindaqt-7"));
        QTRY_COMPARE(endpoint.calls, 1); QCOMPARE(endpoint.display, QStringLiteral("qindaqt-7"));
        const QString sessionOwner = endpoint.caller;
        QVERIFY(sessionOwner.startsWith(QLatin1Char(':'))); QVERIFY(sessionOwner != QDBusConnection::sessionBus().baseService());
        first.unregisterService(QStringLiteral("org.qindaqt.Portal1")); first.unregisterObject(QStringLiteral("/org/qindaqt/Portal1"));
        QDBusConnection::disconnectFromBus(QStringLiteral("portal-first"));
        auto second = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("portal-second"));
        PortalEndpoint replacement;
        QVERIFY(second.registerObject(QStringLiteral("/org/qindaqt/Portal1"), &replacement, QDBusConnection::ExportScriptableSlots));
        QVERIFY(second.registerService(QStringLiteral("org.qindaqt.Portal1")));
        QTRY_COMPARE(replacement.calls, 1); QCOMPARE(replacement.caller, sessionOwner); QCOMPARE(replacement.display, QStringLiteral("qindaqt-7"));
        QVERIFY(first.baseService() != second.baseService());
        session.stop(); QTRY_VERIFY(!second.interface()->isServiceRegistered(sessionOwner).value());
        const int calls = replacement.calls; QTest::qWait(150); QCOMPARE(replacement.calls, calls);
        second.unregisterService(QStringLiteral("org.qindaqt.Portal1")); second.unregisterObject(QStringLiteral("/org/qindaqt/Portal1")); QDBusConnection::disconnectFromBus(QStringLiteral("portal-second"));
    }
    void invalidOrMissingDisplayCannotSelectSession() {
        auto bus = QDBusConnection::sessionBus(); PortalEndpoint endpoint;
        QVERIFY(bus.registerObject(QStringLiteral("/org/qindaqt/Portal1"), &endpoint, QDBusConnection::ExportScriptableSlots));
        QVERIFY(bus.registerService(QStringLiteral("org.qindaqt.Portal1")));
        for (const auto &display : {QString{}, QStringLiteral("wayland-0"), QStringLiteral("qindaqt-07"), QStringLiteral("qindaqt-4096"), QStringLiteral("../qindaqt-7")}) {
            PortalSessionLifetime session; session.start(QStringLiteral("/bin/true"), display); QTest::qWait(120);
        }
        QCOMPARE(endpoint.calls, 0); bus.unregisterService(QStringLiteral("org.qindaqt.Portal1")); bus.unregisterObject(QStringLiteral("/org/qindaqt/Portal1"));
    }
};
QTEST_GUILESS_MAIN(PortalLifetimeTest)
#include "tst_portal_session_lifetime.moc"
