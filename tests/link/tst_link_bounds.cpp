// SPDX-License-Identifier: GPL-3.0-or-later
// Bounds on unauthenticated connections, persistence failure and strict
// pairing fields (protocol page, "Connection lifecycle" and "Pairing").
#include "support/hub_fixture.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QDir>
#include <QtCore/QFile>

using namespace QindaQt::Link;
using namespace QindaQt::Link::Test;

class LinkBoundsTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void helloDeadlineClosesSilently()
    {
        SessionTimings t;
        t.helloDeadlineMs = 150;
        HubFixture f(t);
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()));
        QVERIFY(phone.waitClosed(3000));
        QCOMPARE(phone.count(QStringLiteral("bye")), 0);
    }

    void unpairedLifetimeIsNotExtendedByPing()
    {
        SessionTimings t;
        t.unpairedLifetimeMs = 500;
        t.pairTimeoutMs = 60000;
        t.pingIdleMs = 100;
        t.deadMs = 60000;
        HubFixture f(t);
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        QElapsedTimer clock;
        clock.start();
        Envelope bye;
        bool gotBye = false;
        while (clock.elapsed() < 3000 && !gotBye) {
            Envelope ping;
            ping.t = QStringLiteral("ping");
            ping.id = 1 + clock.elapsed();
            phone.send(ping);  // keeps traffic flowing; must not extend the lifetime
            gotBye = phone.waitFor(QStringLiteral("bye"), &bye, 80);
        }
        QVERIFY(gotBye);
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-timeout"));
        QVERIFY(clock.elapsed() < 2000);
        QVERIFY(phone.waitClosed());
    }

    void atMostMaxUnpairedConnections()
    {
        HubFixture f({}, 2);
        ScriptedPhone a, b, c;
        QVERIFY(a.connectTo(f.port()) && a.waitFor(QStringLiteral("hello")));
        a.sendHello(false);
        QVERIFY(b.connectTo(f.port()) && b.waitFor(QStringLiteral("hello")));
        b.sendHello(false);
        QVERIFY(waitUntil([&] { return f.hub.devices().size() == 2; }));
        QVERIFY(c.connectTo(f.port()));
        QVERIFY(c.waitClosed());
        QVERIFY(a.isConnected() && b.isConnected());
    }

    void onlyOnePairingAtATime()
    {
        HubFixture f;
        ScriptedPhone a, b;
        QVERIFY(a.connectTo(f.port()) && a.waitFor(QStringLiteral("hello")));
        a.sendHello(false);
        QString code;
        QVERIFY(a.pairUntilCode(&code));
        QVERIFY(b.connectTo(f.port()) && b.waitFor(QStringLiteral("hello")));
        b.sendHello(false);
        Envelope req;
        req.t = QStringLiteral("pair.request");
        req.id = 3;
        b.send(req);
        Envelope err;
        QVERIFY(b.waitFor(QStringLiteral("error"), &err));
        QCOMPARE(err.b.value(QStringLiteral("code")).toString(), QStringLiteral("busy"));
        a.send(makeBye(QStringLiteral("pair-rejected")));
        QVERIFY(a.waitClosed());
        req.id = 4;
        bool committed = false;
        for (int i = 0; i < 15 && !committed; ++i) {
            req.id = 10 + i;
            b.send(req);
            committed = b.waitFor(QStringLiteral("pair.commit"), nullptr, 200);
        }
        QVERIFY(committed);
    }

    void unwritableTrustStoreNeverEntersReady()
    {
        HubFixture f;
        ScriptedPhone phone;
        QSignalSpy requested(&f.hub, &Hub::pairingRequested);
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false, {QStringLiteral("status")});
        QString code;
        QVERIFY(phone.pairUntilCode(&code));
        QVERIFY(waitUntil([&] { return !requested.isEmpty(); }));
        // A directory named devices.json makes the atomic rename fail.
        QVERIFY(QDir(f.dir.path()).mkdir(QStringLiteral("devices.json")));
        f.hub.confirmPairing(phone.deviceId(), true);
        Envelope accept;
        accept.t = QStringLiteral("pair.confirm");
        accept.b = QJsonObject{{QStringLiteral("accept"), true}};
        phone.send(accept);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("error"));
        QVERIFY(phone.waitClosed());
        QVERIFY(!f.store.contains(phone.deviceId()));
        QVERIFY(f.hub.devices().isEmpty());
    }

    void malformedNonceRejectsPairing_data()
    {
        QTest::addColumn<QString>("np");
        QTest::newRow("uppercase") << QString(64, QLatin1Char('A'));
        QTest::newRow("short") << QString(62, QLatin1Char('a'));
        QTest::newRow("junk") << (QString(63, QLatin1Char('a')) + QLatin1Char('g'));
    }
    void malformedNonceRejectsPairing()
    {
        QFETCH(QString, np);
        HubFixture f;
        ScriptedPhone phone;
        QVERIFY(phone.connectTo(f.port()) && phone.waitFor(QStringLiteral("hello")));
        phone.sendHello(false);
        Envelope req;
        req.t = QStringLiteral("pair.request");
        req.id = 1;
        phone.send(req);
        QVERIFY(phone.waitFor(QStringLiteral("pair.commit")));
        Envelope nonce;
        nonce.t = QStringLiteral("pair.nonce");
        nonce.b = QJsonObject{{QStringLiteral("np"), np}};
        phone.send(nonce);
        Envelope bye;
        QVERIFY(phone.waitFor(QStringLiteral("bye"), &bye));
        QCOMPARE(bye.b.value(QStringLiteral("reason")).toString(), QStringLiteral("pair-rejected"));
    }
};

QTEST_GUILESS_MAIN(LinkBoundsTest)
#include "tst_link_bounds.moc"
