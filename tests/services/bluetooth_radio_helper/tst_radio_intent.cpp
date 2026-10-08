// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/bluetooth_radio_helper/src/radio_operation_p.h"
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusReply>
#include <qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h>
#include <qindaqt/services/bluetooth_radio_helper/radio_service_session.h>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest/QTest>
#include <utility>

using namespace QindaQt::BluetoothRadio;
namespace {
class Helper final : public QDBusVirtualObject {
public:
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &) override {
        if (message.member() != QLatin1String("ObserveAndUnblock")) return false;
        pending = message; issued = qdbus_cast<Request>(message.arguments().constFirst());
        received = true; return true; // Deliberately deferred: inspect real live intent.
    }
    QDBusMessage pending;
    Request issued;
    bool received = false;
};
class Connections final {
public:
    explicit Connections(const QString &address) {
        for (int index = 0; index < 3; ++index) {
            const auto name = QStringLiteral("radio-intent-%1").arg(QUuid::createUuid().toString());
            names.append(name); buses.append(QDBusConnection::connectToBus(address, name));
        }
    }
    ~Connections() { for (const auto &name : names) QDBusConnection::disconnectFromBus(name); }
    QStringList names;
    QList<QDBusConnection> buses;
};
bool ask(const QDBusConnection &sender, const QString &owner, const Request &request) {
    auto message = QDBusMessage::createMethodCall(owner, QString::fromLatin1(kIntentPath),
        QString::fromLatin1(kIntentInterface), QStringLiteral("Current"));
    message << QVariant::fromValue(request);
    QDBusPendingCallWatcher watcher(sender.asyncCall(message, 500));
    if (!QTest::qWaitFor([&watcher] { return watcher.isFinished(); }, 1000)) return false;
    const auto reply = watcher.reply();
    return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == QLatin1String("b")
        && reply.arguments().size() == 1 && reply.arguments().constFirst().toBool();
}
}
namespace {
// This owns only the alias-admission seam for the real operation engine.
// Full Qt authority and complete-intent checks have separate fixtures above.
class AliasAuthority final : public RadioAuthority {
public:
    explicit AliasAuthority(QDBusConnection connection) : bus(std::move(connection)) {}
    bool current(const QString &sender, const Request &) override {
        bus.interface()->setTimeout(250);
        const auto owner = bus.interface()->serviceOwner(QStringLiteral("org.qindaqt.Bluetooth1"));
        return owner.isValid() && owner.value() == sender;
    }
private:
    QDBusConnection bus;
};
class CountedLease final : public RadioLease {
public:
    explicit CountedLease(int &count) : writes(count) {}
    RadioObservation observe() override { return {true, blocked, false}; }
    RadioWrite unblock(const std::function<bool()> &current) override {
        if (!current()) return RadioWrite::Denied;
        ++writes; blocked = false; return RadioWrite::Attempted;
    }
private:
    int &writes;
    bool blocked = true;
};
class CountedPlatform final : public RadioPlatform {
public:
    int writes = 0;
    RadioSelection select(const QString &) override {
        return {std::make_unique<CountedLease>(writes), {}};
    }
};
}
class RadioIntentTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void relinquishedUniqueOwnerReacquiresWithoutForgettingNonce() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); Connections peers(bus.address);
        auto ownerA = peers.buses[0], ownerB = peers.buses[1];
        const auto alias = QStringLiteral("org.qindaqt.Bluetooth1");
        QVERIFY(ownerA.registerService(alias));
        AliasAuthority authority(peers.buses[2]); CountedPlatform platform;
        RadioOperation operation(authority, platform, [] { return quint64(100); });
        Request original{QString(32, QLatin1Char('a')), bus.connection.baseService(),
            QStringLiteral("/org/bluez/hci0"), QStringLiteral("12:34:56:78:9A:BC"),
            ownerA.baseService(), 2100, ownerA.baseService(), ownerA.baseService()};
        QCOMPARE(operation.execute(ownerA.baseService(), original).disposition,
                 Disposition::VerifiedUnblocked);
        QVERIFY(ownerA.unregisterService(alias)); QVERIFY(ownerB.registerService(alias));
        auto second = original; second.nonce = QString(32, QLatin1Char('b'));
        second.initiatingCaller = ownerB.baseService();
        second.authorityOwner = ownerB.baseService(); second.transportCaller = ownerB.baseService();
        QCOMPARE(operation.execute(ownerB.baseService(), second).disposition,
                 Disposition::VerifiedUnblocked);
        QVERIFY(ownerB.unregisterService(alias)); QVERIFY(ownerA.registerService(alias));
        const auto replay = operation.execute(ownerA.baseService(), original);
        QCOMPARE(replay.reasonCode, QStringLiteral("radio-request-rejected"));
        QCOMPARE(replay.disposition, Disposition::Refused);
        QCOMPARE(platform.writes, 2);
    }
    void onlyExactIssuedRequestAndCurrentHelperCanQuery() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        Connections peers(bus.address);
        RadioServiceSession session(bus.address); QVERIFY(session.prepared());
        auto service = session.authorityConnection(), helperBus = peers.buses[1], foreign = peers.buses[2];
        QVERIFY(service.registerService(QStringLiteral("org.qindaqt.Bluetooth1")));
        Helper helper;
        QVERIFY(helperBus.registerVirtualObject(QString::fromLatin1(kPath), &helper));
        QVERIFY(helperBus.registerService(QString::fromLatin1(kService)));
        QtRadioPowerPort port(session);
        bool live = true;
        const auto id = port.observeAndUnblock(QStringLiteral(":1.90"), QStringLiteral("/org/bluez/hci0"),
            QStringLiteral("12:34:56:78:9A:BC"), foreign.baseService(), [&live] { return live; });
        QVERIFY(id); QTRY_VERIFY(helper.received);
        QVERIFY(ask(helperBus, service.baseService(), helper.issued));
        QVERIFY(!ask(foreign, service.baseService(), helper.issued));
        auto forged = helper.issued; forged.adapterPath = QStringLiteral("/org/bluez/hci1");
        QVERIFY(!ask(helperBus, service.baseService(), forged));
        forged = helper.issued; ++forged.deadlineBoottimeMs;
        QVERIFY(!ask(helperBus, service.baseService(), forged));
        forged = helper.issued; forged.initiatingCaller = helperBus.baseService();
        QVERIFY(!ask(helperBus, service.baseService(), forged));
        forged = helper.issued; forged.transportCaller = helperBus.baseService();
        QVERIFY(!ask(helperBus, service.baseService(), forged));
        forged = helper.issued; forged.authorityOwner = helperBus.baseService();
        QVERIFY(!ask(helperBus, service.baseService(), forged));
        live = false; QVERIFY(!ask(helperBus, service.baseService(), helper.issued));
        live = true; port.cancel(id);
        QVERIFY(!ask(helperBus, service.baseService(), helper.issued));
        helperBus.unregisterObject(QString::fromLatin1(kPath));
    }
    void sameUidReplacementHelperCannotReviveIntent() {
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start()); Connections peers(bus.address);
        RadioServiceSession session(bus.address); QVERIFY(session.prepared());
        auto service = session.authorityConnection(), helperBus = peers.buses[1], replacement = peers.buses[2];
        QVERIFY(service.registerService(QStringLiteral("org.qindaqt.Bluetooth1")));
        Helper helper;
        QVERIFY(helperBus.registerVirtualObject(QString::fromLatin1(kPath), &helper));
        QVERIFY(helperBus.registerService(QString::fromLatin1(kService)));
        QtRadioPowerPort port(session);
        QVERIFY(port.observeAndUnblock(QStringLiteral(":1.90"), QStringLiteral("/org/bluez/hci0"),
            QStringLiteral("12:34:56:78:9A:BC"), service.baseService(), [] { return true; }));
        QTRY_VERIFY(helper.received);
        QVERIFY(helperBus.unregisterService(QString::fromLatin1(kService)));
        QVERIFY(replacement.registerService(QString::fromLatin1(kService)));
        QVERIFY(!ask(helperBus, service.baseService(), helper.issued));
        QVERIFY(!ask(replacement, service.baseService(), helper.issued));
        helperBus.unregisterObject(QString::fromLatin1(kPath));
    }
};
QTEST_GUILESS_MAIN(RadioIntentTest)
#include "tst_radio_intent.moc"
