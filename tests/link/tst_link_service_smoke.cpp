// SPDX-License-Identifier: GPL-3.0-or-later
// Starts the real qindaqt-link-service on the private bus provided by
// dbus-run-session, with a temporary data dir, an ephemeral port, no ADB and
// no notifications, then pairs a scripted phone through the typed client.
#include "support/scripted_phone.h"

#include <qindaqt/services/link_client/link_client.h>
#include <qindaqt/services/link_client/qt_link_transport.h>

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtTest/QSignalSpy>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

class LinkServiceSmokeTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void pairsAndReportsStatusOverTheBus()
    {
        QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
        QTemporaryDir data;
        QProcess service;
        service.setProcessChannelMode(QProcess::ForwardedChannels);
        service.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY),
                      {QStringLiteral("--data-dir"), data.path(), QStringLiteral("--port"), QStringLiteral("0"),
                       QStringLiteral("--no-adb"), QStringLiteral("--no-notifications"),
                       QStringLiteral("--trusted-caller"), QCoreApplication::applicationFilePath()});
        QVERIFY(service.waitForStarted());

        QDBusConnection bus = QDBusConnection::sessionBus();
        QtLinkTransport transport(bus);
        LinkClient client(transport);
        QSignalSpy requested(&client, &LinkClient::pairingRequested);
        QVERIFY(client.start());
        QVERIFY(waitUntil([&] { return client.available(); }, 10000));

        QDBusInterface iface(QStringLiteral("org.qindaqt.Link1"), QStringLiteral("/org/qindaqt/Link1"),
                             QStringLiteral("org.qindaqt.Link1"), bus);
        const QDBusReply<quint16> port = iface.call(QStringLiteral("ListenPort"));
        QVERIFY(port.isValid());
        QVERIFY(port.value() != 47160 && port.value() != 0);

        ScriptedPhone phone;
        QVERIFY(phone.connectTo(port.value()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status"), QStringLiteral("ring")});
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        QCOMPARE(requested.first().at(3).toString(), code);
        QCOMPARE(requested.first().at(0).toString(), phone.deviceId());

        client.confirmPairing(phone.deviceId(), true);
        Envelope confirm;
        QVERIFY(phone.waitFor(QStringLiteral("pair.confirm"), &confirm));
        QVERIFY(confirm.b.value(QStringLiteral("accept")).toBool());
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        phone.send(accept);
        QVERIFY(waitUntil([&] {
            return !client.devices().isEmpty() && client.devices().first().paired && client.devices().first().ready;
        }));

        Envelope status;
        status.t = QStringLiteral("status");
        status.b = QJsonObject{{QStringLiteral("battery"), QJsonObject{{QStringLiteral("level"), 64},
                                  {QStringLiteral("charging"), false}, {QStringLiteral("plugged"), QStringLiteral("none")}}}};
        phone.send(status);
        QVERIFY(waitUntil([&] { return client.devices().first().batteryLevel == 64; }));

        client.ring(phone.deviceId());
        QVERIFY(phone.waitFor(QStringLiteral("ring")));

        // The trust store reached disk, owner-only.
        const QFileInfo devices(data.filePath(QStringLiteral("devices.json")));
        QVERIFY(devices.exists());
        QVERIFY(!(devices.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));

        client.unpair(phone.deviceId());
        QVERIFY(phone.waitFor(QStringLiteral("bye")));
        QVERIFY(waitUntil([&] { return client.devices().isEmpty(); }));

        service.terminate();  // our own child, by handle
        QVERIFY(service.waitForFinished(5000));
    }
};

QTEST_GUILESS_MAIN(LinkServiceSmokeTest)
#include "tst_link_service_smoke.moc"
