// SPDX-License-Identifier: GPL-3.0-or-later
// Session behavior beyond pairing: replacement, LAN rule, keepalive with
// injected timings, unpair, status validation and log redaction.
#include "support/hub_fixture.h"

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

namespace {
Envelope statusFrame(int level)
{
    Envelope e;
    e.t = QStringLiteral("status");
    e.b = QJsonObject{{QStringLiteral("battery"), QJsonObject{{QStringLiteral("level"), level},
                          {QStringLiteral("charging"), false}, {QStringLiteral("plugged"), QStringLiteral("none")}}}};
    return e;
}
QJsonObject firstDevice(HubFixture &f) { return f.hub.devices().first().toObject(); }
}

class LinkSessionTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void reconnectWithPinIsReadyAndReplacesTheOldConnection()
    {
        HubFixture f;
        ScriptedPhone first;
        QVERIFY(f.pair(first));
        ScriptedPhone second(first.identity());
        QVERIFY(second.connectTo(f.port()) && second.waitFor(QStringLiteral("hello")));
        second.sendHello(true, {QStringLiteral("status")});
        Envelope bye;
        QVERIFY(first.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("replaced"));
        QVERIFY(first.waitClosed());
        second.send(statusFrame(42));
        QVERIFY(waitUntil([&] {
            return firstDevice(f).value(QStringLiteral("battery")).toObject().value(QStringLiteral("level")).toInt() == 42;
        }));
        QVERIFY(firstDevice(f).value(QStringLiteral("ready")).toBool());
    }

    void lanNeverReplacesLiveLoopback()
    {
        HubFixture f;
        QVERIFY(f.hub.listen(QHostAddress::LocalHost, 0, Origin::Lan));
        ScriptedPhone usb;
        QVERIFY(f.pair(usb));
        ScriptedPhone lan(usb.identity());
        QVERIFY(lan.connectTo(f.hub.port(1)));
        Envelope bye;
        QVERIFY(lan.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("replaced"));
        QVERIFY(lan.waitClosed());
        QVERIFY(usb.isConnected());
        QCOMPARE(firstDevice(f).value(QStringLiteral("transport")).toString(), QStringLiteral("usb"));
    }

    void lanConnectsWhenNoLoopbackIsLive()
    {
        HubFixture f;
        QVERIFY(f.hub.listen(QHostAddress::LocalHost, 0, Origin::Lan));
        ScriptedPhone usb;
        QVERIFY(f.pair(usb));
        ScriptedPhone lan(usb.identity());
        // Drop the loopback connection first.
        usb.sendRaw(QByteArray::fromHex("00000000"));
        QVERIFY(usb.waitClosed());
        QVERIFY(waitUntil([&] { return !firstDevice(f).value(QStringLiteral("connected")).toBool(); }));
        QVERIFY(lan.connectTo(f.hub.port(1)) && lan.waitFor(QStringLiteral("hello")));
        lan.sendHello(true, {QStringLiteral("status")});
        QVERIFY(waitUntil([&] { return firstDevice(f).value(QStringLiteral("transport")).toString() == QLatin1String("lan"); }));
    }

    void keepaliveDeathAndSurvival()
    {
        SessionTimings t;
        t.pingIdleMs = 100;
        t.deadMs = 500;
        {
            HubFixture f(t);
            ScriptedPhone silent;
            silent.setAutoPong(false);
            QVERIFY(silent.connectTo(f.port()) && silent.waitFor(QStringLiteral("hello")));
            silent.sendHello(false);
            QVERIFY(silent.waitFor(QStringLiteral("ping")));   // after ~100 ms idle
            QVERIFY(silent.waitClosed(3000));                  // no traffic for 500 ms
        }
        {
            HubFixture f(t);
            ScriptedPhone alive;  // answers pings
            QVERIFY(alive.connectTo(f.port()) && alive.waitFor(QStringLiteral("hello")));
            alive.sendHello(false);
            QTest::qWait(1500);  // three dead-intervals
            QVERIFY(alive.isConnected());
            QVERIFY(alive.count(QStringLiteral("ping")) >= 3);
        }
    }

    void unpairSendsByeAndForgetsTheKey()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(f.pair(phone));
        f.hub.unpair(phone.deviceId());
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("unpaired"));
        QVERIFY(!f.store.contains(phone.deviceId()));
        QVERIFY(f.hub.devices().isEmpty());
    }

    void readyRepliesToUnknownRequestsAndBadStatus()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(f.pair(phone));
        Envelope unknown;
        unknown.t = QStringLiteral("zzz.nothing");
        unknown.id = 9;
        phone.send(unknown);
        Envelope err;
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.re, 9);
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("unknown-type"));
        Envelope bad = statusFrame(300);
        bad.id = 10;
        phone.send(bad);
        QVERIFY(phone.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("bad-request"));
        Envelope ping;
        ping.t = QStringLiteral("ping");
        ping.id = 11;
        phone.send(ping);
        Envelope pong;
        QVERIFY(phone.waitFor(QStringLiteral("pong"), &pong));
        QCOMPARE(pong.re, 11);
    }

    void ringWithoutCapabilityIsUnsupported()
    {
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(f.pair(phone, {QStringLiteral("status")}));
        QString code;
        bool done = false;
        f.hub.ring(phone.deviceId(), [&](bool, const QString &c) { code = c; done = true; });
        QVERIFY(done);
        QCOMPARE(code, QStringLiteral("unsupported"));
    }

    void vaultBodiesAreRedactedInLogs()
    {
        Envelope e;
        e.t = QStringLiteral("vault.type");
        e.b = QJsonObject{{QStringLiteral("steps"), QStringLiteral("hunter2-secret")}};
        const QString line = frameLogLine(e);
        QVERIFY(!line.contains(QLatin1String("hunter2")));
        QVERIFY(!line.contains(QLatin1String("steps")));
        QVERIFY(line.startsWith(QLatin1String("vault.type")));
    }
};

QTEST_GUILESS_MAIN(LinkSessionTest)
#include "tst_link_session.moc"
