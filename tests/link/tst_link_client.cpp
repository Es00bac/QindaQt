// SPDX-License-Identifier: GPL-3.0-or-later
// LinkClient against a fake transport: parsing and availability gating.
#include <qindaqt/services/link_client/link_client.h>

#include <QtTest/QtTest>

using namespace QindaQt::Link;

namespace {
class FakeTransport final : public LinkTransport {
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestDevices(quint64 token) override { lastToken = token; }
    void confirmPairing(const QString &id, bool accept) override { confirmed = id + (accept ? "+" : "-"); }
    void unpair(const QString &id) override { unpaired = id; }
    void ring(const QString &id) override { rang = id; }
    quint64 lastToken = 0;
    QString confirmed, unpaired, rang;
};

const QByteArray kJson = R"([{"id":"abc","name":"Phone","model":"M","paired":true,"connected":true,
 "ready":true,"transport":"usb","battery":{"level":87,"charging":true,"plugged":"usb"},
 "network":{"type":"wifi","signal":null},"lastSeen":5},{"name":"no id"}])";
}

class LinkClientTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void parsesDevicesAndSkipsMalformed()
    {
        const auto list = parseDevices(kJson);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list[0].batteryLevel, 87);
        QVERIFY(list[0].charging);
        QCOMPARE(list[0].signal, -1);
        QCOMPARE(list[0].transport, QStringLiteral("usb"));
        QVERIFY(parseDevices("not json").isEmpty());
        // Link1 does not emit `caps` yet: absent means unknown (empty).
        QVERIFY(list[0].capabilities.isEmpty());
    }
    void parsesOptionalCapabilities()
    {
        const auto list = parseDevices(
            R"([{"id":"a","caps":["status","ring",7]},{"id":"b","caps":"ring"}])");
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].capabilities, (QStringList{QStringLiteral("status"), QStringLiteral("ring")}));
        QVERIFY(list[1].capabilities.isEmpty());
    }

    void staleRepliesAreIgnoredAndLossClearsDevices()
    {
        FakeTransport t;
        LinkClient c(t);
        QVERIFY(c.start());
        Q_EMIT t.serviceAvailableChanged(true);
        QVERIFY(c.available());
        const quint64 first = t.lastToken;
        Q_EMIT t.serviceAvailableChanged(false);
        QVERIFY(c.devices().isEmpty());
        Q_EMIT t.serviceAvailableChanged(true);
        Q_EMIT t.devicesReceived(first, kJson);  // reply to the previous incarnation
        QVERIFY(c.devices().isEmpty());
        Q_EMIT t.devicesReceived(t.lastToken, kJson);
        QCOMPARE(c.devices().size(), 1);
        c.confirmPairing(QStringLiteral("abc"), true);
        c.unpair(QStringLiteral("abc"));
        c.ring(QStringLiteral("abc"));
        QCOMPARE(t.confirmed, QStringLiteral("abc+"));
        QCOMPARE(t.unpaired, QStringLiteral("abc"));
        QCOMPARE(t.rang, QStringLiteral("abc"));
    }
};

QTEST_GUILESS_MAIN(LinkClientTest)
#include "tst_link_client.moc"
