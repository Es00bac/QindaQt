// SPDX-License-Identifier: GPL-3.0-or-later
// LAN mode (ADR-0440) against a real Hub and real TLS: a LAN listener is
// simulated by binding 127.0.0.2 and handing that address to the controller
// through its injected provider. The Avahi side is a FakePublisher.
#include "support/fake_publisher.h"
#include "support/scripted_phone.h"

#include <qindaqt/services/link_service/lan_controller.h>
#include <qindaqt/services/link_service/lan_settings.h>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QTcpServer>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
const QHostAddress kLan(QStringLiteral("127.0.0.2"));

QJsonObject statusOf(const LanController &c) { return QJsonDocument::fromJson(c.statusJson().toUtf8()).object(); }

struct LanFixture {
    explicit LanFixture(bool enabled = true, quint16 port = 0, int windowMs = 120000)
        : store(dir.path()), hub(generateIdentity(QStringLiteral("lan-desktop")), store, hubOptions())
    {
        store.load();
        loopbackUp = hub.listen(QHostAddress::LocalHost, 0);
        if (enabled && !LanSettings(cfg.path()).setLanEnabled(true)) qFatal("cannot seed settings");
        LanOptions o;
        o.configDir = cfg.path();
        o.port = port;
        o.addresses = [this] { return addresses; };
        o.publisher = &publisher;
        o.instanceName = QStringLiteral("test-host");
        o.pairingWindowMs = windowMs;
        o.resyncMs = 50;
        addresses = {kLan};
        lan = std::make_unique<LanController>(hub, o);
    }
    static HubOptions hubOptions()
    {
        HubOptions o;
        o.local = {QStringLiteral("desktop"), QStringLiteral("test"), QStringLiteral("QindaQt"),
                   QStringLiteral("Test OS"), QStringLiteral("0.0.0"), {}, false};
        return o;
    }
    [[nodiscard]] quint16 lanPort() const { return hub.portOf(kLan, Origin::Lan); }
    [[nodiscard]] QJsonObject firstDevice() const { return hub.devices().first().toObject(); }

    QTemporaryDir dir, cfg;
    TrustStore store;
    Hub hub;
    FakePublisher publisher;
    QList<QHostAddress> addresses;
    std::unique_ptr<LanController> lan;
    bool loopbackUp = false;
};
} // namespace

class LinkLanTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void settingDefaultsOffAndPersistsAtomically()
    {
        QTemporaryDir cfg;
        LanSettings s(cfg.path());
        QVERIFY(!s.lanEnabled());  // missing file
        QFile f(s.path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QVERIFY(!s.lanEnabled());  // corrupt file reads as defaults
        QVERIFY(s.setLanEnabled(true));
        QVERIFY(s.lanEnabled());
        // Unknown keys survive a rewrite.
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"lanEnabled":true,"future":42})");
        f.close();
        QVERIFY(s.setLanEnabled(false));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        QCOMPARE(o.value(QStringLiteral("future")).toInt(), 42);
        QVERIFY(!o.value(QStringLiteral("lanEnabled")).toBool());
        QVERIFY(!LanSettings(cfg.path()).lanEnabled());
    }

    void offByDefaultBindsAndPublishesNothing()
    {
        LanFixture f(false);
        QVERIFY(f.loopbackUp);
        QVERIFY(!f.lan->enabled());
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(0));
        QVERIFY(!f.publisher.published);
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("state")).toString(), QStringLiteral("off"));
        QCOMPARE(f.lan->openPairingWindow(), quint32(0));  // no window while LAN is off
    }

    void enablingPersistsListensAndAdvertises()
    {
        LanFixture f(false);
        QVERIFY(f.lan->setEnabled(true));
        QVERIFY(LanSettings(f.cfg.path()).lanEnabled());
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(1));
        QVERIFY(f.lanPort() != 0);
        QVERIFY(f.publisher.published);
        QCOMPARE(f.publisher.advert.type, QStringLiteral("_qindalink._tcp"));
        QCOMPARE(f.publisher.advert.port, f.lanPort());
        QCOMPARE(f.publisher.advert.txt.value(QStringLiteral("proto")), QStringLiteral("1"));
        QCOMPARE(f.publisher.advert.txt.value(QStringLiteral("name")), QStringLiteral("test-host"));
        const QString id = f.publisher.advert.txt.value(QStringLiteral("id"));
        QCOMPARE(id.size(), 16);
        QVERIFY(QString::fromLatin1(f.hub.identity().spkiDigest().toHex()).startsWith(id));
        const QJsonObject st = statusOf(*f.lan);
        QCOMPARE(st.value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
        // Status is open metadata: it must not leak addresses.
        QVERIFY(!f.lan->statusJson().contains(kLan.toString()));
    }

    void settingIsAppliedAtConstruction()
    {
        LanFixture f(true);
        QVERIFY(f.lan->enabled());
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(1));
        QVERIFY(f.publisher.published);
    }

    void lanPairingRefusedWithoutWindowAcceptedWith()
    {
        LanFixture f(true);
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.lanPort(), kLan.toString()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status")});
        Envelope request;
        request.t = QStringLiteral("pair.request");
        request.id = 5;
        phone.send(request);
        Envelope err;
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("denied"));

        QSignalSpy window(f.lan.get(), &LanController::pairingWindowChanged);
        QCOMPARE(f.lan->openPairingWindow(), quint32(120));
        QCOMPARE(window.size(), 1);
        QVERIFY(window.first().at(0).toBool());
        QVERIFY(f.lan->pairingWindowRemaining() > 0);
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        QCOMPARE(code.size(), 6);
    }

    void pairingWindowClosesOnItsOwn()
    {
        LanFixture f(true, 0, 150);
        QSignalSpy window(f.lan.get(), &LanController::pairingWindowChanged);
        QCOMPARE(f.lan->openPairingWindow(), quint32(0));  // 150 ms rounds down to 0 s: a window still opens
        QVERIFY(f.hub.pairingWindowOpen());
        QVERIFY(waitUntil([&] { return window.size() == 2; }));
        QVERIFY(!window.last().at(0).toBool());
        QVERIFY(!f.hub.pairingWindowOpen());
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.lanPort(), kLan.toString()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {});
        Envelope request;
        request.t = QStringLiteral("pair.request");
        request.id = 6;
        phone.send(request);
        Envelope err;
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("denied"));
    }

    void pairedPhoneConnectsOverLanAndLoopbackWins()
    {
        LanFixture f(true);
        ScriptedPhone usb;
        // Pair over loopback (always accepted), as a phone on USB would.
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        QVERIFY(usb.connectTo(f.hub.port()) && usb.waitFor(QStringLiteral("hello")));
        usb.sendHello(false, {QStringLiteral("status")});
        QString code;
        QVERIFY(usb.pairUntilCode(&code));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        f.hub.confirmPairing(usb.deviceId(), true);
        Envelope c;
        QVERIFY(usb.waitFor(QStringLiteral("pair.confirm"), &c));
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        usb.send(accept);
        QVERIFY(waitUntil([&] { return f.store.contains(usb.deviceId()); }));

        // A LAN attempt while USB is live is refused with "replaced".
        ScriptedPhone lan(usb.identity());
        QVERIFY(lan.connectTo(f.lanPort(), kLan.toString()));
        Envelope bye;
        QVERIFY(lan.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("replaced"));
        QVERIFY(usb.isConnected());
        QCOMPARE(f.firstDevice().value(QStringLiteral("transport")).toString(), QStringLiteral("usb"));

        // Once USB drops, the paired phone reconnects over LAN and the snapshot says so.
        usb.sendRaw(QByteArray::fromHex("00000000"));
        QVERIFY(usb.waitClosed());
        QVERIFY(waitUntil([&] { return !f.firstDevice().value(QStringLiteral("connected")).toBool(); }));
        ScriptedPhone lan2(usb.identity());
        QVERIFY(lan2.connectTo(f.lanPort(), kLan.toString()) && lan2.waitFor(QStringLiteral("hello")));
        lan2.sendHello(true, {QStringLiteral("status")});
        QVERIFY(waitUntil([&] { return f.firstDevice().value(QStringLiteral("transport")).toString() == QLatin1String("lan"); }));
    }

    void disablingWithdrawsClosesListenersAndDropsLanSessions()
    {
        LanFixture f(true);
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.lanPort(), kLan.toString()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {});
        const quint16 oldPort = f.lanPort();
        QVERIFY(f.lan->setEnabled(false));
        QVERIFY(!LanSettings(f.cfg.path()).lanEnabled());
        QVERIFY(!f.publisher.published);
        QCOMPARE(f.publisher.withdrawCount, 1);
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(0));
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("shutdown"));
        QVERIFY(phone.waitClosed());
        ScriptedPhone late;
        QVERIFY(!late.connectTo(oldPort, kLan.toString()));  // nothing listens any more
        QCOMPARE(f.hub.listenerCount(Origin::Loopback), qsizetype(1));  // loopback is untouched
    }

    void bindFailureIsReportedAndRetried()
    {
        QTcpServer blocker;
        QVERIFY(blocker.listen(kLan, 0));
        LanFixture f(true, blocker.serverPort());
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(0));
        QJsonObject st = statusOf(*f.lan);
        QCOMPARE(st.value(QStringLiteral("state")).toString(), QStringLiteral("error"));
        QCOMPARE(st.value(QStringLiteral("failed")).toArray().size(), 1);
        QVERIFY(!f.publisher.published);
        QVERIFY(f.hub.port() != 0);  // loopback still serves
        blocker.close();
        QVERIFY(waitUntil([&] { return f.hub.listenerCount(Origin::Lan) == 1; }));
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
        QVERIFY(f.publisher.published);
    }

    void interfaceChangesAreFollowed()
    {
        LanFixture f(true);
        QCOMPARE(f.publisher.publishCount, 1);
        f.addresses.clear();  // the network went away
        QVERIFY(waitUntil([&] { return f.hub.listenerCount(Origin::Lan) == 0; }));
        QVERIFY(!f.publisher.published);
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("state")).toString(), QStringLiteral("waiting"));
        f.addresses = {kLan};
        QVERIFY(waitUntil([&] { return f.hub.listenerCount(Origin::Lan) == 1; }));
        QVERIFY(f.publisher.published);
    }

    void collisionAndDaemonLossAreSurfacedInStatus()
    {
        LanFixture f(true);
        QSignalSpy changed(f.lan.get(), &LanController::statusChanged);
        f.publisher.collide();
        QCOMPARE(changed.size(), 1);
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("publisher")).toString(), QStringLiteral("renamed"));
        f.publisher.daemonGone();
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("publisher")).toString(), QStringLiteral("unavailable"));
        // Listening is independent of mDNS: manual addresses keep working.
        QCOMPARE(statusOf(*f.lan).value(QStringLiteral("state")).toString(), QStringLiteral("listening"));
    }

    void controllerDestructionWithdrawsTheAdvert()
    {
        LanFixture f(true);
        QVERIFY(f.publisher.published);
        f.lan.reset();
        QVERIFY(!f.publisher.published);
    }
};

QTEST_GUILESS_MAIN(LinkLanTest)
#include "tst_link_lan.moc"
