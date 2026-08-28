// SPDX-License-Identifier: GPL-3.0-or-later

#include "../power_service/support/fake_power_collaborators.h"

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_client/qt_power_transport.h>
#include <qindaqt/services/power_protocol/power_dbus.h>
#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_service/resident_power_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

#include <memory>

using namespace QindaQt::Power;
using namespace QindaQt::Tests;

namespace {

class PrivateBus final
{
public:
    bool start()
    {
        process.setProgram(QStringLiteral("dbus-daemon"));
        process.setArguments({QStringLiteral("--session"), QStringLiteral("--nofork"),
                              QStringLiteral("--nopidfile"),
                              QStringLiteral("--print-address=1")});
        process.start();
        if (!process.waitForStarted() || !process.waitForReadyRead()) {
            return false;
        }
        address = QString::fromUtf8(process.readLine()).trimmed();
        name = QStringLiteral("qindaqt-power-client-test-%1")
                   .arg(QUuid::createUuid().toString(QUuid::Id128));
        connection = QDBusConnection::connectToBus(address, name);
        return !address.isEmpty() && connection.isConnected();
    }

    ~PrivateBus()
    {
        if (!name.isEmpty()) {
            QDBusConnection::disconnectFromBus(name);
        }
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished();
        }
    }

    QProcess process;
    QString address;
    QString name;
    QDBusConnection connection{QStringLiteral("invalid")};
};

class ResidentHost final
{
public:
    ResidentHost(const PrivateBus &bus, QString suffix)
        : m_connectionName(bus.name + std::move(suffix))
        , m_connection(QDBusConnection::connectToBus(bus.address, m_connectionName))
    {
    }

    ~ResidentHost()
    {
        if (m_service != nullptr) {
            m_service->stop();
        }
        if (m_connection.isConnected()) {
            QDBusConnection::disconnectFromBus(m_connectionName);
        }
    }

    [[nodiscard]] bool isConnected() const { return m_connection.isConnected(); }

    [[nodiscard]] bool start(const QString &serviceName)
    {
        auto battery = std::make_unique<FakeBatteryCollaborator>();
        auto profiles = std::make_unique<FakeProfileCollaborator>();
        auto session = std::make_unique<FakeSessionCollaborator>();
        m_battery = battery.get();
        m_profiles = profiles.get();
        m_session = session.get();
        m_service = std::make_unique<ResidentPowerService>(
            std::move(battery), std::move(profiles), std::move(session), m_connection,
            serviceName);
        return m_service->start() == PowerServiceStartStatus::Started;
    }

    void publishReadyFacts()
    {
        m_battery->publish(fixtureBatteryFacts());
        m_profiles->publish(fixtureProfileFacts());
        m_session->publish(fixtureSessionFacts());
    }

    FakeBatteryCollaborator *battery() const { return m_battery; }
    ResidentPowerService *service() const { return m_service.get(); }

private:
    QString m_connectionName;
    QDBusConnection m_connection;
    FakeBatteryCollaborator *m_battery = nullptr;
    FakeProfileCollaborator *m_profiles = nullptr;
    FakeSessionCollaborator *m_session = nullptr;
    std::unique_ptr<ResidentPowerService> m_service;
};

} // namespace

class QtPowerTransportTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void successiveOwnersDelayedOperationAndEpochFencing();
};

void QtPowerTransportTests::successiveOwnersDelayedOperationAndEpochFencing()
{
    registerDBusTypes();
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.PowerClient.p%1")
                                    .arg(QCoreApplication::applicationPid());

    ResidentHost first(bus, QStringLiteral("-service-1"));
    QVERIFY(first.isConnected());
    QVERIFY(first.start(serviceName));
    first.publishReadyFacts();

    // The registered object must expose exactly the fixed Power1 signatures.
    QDBusPendingCallWatcher introspection(bus.connection.asyncCall(
        QDBusMessage::createMethodCall(
            serviceName, QString::fromLatin1(kObjectPath),
            QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"))));
    QSignalSpy introspected(&introspection, &QDBusPendingCallWatcher::finished);
    QTRY_COMPARE(introspected.size(), 1);
    const QDBusPendingReply<QString> introspectionReply = introspection;
    QVERIFY2(!introspectionReply.isError(), qPrintable(introspectionReply.error().message()));
    const QString introspectionXml = introspectionReply.value();
    QVERIFY(introspectionXml.contains(
        QStringLiteral("type=\"(uttuuss(bbbbbbb)(bubduubdbxbxu)a((ts)ussbbduubddbdbx"
                       "bxu)(sa(ss)a((ts)sss)s)a(ssss)a((ts)sbuuub)a((ts)sbuubuuus)"
                       "(bsut))\"")));

    QtPowerTransport transport(bus.connection, serviceName);
    PowerClient client(&transport);
    QSignalSpy completed(&client, &PowerClient::operationCompleted);
    client.start();
    QTRY_COMPARE(client.state(), PowerClientState::Ready);
    QVERIFY(client.snapshot().capabilities.testFlag(Capability::Supplies));
    const quint64 firstEpoch = client.snapshot().epoch;
    const QString firstOwner = client.owner();
    QVERIFY(firstOwner.startsWith(QLatin1Char(':')));

    // A delayed operation completes exactly once through the real transport.
    const quint64 requestId = client.setKeyboardBrightness(
        client.snapshot().keyboardBacklights.first().handle, 200);
    QTRY_COMPARE(first.battery()->keyboardOperations.size(), 1);
    first.battery()->finish(first.battery()->keyboardOperations.first().operationId,
                            succeededOutcome());
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(0).toULongLong(), requestId);
    QCOMPARE(completed.first().at(1).value<OperationResult>().status,
             OperationStatus::Succeeded);

    // Upstream authority replacement advances the epoch; the client rejects
    // stale-epoch handles instead of replaying the operation.
    first.battery()->replaceAuthority();
    first.publishReadyFacts();
    QTRY_VERIFY(client.snapshot().epoch > firstEpoch);
    const quint64 staleRequestId = client.setKeyboardBrightness(
        {.epoch = firstEpoch, .opaqueId = QStringLiteral("keyboard-kbd0")}, 10);
    QTRY_COMPARE(completed.size(), 2);
    QCOMPARE(completed.last().at(1).value<OperationResult>().status,
             OperationStatus::Rejected);
    QCOMPARE(completed.last().at(1).value<OperationResult>().reasonCode,
             QStringLiteral("stale-handle"));
    QVERIFY(staleRequestId != 0);

    first.service()->stop();
    QTRY_COMPARE(client.state(), PowerClientState::Unavailable);
    QVERIFY(client.owner().isEmpty());

    ResidentHost second(bus, QStringLiteral("-service-2"));
    QVERIFY(second.isConnected());
    QVERIFY(second.start(serviceName));
    second.publishReadyFacts();
    QTRY_COMPARE(client.state(), PowerClientState::Ready);
    QVERIFY(client.snapshot().epoch != firstEpoch);
    QVERIFY(client.owner() != firstOwner);
    client.stop();
}

QTEST_GUILESS_MAIN(QtPowerTransportTests)
#include "tst_qt_power_transport.moc"
