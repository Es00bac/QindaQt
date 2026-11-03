// SPDX-License-Identifier: GPL-3.0-or-later
// FdoPhoneNotificationPresenter against an in-process fake org.freedesktop.Notifications on the
// (private, dbus-run-session) session bus. Never talks to the user's notification server.
#include <qindaqt/services/link_notifications/fdo_phone_presenter.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QScopeGuard>
#include <QtDBus/QDBusConnection>
#include <QtTest/QtTest>

using namespace QindaQt::Link;

class FakeServer : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    QStringList caps;
    QStringList bodies;
    uint next = 0;
public Q_SLOTS:
    QStringList GetCapabilities() { return caps; }
    uint Notify(const QString &, uint, const QString &, const QString &, const QString &body, const QStringList &,
                const QVariantMap &, int)
    {
        bodies << body;
        return ++next;
    }
    void CloseNotification(uint) {}
};

class FdoPresenterTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void escapesPhoneTextOnlyWhenTheServerRendersMarkup_data()
    {
        QTest::addColumn<QStringList>("caps");
        QTest::addColumn<QString>("expected");
        QTest::newRow("markup") << QStringList{"body", "body-markup"} << QStringLiteral("&lt;a href=x&gt;hi&lt;/a&gt; &amp; co");
        QTest::newRow("plain") << QStringList{"body", "actions"} << QStringLiteral("<a href=x>hi</a> & co");
    }
    void escapesPhoneTextOnlyWhenTheServerRendersMarkup()
    {
        QFETCH(QStringList, caps);
        QFETCH(QString, expected);
        const QString address = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        QVERIFY2(!address.isEmpty(), "run under dbus-run-session");
        QDBusConnection serverBus = QDBusConnection::connectToBus(address, QStringLiteral("server-%1").arg(QTest::currentDataTag()));
        QDBusConnection clientBus = QDBusConnection::connectToBus(address, QStringLiteral("client-%1").arg(QTest::currentDataTag()));
        FakeServer server;
        const auto release = qScopeGuard([&] {
            serverBus.unregisterObject(QStringLiteral("/org/freedesktop/Notifications"));
            serverBus.unregisterService(QStringLiteral("org.freedesktop.Notifications"));
        });
        server.caps = caps;
        QVERIFY(serverBus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), &server, QDBusConnection::ExportAllSlots));
        QVERIFY(serverBus.registerService(QStringLiteral("org.freedesktop.Notifications")));
        FdoPhoneNotificationPresenter presenter(clientBus);
        QTRY_COMPARE_WITH_TIMEOUT(presenter.serverRendersMarkup(), caps.contains("body-markup"), 5000);
        PresentedNotification n;
        n.appName = QStringLiteral("App (phone)");
        n.summary = QStringLiteral("s");
        n.body = QStringLiteral("<a href=x>hi</a> & co");
        QVERIFY(presenter.present(n, 0) != 0);
        QTRY_COMPARE_WITH_TIMEOUT(server.bodies.size(), 1, 5000);
        QCOMPARE(server.bodies.first(), expected);
    }
    void escapeIsAppliedBeforeCapabilitiesAreKnown()
    {
        QCOMPARE(FdoPhoneNotificationPresenter::escapeMarkup(QStringLiteral("<img src=x>")), QStringLiteral("&lt;img src=x&gt;"));
    }
};

QTEST_GUILESS_MAIN(FdoPresenterTest)
#include "tst_link_fdo_presenter.moc"
