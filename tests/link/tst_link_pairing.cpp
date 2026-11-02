// SPDX-License-Identifier: GPL-3.0-or-later
// End-to-end desktop pairing against a scripted phone over real TLS 1.3 on an
// ephemeral loopback port.
#include "support/hub_fixture.h"

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

class LinkPairingTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void success()
    {
        HubFixture f;
        QVERIFY(f.listening);
        ScriptedPhone phone;
        QVERIFY(f.pair(phone, {QStringLiteral("status"), QStringLiteral("ring")}));
        const QJsonArray devices = f.hub.devices();
        QCOMPARE(devices.size(), 1);
        const QJsonObject d = devices.first().toObject();
        QCOMPARE(d.value(QStringLiteral("id")).toString(), phone.deviceId());
        QVERIFY(d.value(QStringLiteral("paired")).toBool());
        QVERIFY(d.value(QStringLiteral("connected")).toBool());
        QCOMPARE(d.value(QStringLiteral("transport")).toString(), QStringLiteral("usb"));
        // The same connection is now READY: status flows and ring round-trips.
        Envelope status;
        status.t = QStringLiteral("status");
        status.b = QJsonObject{{QStringLiteral("battery"), QJsonObject{{QStringLiteral("level"), 87},
                                  {QStringLiteral("charging"), true}, {QStringLiteral("plugged"), QStringLiteral("usb")}}},
                               {QStringLiteral("network"), QJsonObject{{QStringLiteral("type"), QStringLiteral("wifi")},
                                  {QStringLiteral("signal"), QJsonValue::Null}}}};
        phone.send(status);
        QVERIFY(waitUntil([&] {
            return f.hub.devices().first().toObject().value(QStringLiteral("battery")).toObject()
                       .value(QStringLiteral("level")).toInt() == 87;
        }));
        bool ringOk = false, ringDone = false;
        f.hub.ring(phone.deviceId(), [&](bool ok, const QString &) { ringOk = ok; ringDone = true; });
        Envelope ring;
        QVERIFY(phone.waitFor(QStringLiteral("ring"), &ring));
        Envelope ok;
        ok.t = QStringLiteral("ok");
        ok.re = ring.id;
        phone.send(ok);
        QVERIFY(waitUntil([&] { return ringDone; }));
        QVERIFY(ringOk);
    }

    void tamperedRevealIsRejectedByThePhone()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()));
        QVERIFY(phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        QString code;
        QVERIFY(!phone.pairUntilCode(&code, nullptr, true));  // sends bye pair-rejected
        QVERIFY(phone.waitClosed());
        QVERIFY(!f.store.contains(phone.deviceId()));
    }

    void rejectFromDesktopSendsByeAndPinsNothing()
    {
        HubFixture f;
        ScriptedPhone phone;
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        f.hub.confirmPairing(phone.deviceId(), false);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-rejected"));
        QVERIFY(phone.waitClosed());
        QVERIFY(!f.store.contains(phone.deviceId()));
    }

    void rejectFromPhone()
    {
        HubFixture f;
        ScriptedPhone phone;
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        Envelope no;
        no.t = QStringLiteral("pair.confirm");
        no.b = QJsonObject{{QStringLiteral("accept"), false}};
        phone.send(no);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-rejected"));
        QVERIFY(phone.waitClosed());
        QVERIFY(!f.store.contains(phone.deviceId()));
    }

    void timeout()
    {
        SessionTimings t;
        t.pairTimeoutMs = 200;
        HubFixture f(t);
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        Envelope request;
        request.t = QStringLiteral("pair.request");
        request.id = 5;
        phone.send(request);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-timeout"));
        QVERIFY(phone.waitClosed());
    }

    void featureFrameWhileUnpairedClosesTheConnection()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status")});
        Envelope status;
        status.t = QStringLiteral("status");
        status.b = QJsonObject{{QStringLiteral("battery"), QJsonObject{{QStringLiteral("level"), 1},
                                  {QStringLiteral("charging"), false}, {QStringLiteral("plugged"), QStringLiteral("none")}}}};
        phone.send(status);
        QVERIFY(phone.waitClosed());
        QCOMPARE(phone.count(QStringLiteral("bye")), 0);
    }

    void malformedFrameClosesWithoutReply()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendRaw(QByteArray::fromHex("00000000"));
        QVERIFY(phone.waitClosed());
    }

    void protocolVersionMismatch()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        Envelope hello;
        hello.t = QStringLiteral("hello");
        hello.b = QJsonObject{{QStringLiteral("proto"), 2}, {QStringLiteral("role"), QStringLiteral("phone")},
                              {QStringLiteral("name"), QStringLiteral("x")}, {QStringLiteral("model"), QStringLiteral("x")},
                              {QStringLiteral("os"), QStringLiteral("x")}, {QStringLiteral("app"), QStringLiteral("x")},
                              {QStringLiteral("caps"), QJsonArray()}, {QStringLiteral("trusted"), false}};
        phone.send(hello);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("protocol-version"));
    }

    void stalePinStaysUnpairedAndKeepsThePin()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(f.pair(phone));
        phone.setAutoPong(true);
        // The phone "forgot" the desktop: reconnect with trusted=false.
        ScriptedPhone again(phone.identity());
        QVERIFY(again.connectTo(f.port()) && again.waitFor(QStringLiteral("hello")));
        again.sendHello(false, {QStringLiteral("status")});
        QVERIFY(waitUntil([&] {
            const auto d = f.hub.devices().first().toObject();
            return d.value(QStringLiteral("connected")).toBool();
        }));
        const auto d = f.hub.devices().first().toObject();
        QVERIFY(!d.value(QStringLiteral("ready")).toBool());
        QVERIFY(f.store.contains(phone.deviceId()));  // pin kept
        // Pairing again replaces the pin on the same connection.
        QString code;
        QVERIFY(again.pairUntilCode(&code));
    }

    void busyAndLanWindow()
    {
        HubFixture f;
        QVERIFY(f.hub.listen(QHostAddress::LocalHost, 0, Origin::Lan));
        // Busy: a second pair.request while one is pending.
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        Envelope again;
        again.t = QStringLiteral("pair.request");
        again.id = 78;
        phone.send(again);
        Envelope err;
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.re, 78);
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("busy"));
        // LAN origin without a pairing window is denied.
        ScriptedPhone lan;
        QVERIFY(lan.connectTo(f.hub.port(1)) && lan.waitFor(QStringLiteral("hello")));
        lan.sendHello(false);
        Envelope req;
        req.t = QStringLiteral("pair.request");
        req.id = 9;
        lan.send(req);
        QVERIFY(lan.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("denied"));
        // With the window open the request proceeds.
        f.hub.setPairingWindowOpen(true);
        req.id = 10;
        lan.send(req);
        QVERIFY(lan.waitFor(QStringLiteral("pair.commit")));
    }
};

QTEST_GUILESS_MAIN(LinkPairingTest)
#include "tst_link_pairing.moc"
