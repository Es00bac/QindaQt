// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_schedule_client.h>

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QProcess>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Services::NightLight;
namespace {
const QString kName = QStringLiteral("org.qindaqt.NightLight");
const QString kPath = QStringLiteral("/org/qindaqt/NightLight");
const QString kInterface = QStringLiteral("org.qindaqt.NightLight.Schedule1");

class PrivateBus final {
public:
    PrivateBus()
    {
        process.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
            {QStringLiteral("--session"), QStringLiteral("--nofork"),
             QStringLiteral("--print-address=1"), QStringLiteral("--print-pid=1")});
        if (!process.waitForStarted()) return;
        if (!process.waitForReadyRead()) return;
        address = QString::fromUtf8(process.readLine()).trimmed();
        process.readLine();
        valid = !address.isEmpty();
    }
    ~PrivateBus() { process.terminate(); process.waitForFinished(); }
    QProcess process;
    QString address;
    bool valid = false;
};

QVariantMap validValues()
{
    const qulonglong a = 1781251200000ULL;
    return {{QStringLiteral("active"), true},
            {QStringLiteral("mode"), QStringLiteral("DarkLight")},
            {QStringLiteral("dayTemperatureKelvin"), qint32(6500)},
            {QStringLiteral("nightTemperatureKelvin"), qint32(4500)},
            {QStringLiteral("disabledOutputs"), QStringList{}},
        {QStringLiteral("disabledOutputUuids"), QStringList{}},
        {QStringLiteral("outputIdentityAvailable"), true},
            {QStringLiteral("scheduleSource"), QStringLiteral("Times")},
            {QStringLiteral("automaticLocation"), false},
            {QStringLiteral("latitudeDegrees"), 0.0},
            {QStringLiteral("longitudeDegrees"), 0.0},
            {QStringLiteral("sunriseStart"), QStringLiteral("06:00:00")},
            {QStringLiteral("sunsetStart"), QStringLiteral("18:00:00")},
            {QStringLiteral("transitionSeconds"), qint32(1800)},
            {QStringLiteral("available"), true},
            {QStringLiteral("daylight"), true},
            {QStringLiteral("previousStartMs"), a},
            {QStringLiteral("previousEndMs"), a + 1800000},
            {QStringLiteral("nextStartMs"), a + 43200000},
            {QStringLiteral("nextEndMs"), a + 45000000},
            {QStringLiteral("diagnostic"), QString{}}};
}

class FakeScheduleService final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.NightLight.Schedule1")
public:
    explicit FakeScheduleService(QDBusConnection bus) : m_bus(std::move(bus)) {}
    QString caller;
    QByteArray nonce;
    int subscribeCount = 0;
    void sendFrame(quint64 cookie, quint64 revision, QVariantMap values,
                   const QByteArray &frameNonce = {})
    {
        auto signal = QDBusMessage::createTargetedSignal(
            caller, kPath, kInterface, QStringLiteral("ScheduleFrame"));
        signal.setArguments({QVariant(frameNonce.isEmpty() ? nonce : frameNonce),
                             QVariant::fromValue(qulonglong(cookie)),
                             QVariant::fromValue(qulonglong(revision)), QVariant(values)});
        m_bus.send(signal);
    }
public Q_SLOTS:
    void Subscribe(const QByteArray &requestNonce)
    {
        caller = message().service();
        nonce = requestNonce;
        ++subscribeCount;
    }
    void Unsubscribe(const QByteArray &, qulonglong) {}
private:
    QDBusConnection m_bus;
};
}

class NightLightScheduleClientTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void ownerNonceRevisionAndSignalReceiptFenceFrames();
};

void NightLightScheduleClientTests::ownerNonceRevisionAndSignalReceiptFenceFrames()
{
    PrivateBus daemon;
    QVERIFY(daemon.valid);
    const auto makeBus = [&daemon](const QString &name) {
        return QDBusConnection::connectToBus(daemon.address, name);
    };
    auto serverBus = makeBus(QStringLiteral("schedule-server-one"));
    auto clientBus = makeBus(QStringLiteral("schedule-client"));
    auto attackerBus = makeBus(QStringLiteral("schedule-attacker"));
    QVERIFY(serverBus.isConnected());
    QVERIFY(clientBus.isConnected());
    QVERIFY(attackerBus.isConnected());
    QVERIFY(serverBus.registerService(kName));
    FakeScheduleService first(serverBus);
    QVERIFY(serverBus.registerObject(kPath, &first, QDBusConnection::ExportAllSlots));

    QtNightLightScheduleClient client(clientBus);
    QSignalSpy states(&client, &QtNightLightScheduleClient::stateChanged);
    QSignalSpy unavailable(&client, &QtNightLightScheduleClient::unavailable);
    client.start();
    QTRY_COMPARE_WITH_TIMEOUT(first.subscribeCount, 1, 5000);
    QVERIFY(!first.nonce.isEmpty());
    QTest::qWait(100); // A successful empty Subscribe reply is not an authorized frame.
    QVERIFY(!client.state());
    QCOMPARE(states.size(), 0);

    const quint64 cookie = 17;
    first.sendFrame(cookie, 1, validValues());
    QTRY_VERIFY_WITH_TIMEOUT(client.state().has_value(), 5000);
    QCOMPARE(client.state()->revision, quint64(1));
    QCOMPARE(client.state()->settings.output.dayTemperatureKelvin, 6500);
    QCOMPARE(states.size(), 1);

    auto forged = QDBusMessage::createTargetedSignal(
        clientBus.baseService(), kPath, kInterface, QStringLiteral("ScheduleFrame"));
    forged.setArguments({QVariant(first.nonce), QVariant::fromValue(qulonglong(cookie)),
                         QVariant::fromValue(qulonglong(2)), QVariant(validValues())});
    QVERIFY(attackerBus.send(forged));
    QTest::qWait(50);
    QCOMPARE(states.size(), 1);

    auto hostile = validValues();
    hostile[QStringLiteral("dayTemperatureKelvin")] = QStringLiteral("6500");
    first.sendFrame(cookie, 2, hostile);
    QTest::qWait(100);
    QCOMPARE(states.size(), 1);
    QVERIFY(unavailable.size() >= 1);
    QVERIFY(!client.state());

    const QByteArray retiredNonce = first.nonce;
    serverBus.unregisterService(kName);
    auto replacementBus = makeBus(QStringLiteral("schedule-server-two"));
    QVERIFY(replacementBus.isConnected());
    QVERIFY(replacementBus.registerService(kName));
    FakeScheduleService second(replacementBus);
    QVERIFY(replacementBus.registerObject(kPath, &second, QDBusConnection::ExportAllSlots));
    client.start();
    QTRY_COMPARE_WITH_TIMEOUT(second.subscribeCount, 1, 5000);
    QCOMPARE(second.nonce.size(), 16);
    QVERIFY(retiredNonce != second.nonce);
    first.sendFrame(cookie, 3, validValues(), retiredNonce);
    QTest::qWait(100);
    QVERIFY(!client.state());
    second.sendFrame(29, 1, validValues());
    QTRY_VERIFY_WITH_TIMEOUT(client.state().has_value(), 5000);
    QCOMPARE(client.state()->revision, quint64(1));
    client.stop();
    serverBus.unregisterObject(kPath);
    QDBusConnection::disconnectFromBus(QStringLiteral("schedule-server-one"));
    replacementBus.unregisterObject(kPath);
    replacementBus.unregisterService(kName);
    QDBusConnection::disconnectFromBus(QStringLiteral("schedule-server-two"));
    QDBusConnection::disconnectFromBus(QStringLiteral("schedule-client"));
    QDBusConnection::disconnectFromBus(QStringLiteral("schedule-attacker"));
}

QTEST_GUILESS_MAIN(NightLightScheduleClientTests)
#include "tst_night_light_schedule_client.moc"
