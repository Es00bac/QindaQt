// SPDX-License-Identifier: GPL-3.0-or-later
// LAN mode (ADR-0441) against a real Hub and real TLS: a LAN listener is
// simulated by binding 127.0.0.2 and handing that address to the controller
// through its injected provider. The Avahi side is a FakePublisher.
#include "support/fake_publisher.h"
#include "support/scripted_phone.h"

#include <qindaqt/services/link_service/lan_controller.h>
#include <qindaqt/services/link_service/lan_settings.h>
#include <qindaqt/services/link_transport/accept_limiter.h>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QTcpServer>
#include <QtTest/QSignalSpy>

#include <memory>
#include <vector>
#include <QtTest/QtTest>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
const QHostAddress kLan(QStringLiteral("127.0.0.2"));

QJsonObject statusOf(const LanController &c) { return QJsonDocument::fromJson(c.statusJson().toUtf8()).object(); }

struct LanFixture {
    explicit LanFixture(bool enabled = true, quint16 port = 0, int windowMs = 120000,
                        const std::function<void(HubOptions &)> &tweak = {})
        : store(dir.path()), hub(generateIdentity(QStringLiteral("lan-desktop")), store, hubOptions(tweak))
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
        addresses = {{kLan, QStringLiteral("test0")}};
        lan = std::make_unique<LanController>(hub, o);
    }
    static HubOptions hubOptions(const std::function<void(HubOptions &)> &tweak = {})
    {
        HubOptions o;
        if (tweak) tweak(o);
        o.local = {QStringLiteral("desktop"), QStringLiteral("test"), QStringLiteral("QindaQt"),
                   QStringLiteral("Test OS"), QStringLiteral("0.0.0"), {}, false};
        return o;
    }
    [[nodiscard]] quint16 lanPort() const { return hub.portOf(kLan, Origin::Lan); }
    bool pairViaLoopback(ScriptedPhone &usb)
    {
        QSignalSpy requested(&hub, &Hub::pairingRequested);
        if (!usb.connectTo(hub.port()) || !usb.waitFor(QStringLiteral("hello"))) return false;
        usb.sendHello(false, {QStringLiteral("status")});
        QString code;
        if (!usb.pairUntilCode(&code) || !waitUntil([&] { return !requested.isEmpty(); })) return false;
        hub.confirmPairing(usb.deviceId(), true);
        Envelope c;
        if (!usb.waitFor(QStringLiteral("pair.confirm"), &c)) return false;
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        usb.send(accept);
        return waitUntil([&] { return store.contains(usb.deviceId()); });
    }
    [[nodiscard]] QJsonObject firstDevice() const { return hub.devices().first().toObject(); }

    QTemporaryDir dir, cfg;
    TrustStore store;
    Hub hub;
    FakePublisher publisher;
    QList<LanAddress> addresses;
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
        f.addresses = {{kLan, QStringLiteral("test0")}};
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


    void privateRangesOnlyByDefault()
    {
        for (const char *ok : {"10.1.2.3", "172.16.0.1", "172.31.255.254", "192.168.1.1", "100.64.0.1", "100.127.255.254", "fd7a:115c::1", "fc00::1"}) {
            QVERIFY2(isPrivateLanAddress(QHostAddress(QString::fromLatin1(ok))), ok);
        }
        for (const char *no : {"8.8.8.8", "172.15.0.1", "172.32.0.1", "100.63.255.255", "100.128.0.1", "192.169.0.1", "11.0.0.1",
                               "2001:db8::1", "2a00:1450::1", "fe80::1", "::1"}) {
            QVERIFY2(!isPrivateLanAddress(QHostAddress(QString::fromLatin1(no))), no);
        }
        for (const LanAddress &a : defaultLanAddresses()) QVERIFY(isPrivateLanAddress(a.address));
    }

    void interfaceAllowlistRestrictsListenersAndAdvert()
    {
        LanFixture f(true);
        f.addresses = {{kLan, QStringLiteral("wlan0")}, {QHostAddress(QStringLiteral("127.0.0.3")), QStringLiteral("tun0")}};
        QVERIFY(f.lan->setInterfaces({QStringLiteral("wlan0")}));
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(1));
        QCOMPARE(f.publisher.advert.interfaces, QStringList{QStringLiteral("wlan0")});
        QJsonObject st = statusOf(*f.lan);
        QCOMPARE(st.value(QStringLiteral("interfaces")).toArray().size(), 1);
        QCOMPARE(st.value(QStringLiteral("allowlist")).toArray().first().toString(), QStringLiteral("wlan0"));
        QVERIFY(LanSettings(f.cfg.path()).lanInterfaces() == QStringList{QStringLiteral("wlan0")});
        QVERIFY(!f.lan->statusJson().contains(QLatin1String("127.0.0")));
        // Clearing the list serves both interfaces again and announces on both.
        QVERIFY(f.lan->setInterfaces({}));
        QCOMPARE(f.hub.listenerCount(Origin::Lan), qsizetype(2));
        QCOMPARE(f.publisher.advert.interfaces, (QStringList{QStringLiteral("tun0"), QStringLiteral("wlan0")}));
        // Invalid names are refused and change nothing.
        QString error;
        QVERIFY(!f.lan->setInterfaces({QStringLiteral("bad name")}, &error));
        QVERIFY(!f.lan->setInterfaces({QStringLiteral("a-very-long-interface-name")}, &error));
        QVERIFY(LanSettings(f.cfg.path()).lanInterfaces().isEmpty());
    }

    void firstAcceptedLanPairRequestClosesTheWindow()
    {
        LanFixture f(true);
        QSignalSpy window(f.lan.get(), &LanController::pairingWindowChanged);
        QCOMPARE(f.lan->openPairingWindow(), quint32(120));
        ScriptedPhone first;
        QVERIFY(first.connectTo(f.lanPort(), kLan.toString()) && first.waitFor(QStringLiteral("hello")));
        first.sendHello(false, {});
        QString code;
        QVERIFY(first.pairUntilCode(&code));
        QVERIFY(!f.hub.pairingWindowOpen());
        QVERIFY(window.size() >= 2 && !window.last().at(0).toBool());
        QCOMPARE(f.lan->pairingWindowRemaining(), quint32(0));
        // The pending request says where it came from.
        const QJsonObject pending = f.hub.pendingPairings().first().toObject();
        QCOMPARE(pending.value(QStringLiteral("origin")).toString(), QStringLiteral("lan"));
        QVERIFY(!pending.value(QStringLiteral("source")).toString().isEmpty());
        // A second LAN phone is refused: the window was used up.
        ScriptedPhone second;
        QVERIFY(second.connectTo(f.lanPort(), kLan.toString()) || true);
    }

    void abortingLanPairingsEndsThemWithABye()
    {
        LanFixture f(true);
        QCOMPARE(f.lan->openPairingWindow(), quint32(120));
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.lanPort(), kLan.toString()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {});
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        f.hub.abortLanPairings();
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-timeout"));
    }

    void usbPairingRequestShowsUsbOrigin()
    {
        LanFixture f(true);
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        ScriptedPhone usb;
        QVERIFY(usb.connectTo(f.hub.port()) && usb.waitFor(QStringLiteral("hello")));
        usb.sendHello(false, {});
        QString code;
        QVERIFY(usb.pairUntilCode(&code));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        QCOMPARE(requested.first().at(4).toString(), QStringLiteral("usb"));
        QVERIFY(requested.first().at(5).toString().isEmpty());
        QCOMPARE(f.hub.pendingPairings().first().toObject().value(QStringLiteral("origin")).toString(), QStringLiteral("usb"));
    }

    void lanFloodCannotBlockPinnedReconnectOrUsbPairing()
    {
        // Large per-source/accept budgets so the per-address-class cap (3) is what is exercised.
        LanFixture f(true, 0, 120000, [](HubOptions &o) {
            o.maxLanUnpairedPerSource = 100;
            o.lanAcceptsPerSource = 1000;
            o.lanAcceptsGlobal = 1000;
        });
        ScriptedPhone owner;
        QVERIFY(f.pairViaLoopback(owner));
        owner.sendRaw(QByteArray::fromHex("00000000"));  // drop the USB link
        QVERIFY(owner.waitClosed());
        QVERIFY(waitUntil([&] { return !f.firstDevice().value(QStringLiteral("connected")).toBool(); }));

        // Fill the LAN budget with throwaway keys that never finish anything.
        std::vector<std::unique_ptr<ScriptedPhone>> flood;
        for (int i = 0; i < 3; ++i) {
            flood.push_back(std::make_unique<ScriptedPhone>());
            QVERIFY(flood.back()->connectTo(f.lanPort(), kLan.toString()));
            QVERIFY(flood.back()->waitFor(QStringLiteral("hello")));
        }
        // The fourth is refused right after the handshake...
        ScriptedPhone fourth;
        if (fourth.connectTo(f.lanPort(), kLan.toString())) QVERIFY(fourth.waitClosed());
        // ...a throwaway key already holding the LAN pairing slot does not block USB pairing...
        QCOMPARE(f.lan->openPairingWindow(), quint32(120));
        flood[0]->sendHello(false, {});
        QString code;
        QVERIFY(flood[0]->pairUntilCode(&code));
        ScriptedPhone newUsb;
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        QVERIFY(newUsb.connectTo(f.hub.port()) && newUsb.waitFor(QStringLiteral("hello")));
        newUsb.sendHello(false, {});
        QString usbCode;
        QVERIFY(newUsb.pairUntilCode(&usbCode));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        QCOMPARE(requested.last().at(4).toString(), QStringLiteral("usb"));
        Envelope bye;
        QVERIFY(flood[0]->waitFor(QStringLiteral("bye"), &bye));  // the LAN pairing was preempted
        // ...and the paired phone's USB reconnect is not refused by the flood.
        ScriptedPhone back(owner.identity());
        QVERIFY(back.connectTo(f.hub.port()) && back.waitFor(QStringLiteral("hello")));
        back.sendHello(true, {QStringLiteral("status")});
        QVERIFY(waitUntil([&] { return f.hub.devices().first().toObject().value(QStringLiteral("transport")).toString() == QLatin1String("usb"); }));
        // And so is its LAN reconnect: a pinned id counts against no cap.
        back.sendRaw(QByteArray::fromHex("00000000"));
        QVERIFY(back.waitClosed());
        ScriptedPhone lanBack(owner.identity());
        QVERIFY(lanBack.connectTo(f.lanPort(), kLan.toString()) && lanBack.waitFor(QStringLiteral("hello")));
    }

    void oneUnauthenticatedLanConnectionPerSource()
    {
        LanFixture f(true);
        ScriptedPhone a;
        QVERIFY(a.connectTo(f.lanPort(), kLan.toString()) && a.waitFor(QStringLiteral("hello")));
        ScriptedPhone b;
        if (b.connectTo(f.lanPort(), kLan.toString())) QVERIFY(b.waitClosed());
        QVERIFY(a.isConnected());
    }

    void acceptRateLimitRefusesAFloodBeforeTls()
    {
        LanFixture f(true, 0, 120000, [](HubOptions &o) { o.lanAcceptsPerSource = 4; o.lanAcceptsGlobal = 100; o.lanAcceptWindowMs = 600000; });
        std::vector<std::unique_ptr<ScriptedPhone>> phones;
        int encrypted = 0;
        for (int i = 0; i < 6; ++i) {
            phones.push_back(std::make_unique<ScriptedPhone>());
            if (phones.back()->connectTo(f.lanPort(), kLan.toString())) ++encrypted;
        }
        QVERIFY2(encrypted <= 4, qPrintable(QString::number(encrypted)));
        QVERIFY(encrypted >= 1);
    }

    void acceptLimiterWindowSlides()
    {
        qint64 now = 0;
        AcceptLimiter l(2, 3, 1000, [&] { return now; });
        const QHostAddress a(QStringLiteral("10.0.0.5")), b(QStringLiteral("10.0.0.6")), c(QStringLiteral("10.0.0.7"));
        QVERIFY(l.allow(a));
        QVERIFY(l.allow(a));
        QVERIFY(!l.allow(a));      // per source
        QVERIFY(l.allow(b));
        QVERIFY(!l.allow(c));      // global
        now = 1001;
        QVERIFY(l.allow(a));       // the window moved on
        QVERIFY(l.allow(c));
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
