// SPDX-License-Identifier: GPL-3.0-or-later
// The real qindaqt-link-service on a private bus: LAN is off by default, the
// protected members obey CallerAuthority (ADR-0438), and an enabled LAN
// listener (bound to 127.0.0.2 through --lan-test-address) honours the pairing
// window. Never touches the user's service, config or firewall.
#include "support/scripted_phone.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtNetwork/QTcpServer>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
QVariant linkProp(QDBusConnection &bus, const char *name)
{
    QDBusInterface props(QStringLiteral("org.qindaqt.Link1"), QStringLiteral("/org/qindaqt/Link1"),
                         QStringLiteral("org.freedesktop.DBus.Properties"), bus);
    const QDBusReply<QDBusVariant> r = props.call(QStringLiteral("Get"), QStringLiteral("org.qindaqt.Link1"),
                                                  QString::fromLatin1(name));
    return r.isValid() ? r.value().variant() : QVariant();
}
}

class LinkLanServiceTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void lanModeEndToEnd()
    {
        QVERIFY2(!qEnvironmentVariableIsEmpty("DBUS_SESSION_BUS_ADDRESS"), "run under dbus-run-session");
        QTemporaryDir data, cfg, cache;
        quint16 lanPort = 0;
        {
            QTcpServer probe;  // reserve a free port on the simulated LAN address, then release it
            QVERIFY(probe.listen(QHostAddress(QStringLiteral("127.0.0.2")), 0));
            lanPort = probe.serverPort();
        }
        QProcess service;
        service.setProcessChannelMode(QProcess::ForwardedChannels);
        service.start(QStringLiteral(QINDAQT_LINK_SERVICE_BINARY),
                      {QStringLiteral("--data-dir"), data.path(), QStringLiteral("--config-dir"), cfg.path(),
                       QStringLiteral("--cache-dir"), cache.path(), QStringLiteral("--port"), QStringLiteral("0"),
                       QStringLiteral("--lan-port"), QString::number(lanPort), QStringLiteral("--lan-test-address"),
                       QStringLiteral("127.0.0.2"), QStringLiteral("--pairing-window-ms"), QStringLiteral("5000"),
                       QStringLiteral("--no-adb"), QStringLiteral("--no-avahi"), QStringLiteral("--no-notifications"),
                       QStringLiteral("--no-input"), QStringLiteral("--trusted-caller"),
                       QCoreApplication::applicationFilePath()});
        QVERIFY(service.waitForStarted());
        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusInterface iface(QStringLiteral("org.qindaqt.Link1"), QStringLiteral("/org/qindaqt/Link1"),
                             QStringLiteral("org.qindaqt.Link1"), bus);
        QVERIFY(waitUntil([&] { return linkProp(bus, "LanEnabled").isValid(); }, 10000));

        // Off by default, with no LAN listener.
        QCOMPARE(linkProp(bus, "LanEnabled").toBool(), false);
        QCOMPARE(QJsonDocument::fromJson(linkProp(bus, "LanStatus").toString().toUtf8()).object().value(QStringLiteral("state")).toString(),
                 QStringLiteral("off"));
        ScriptedPhone refused;
        QVERIFY(!refused.connectTo(lanPort, QStringLiteral("127.0.0.2")));
        QCOMPARE(QDBusReply<quint32>(iface.call(QStringLiteral("OpenPairingWindow"))).value(), quint32(0));

        // An untrusted caller (gdbus is not on the trusted list) can neither enable LAN nor read it as denied.
        QProcess gdbus;
        gdbus.setProcessChannelMode(QProcess::MergedChannels);
        gdbus.start(QStringLiteral("/usr/bin/gdbus"),
                    {QStringLiteral("call"), QStringLiteral("--session"), QStringLiteral("--dest"), QStringLiteral("org.qindaqt.Link1"),
                     QStringLiteral("--object-path"), QStringLiteral("/org/qindaqt/Link1"), QStringLiteral("--method"),
                     QStringLiteral("org.qindaqt.Link1.SetLanEnabled"), QStringLiteral("true")});
        QVERIFY(gdbus.waitForFinished(15000));
        QVERIFY(QString::fromUtf8(gdbus.readAll()).contains(QLatin1String("org.qindaqt.Link1.Error.Denied")));
        QCOMPARE(linkProp(bus, "LanEnabled").toBool(), false);
        QVERIFY(!QFile::exists(cfg.filePath(QStringLiteral("service.json"))));

        // The trusted test binary enables it; the setting persists and the listener comes up.
        iface.call(QStringLiteral("SetLanEnabled"), true);
        QVERIFY(waitUntil([&] { return linkProp(bus, "LanEnabled").toBool(); }));
        QVERIFY(QFile::exists(cfg.filePath(QStringLiteral("service.json"))));
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(lanPort, QStringLiteral("127.0.0.2")) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status")});
        Envelope request;
        request.t = QStringLiteral("pair.request");
        request.id = 9;
        phone.send(request);
        Envelope err;
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("denied"));

        QCOMPARE(QDBusReply<quint32>(iface.call(QStringLiteral("OpenPairingWindow"))).value(), quint32(5));
        QString code;
        QVERIFY(phone.pairUntilCode(&code));

        iface.call(QStringLiteral("SetLanEnabled"), false);
        QVERIFY(phone.waitClosed());
        QCOMPARE(linkProp(bus, "LanEnabled").toBool(), false);
        service.terminate();  // our own child, by handle
        QVERIFY(service.waitForFinished(5000));
    }
};

QTEST_GUILESS_MAIN(LinkLanServiceTest)
#include "tst_link_lan_service.moc"
