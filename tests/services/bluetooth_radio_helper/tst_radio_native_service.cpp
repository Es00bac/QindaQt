// SPDX-License-Identifier: GPL-3.0-or-later
#include "../bluetooth_bluez_adapter/support/private_bus.h"
#include "../../../src/services/bluetooth_radio_helper/src/native_radio_authority_p.h"
#include "../../../src/services/bluetooth_radio_helper/src/native_radio_service_p.h"
#include <qindaqt/services/bluetooth_radio_helper/qt_radio_power_port.h>
#include <qindaqt/services/bluetooth_radio_helper/radio_service_session.h>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusVirtualObject>
#include <QtTest/QTest>
#include <QtTest/QSignalSpy>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace QindaQt::BluetoothRadio;
namespace {
class Adapter final : public QDBusVirtualObject {
public:
    QString address = QStringLiteral("12:34:56:78:9A:BC");
    QString introspect(const QString &) const override { return {}; }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override {
        if (message.interface() != QLatin1String("org.freedesktop.DBus.Properties")
            || message.member() != QLatin1String("Get")) return false;
        bus.send(message.createReply(QVariantList{QVariant::fromValue(QDBusVariant(address))}));
        return true;
    }
};
class MemoryLease final : public RadioLease {
public:
    explicit MemoryLease(std::atomic<int> &writes) : m_writes(writes) {}
    RadioObservation observe() override { return {true, blocked, false}; }
    RadioWrite unblock(const std::function<bool()> &current) override {
        if (!current()) return RadioWrite::Denied;
        ++m_writes; blocked = false; return RadioWrite::Attempted;
    }
private:
    std::atomic<int> &m_writes;
    bool blocked = true;
};
class MemoryPlatform final : public RadioPlatform {
public:
    explicit MemoryPlatform(std::atomic<int> &writes) : m_writes(writes) {}
    RadioSelection select(const QString &) override {
        return {std::make_unique<MemoryLease>(m_writes), {}};
    }
private:
    std::atomic<int> &m_writes;
};
// Owns only a disposable private broker peer. No Linux radio platform exists in
// this fixture: production dispatch/codec/authority call an in-memory lease.
class HelperThread final {
public:
    explicit HelperThread(QString address) {
        answer = std::async(std::launch::async, [this, address] {
            NativeRadioWire wire;
            if (!wire.open(address, false)) { ready = -1; return; }
            NativeRadioAuthority authority(wire, wire);
            MemoryPlatform platform(writes);
            RadioOperation operation(authority, platform, boottimeMilliseconds);
            NativeRadioService service(wire, operation);
            if (!wire.own(QString::fromLatin1(kService))) { ready = -1; return; }
            ready = 1;
            while (!stop) {
                wire.dispatchPending();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    ~HelperThread() { stop = true; if (answer.valid()) answer.wait(); }
    std::atomic<int> ready{0}, writes{0};
private:
    std::atomic<bool> stop{false};
    std::future<void> answer;
};
}
class RadioNativeServiceTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void actualNativeHelperDispatch_data() {
        QTest::addColumn<bool>("wrongAddress");
        QTest::newRow("issued-current-selected") << false;
        QTest::newRow("address-does-not-match") << true;
    }
    void actualNativeHelperDispatch() {
        QFETCH(bool, wrongAddress);
        QindaQt::Tests::PrivateBus bus; QVERIFY(bus.start());
        RadioServiceSession session(bus.address); QVERIFY(session.prepared());
        auto daemon = session.authorityConnection();
        QVERIFY(daemon.registerService(QStringLiteral("org.qindaqt.Bluetooth1")));
        const auto bluezName = QUuid::createUuid().toString(QUuid::Id128);
        auto bluez = QDBusConnection::connectToBus(bus.address, bluezName);
        Adapter adapter;
        QVERIFY(bluez.registerVirtualObject(QStringLiteral("/org/bluez/hci0"), &adapter));
        QVERIFY(bluez.registerService(QStringLiteral("org.bluez")));
        {
            HelperThread helper(bus.address);
            QTRY_VERIFY(helper.ready.load() != 0); QCOMPARE(helper.ready.load(), 1);
            QtRadioPowerPort port(session); QSignalSpy done(&port, &RadioPowerPort::finished);
            QVERIFY(port.observeAndUnblock(bluez.baseService(), QStringLiteral("/org/bluez/hci0"),
                wrongAddress ? QStringLiteral("AA:BB:CC:DD:EE:FF") : adapter.address,
                daemon.baseService(), [] { return true; }));
            QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 3000);
            const auto result = qvariant_cast<Result>(done.constFirst().at(1));
            QCOMPARE(result.disposition, wrongAddress ? Disposition::Refused : Disposition::VerifiedUnblocked);
            QCOMPARE(helper.writes.load(), wrongAddress ? 0 : 1);
        }
        bluez.unregisterObject(QStringLiteral("/org/bluez/hci0"));
        QDBusConnection::disconnectFromBus(bluezName);
    }
};
QTEST_GUILESS_MAIN(RadioNativeServiceTest)
#include "tst_radio_native_service.moc"
