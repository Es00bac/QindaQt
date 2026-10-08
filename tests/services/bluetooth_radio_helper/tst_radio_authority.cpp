// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/bluetooth_radio_helper/src/qt_radio_authority_p.h"
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest/QTest>
#include <future>
#include <chrono>

using namespace QindaQt::BluetoothRadio;
namespace {
class Intent final : public QDBusVirtualObject {
public:
    Request issued;
    bool allowed = true;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &m, const QDBusConnection &bus) override {
        if (m.interface() != QLatin1String(kIntentInterface)) return false;
        const auto request = qdbus_cast<Request>(m.arguments().constFirst());
        bus.send(m.createReply(QVariantList{allowed && request == issued}));
        return true;
    }
};
class Adapter final : public QDBusVirtualObject {
public:
    QString address = QStringLiteral("12:34:56:78:9A:BC");
    int reads = 0;
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &m, const QDBusConnection &bus) override {
        if (m.interface() != QLatin1String("org.freedesktop.DBus.Properties")
            || m.member() != QLatin1String("Get")) return false;
        ++reads;
        bus.send(m.createReply(QVariantList{QVariant::fromValue(QDBusVariant(address))}));
        return true;
    }
};
}
class RadioAuthorityTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void currentBusOwnerCallerAndExactBluezSelection_data() {
        QTest::addColumn<QString>("change"); QTest::addColumn<bool>("expected");
        QTest::newRow("exact") << QString{} << true;
        QTest::newRow("foreign-sender") << QStringLiteral("sender") << false;
        QTest::newRow("dead-caller") << QStringLiteral("caller") << false;
        QTest::newRow("wrong-bluez-owner") << QStringLiteral("owner") << false;
        QTest::newRow("wrong-live-address") << QStringLiteral("address") << false;
        QTest::newRow("changed-issued-expiry") << QStringLiteral("expiry") << false;
        QTest::newRow("revoked-intent") << QStringLiteral("revoked") << false;
    }
    void currentBusOwnerCallerAndExactBluezSelection() {
        QFETCH(QString, change); QFETCH(bool, expected);
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); registerDBusTypes();
        const QString base = QUuid::createUuid().toString();
        auto daemon = QDBusConnection::connectToBus(bus.address, base + QStringLiteral("-daemon"));
        auto bluez = QDBusConnection::connectToBus(bus.address, base + QStringLiteral("-bluez"));
        auto helper = QDBusConnection::connectToBus(bus.address, base + QStringLiteral("-helper"));
        Intent intent; Adapter adapter;
        QVERIFY(daemon.registerService(QStringLiteral("org.qindaqt.Bluetooth1")));
        QVERIFY(bluez.registerService(QStringLiteral("org.bluez")));
        QVERIFY(daemon.registerVirtualObject(QString::fromLatin1(kIntentPath), &intent));
        QVERIFY(bluez.registerVirtualObject(QStringLiteral("/org/bluez/hci0"), &adapter));
        intent.issued = {QString(32, QLatin1Char('b')), bluez.baseService(),
            QStringLiteral("/org/bluez/hci0"), adapter.address, daemon.baseService(),
            boottimeMilliseconds() + kRequestWindowMs};
        auto request = intent.issued;
        auto sender = daemon.baseService();
        if (change == QLatin1String("sender")) sender = helper.baseService();
        if (change == QLatin1String("caller")) request.initiatingCaller = QStringLiteral(":1.999999");
        if (change == QLatin1String("owner")) request.bluezOwner = helper.baseService();
        if (change == QLatin1String("address")) adapter.address = QStringLiteral("AA:BB:CC:DD:EE:FF");
        if (change == QLatin1String("expiry")) ++request.deadlineBoottimeMs;
        if (change == QLatin1String("revoked")) intent.allowed = false;
        // The actual authority uses blocking bus reads on its owning worker;
        // the test event loop services independent fake peer connections.
        auto answer = std::async(std::launch::async, [helper, sender, request] {
            QtRadioAuthority authority(helper, helper);
            return authority.current(sender, request);
        });
        QVERIFY(QTest::qWaitFor([&answer] {
            return answer.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
        }, 3000));
        QCOMPARE(answer.get(), expected);
        if (expected) QCOMPARE(adapter.reads, 1);
        daemon.unregisterObject(QString::fromLatin1(kIntentPath));
        bluez.unregisterObject(QStringLiteral("/org/bluez/hci0"));
        QDBusConnection::disconnectFromBus(base + QStringLiteral("-daemon"));
        QDBusConnection::disconnectFromBus(base + QStringLiteral("-bluez"));
        QDBusConnection::disconnectFromBus(base + QStringLiteral("-helper"));
    }
};
QTEST_GUILESS_MAIN(RadioAuthorityTest)
#include "tst_radio_authority.moc"
