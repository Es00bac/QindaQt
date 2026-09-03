// SPDX-License-Identifier: GPL-3.0-or-later

#include "support/fake_network_backend.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_limits.h>
#include <qindaqt/services/network_qt_transport/qt_network_transport.h>
#include <qindaqt/services/network_service/resident_network_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtTest>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::Service;
using namespace QindaQt::Network::Tests;

namespace {

class PrivateBus final {
public:
  bool start() {
    daemon.setProgram(QStringLiteral("dbus-daemon"));
    daemon.setArguments(
        {QStringLiteral("--session"), QStringLiteral("--nofork"),
         QStringLiteral("--nopidfile"), QStringLiteral("--print-address=1")});
    daemon.start();
    if (!daemon.waitForStarted() || !daemon.waitForReadyRead(5'000)) {
      return false;
    }
    address = QString::fromUtf8(daemon.readLine()).trimmed();
    name = QStringLiteral("qindaqt-network-resident-%1")
               .arg(QUuid::createUuid().toString(QUuid::Id128));
    connection = QDBusConnection::connectToBus(address, name);
    return !address.isEmpty() && connection.isConnected();
  }
  ~PrivateBus() {
    if (!name.isEmpty()) {
      QDBusConnection::disconnectFromBus(name);
    }
    daemon.terminate();
    if (!daemon.waitForFinished(1'000)) {
      daemon.kill();
      daemon.waitForFinished();
    }
  }
  QProcess daemon;
  QString address;
  QString name;
  QDBusConnection connection{QStringLiteral("invalid")};
};

} // namespace

class NetworkResidencyTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void clientOperatesOverFixedPrivateBusContract();
  void synchronousBackendCompletionRepliesExactlyOnce();
  void stopAndTimeoutReplyExactlyOnce();
  void clientProjectsServiceAvailabilityHonestly();
  void introspectionAndNameTheftAreExact();
};

void NetworkResidencyTests::clientOperatesOverFixedPrivateBusContract() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.r%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString serviceConnectionName = bus.name + QStringLiteral("-service");
  QDBusConnection serviceConnection =
      QDBusConnection::connectToBus(bus.address, serviceConnectionName);
  QVERIFY(serviceConnection.isConnected());
  auto backend = std::make_unique<FakeNetworkBackend>();
  FakeNetworkBackend *fake = backend.get();
  ResidentNetworkService resident(std::move(backend), serviceConnection,
                                  serviceName);
  QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);

  QtNetworkTransport transport(bus.connection, serviceName);
  NetworkClient client(transport);
  QSignalSpy finished(&client, &NetworkClient::operationFinished);
  QVERIFY(client.start());
  fake->publish(readyNetworkObservation());
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
  QCOMPARE(client.projection().owner, serviceConnection.baseService());
  QVERIFY(client.requestScan(30'000));
  QTRY_COMPARE_WITH_TIMEOUT(fake->calls.size(), 1, 5'000);
  BackendOperationOutcome success;
  success.status = BackendOperationStatus::Succeeded;
  fake->finish(fake->calls.first().operationId, success);
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5'000);
  QCOMPARE(finished.first().at(0).value<OperationResult>().status,
           OperationStatus::Succeeded);
  QTRY_VERIFY_WITH_TIMEOUT(client.operationAdmissionReady(), 5'000);
  const AccessPoint visible = client.projection().accessPoints.at(1);
  const QString accessPointId =
      visibleAccessPointId(visible.deviceInterface, visible.bssid);
  QVERIFY(client.connectVisibleNetwork(accessPointId));
  QTRY_COMPARE_WITH_TIMEOUT(fake->calls.size(), 2, 5'000);
  QCOMPARE(fake->calls.constLast().request.kind,
           OperationKind::ConnectVisibleNetwork);
  QCOMPARE(fake->calls.constLast().request.identifier, accessPointId);
  fake->finish(fake->calls.constLast().operationId, success);
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5'000);
  QCOMPARE(finished.constLast().at(0).value<OperationResult>().kind,
           OperationKind::ConnectVisibleNetwork);
  client.stop();
  resident.stop();
  QDBusConnection::disconnectFromBus(serviceConnectionName);
}

void NetworkResidencyTests::synchronousBackendCompletionRepliesExactlyOnce() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.s%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString serviceConnectionName = bus.name + QStringLiteral("-service");
  QDBusConnection serviceConnection =
      QDBusConnection::connectToBus(bus.address, serviceConnectionName);
  QVERIFY(serviceConnection.isConnected());
  auto backend = std::make_unique<FakeNetworkBackend>();
  FakeNetworkBackend *fake = backend.get();
  fake->synchronousOutcome = BackendOperationOutcome{
      .status = BackendOperationStatus::Failed,
      .reasonCode = QStringLiteral("scan-dispatch-failed"),
      .diagnostic = {}};
  ResidentNetworkService resident(std::move(backend), serviceConnection,
                                  serviceName);
  QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);

  QtNetworkTransport transport(bus.connection, serviceName);
  NetworkClient client(transport);
  QSignalSpy finished(&client, &NetworkClient::operationFinished);
  QVERIFY(client.start());
  fake->publish(readyNetworkObservation());
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
  QVERIFY(client.requestScan(30'000));
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2'000);
  QCOMPARE(fake->calls.size(), 1);
  const OperationResult result =
      finished.first().at(0).value<OperationResult>();
  QCOMPARE(result.status, OperationStatus::Failed);
  QCOMPARE(result.reasonCode, QStringLiteral("scan-dispatch-failed"));
  QTest::qWait(200);
  QCOMPARE(finished.size(), 1);

  client.stop();
  resident.stop();
  QDBusConnection::disconnectFromBus(serviceConnectionName);
}

void NetworkResidencyTests::stopAndTimeoutReplyExactlyOnce() {
  {
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.t%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString serviceConnectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, serviceConnectionName);
    auto backend = std::make_unique<FakeNetworkBackend>();
    FakeNetworkBackend *fake = backend.get();
    ResidentNetworkService resident(std::move(backend), serviceConnection,
                                    serviceName, 100);
    QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);
    QtNetworkTransport transport(bus.connection, serviceName);
    NetworkClient client(transport);
    QSignalSpy finished(&client, &NetworkClient::operationFinished);
    QVERIFY(client.start());
    fake->publish(readyNetworkObservation());
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
    QVERIFY(client.requestScan(30'000));
    QTRY_COMPARE_WITH_TIMEOUT(fake->calls.size(), 1, 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2'000);
    QCOMPARE(finished.first().at(0).value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(finished.first().at(0).value<OperationResult>().reasonCode,
             QStringLiteral("backend-timeout"));
    QCOMPARE(fake->cancelled, QList<quint64>{fake->calls.first().operationId});
    BackendOperationOutcome late;
    late.status = BackendOperationStatus::Succeeded;
    fake->finish(fake->calls.first().operationId, late);
    QTest::qWait(100);
    QCOMPARE(finished.size(), 1);
    client.stop();
    resident.stop();
    QDBusConnection::disconnectFromBus(serviceConnectionName);
  }

  {
    PrivateBus bus;
    QVERIFY(bus.start());
    const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.p%1")
                                    .arg(QCoreApplication::applicationPid());
    const QString serviceConnectionName = bus.name + QStringLiteral("-service");
    QDBusConnection serviceConnection =
        QDBusConnection::connectToBus(bus.address, serviceConnectionName);
    auto backend = std::make_unique<FakeNetworkBackend>();
    FakeNetworkBackend *fake = backend.get();
    ResidentNetworkService resident(std::move(backend), serviceConnection,
                                    serviceName);
    QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);
    QtNetworkTransport transport(bus.connection, serviceName);
    NetworkClient client(transport);
    QSignalSpy finished(&client, &NetworkClient::operationFinished);
    QVERIFY(client.start());
    fake->publish(readyNetworkObservation());
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Ready, 5'000);
    QVERIFY(client.requestScan(30'000));
    QTRY_COMPARE_WITH_TIMEOUT(fake->calls.size(), 1, 2'000);
    const quint64 operationId = fake->calls.first().operationId;
    resident.stop();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2'000);
    QCOMPARE(finished.first().at(0).value<OperationResult>().status,
             OperationStatus::Uncertain);
    QCOMPARE(fake->cancelled, QList<quint64>{operationId});
    BackendOperationOutcome late;
    late.status = BackendOperationStatus::Succeeded;
    fake->finish(operationId, late);
    QTest::qWait(100);
    QCOMPARE(finished.size(), 1);
    client.stop();
    QDBusConnection::disconnectFromBus(serviceConnectionName);
  }
}

void NetworkResidencyTests::clientProjectsServiceAvailabilityHonestly() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.a%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString serviceConnectionName = bus.name + QStringLiteral("-service");
  QDBusConnection serviceConnection =
      QDBusConnection::connectToBus(bus.address, serviceConnectionName);
  auto backend = std::make_unique<FakeNetworkBackend>();
  FakeNetworkBackend *fake = backend.get();
  ResidentNetworkService resident(std::move(backend), serviceConnection,
                                  serviceName);
  QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);

  QtNetworkTransport transport(bus.connection, serviceName);
  NetworkClient client(transport);
  QVERIFY(client.start());
  BackendObservation unavailable;
  unavailable.availability = Availability::Unavailable;
  unavailable.reasonCode = QStringLiteral("networkmanager-unavailable");
  fake->publish(unavailable);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Unavailable, 5'000);
  QVERIFY(client.model().snapshot().has_value());
  QCOMPARE(client.model().snapshot()->availability, Availability::Unavailable);

  BackendObservation degraded;
  degraded.availability = Availability::Degraded;
  degraded.reasonCode = QStringLiteral("networkmanager-data-degraded");
  fake->publish(degraded);
  QTRY_COMPARE_WITH_TIMEOUT(client.state(), ClientState::Degraded, 5'000);
  QCOMPARE(client.model().snapshot()->availability, Availability::Degraded);
  QVERIFY(client.lastError().contains(
      QStringLiteral("networkmanager-data-degraded")));

  client.stop();
  resident.stop();
  QDBusConnection::disconnectFromBus(serviceConnectionName);
}

void NetworkResidencyTests::introspectionAndNameTheftAreExact() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.i%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString serviceConnectionName = bus.name + QStringLiteral("-service");
  QDBusConnection serviceConnection =
      QDBusConnection::connectToBus(bus.address, serviceConnectionName);
  auto backend = std::make_unique<FakeNetworkBackend>();
  ResidentNetworkService resident(std::move(backend), serviceConnection,
                                  serviceName);
  QCOMPARE(resident.start(), NetworkServiceStartStatus::Started);

  QDBusMessage call = QDBusMessage::createMethodCall(
      serviceName, QString::fromLatin1(kObjectPath),
      QStringLiteral("org.freedesktop.DBus.Introspectable"),
      QStringLiteral("Introspect"));
  QDBusPendingCallWatcher pending(bus.connection.asyncCall(call));
  QSignalSpy replied(&pending, &QDBusPendingCallWatcher::finished);
  QTRY_COMPARE_WITH_TIMEOUT(replied.size(), 1, 5'000);
  const QDBusPendingReply<QString> reply = pending;
  QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
  const QString xml = reply.value();
  QVERIFY(xml.contains(QStringLiteral("name=\"RequestScan\"")));
  QVERIFY(xml.contains(QStringLiteral("name=\"ConnectKnownNetwork\"")));
  QVERIFY(xml.contains(QStringLiteral("name=\"ConnectVisibleNetwork\"")));
  QVERIFY(xml.contains(QStringLiteral("name=\"DisconnectActive\"")));
  QVERIFY(xml.contains(QStringLiteral("name=\"SetRadio\"")));
  QVERIFY(xml.contains(QStringLiteral("type=\"ay\"")));
  QVERIFY(xml.contains(QStringLiteral("type=\"x\"")));
  const qsizetype networkBegin =
      xml.indexOf(QStringLiteral("<interface name=\"org.qindaqt.Network1\">"));
  const qsizetype networkEnd =
      xml.indexOf(QStringLiteral("</interface>"), networkBegin);
  QVERIFY(networkBegin >= 0 && networkEnd > networkBegin);
  QVERIFY(!xml.mid(networkBegin, networkEnd - networkBegin)
               .contains(QStringLiteral("a{sv}")));

  auto loserBackend = std::make_unique<FakeNetworkBackend>();
  ResidentNetworkService loser(std::move(loserBackend), bus.connection,
                               serviceName);
  QCOMPARE(loser.start(), NetworkServiceStartStatus::NameAlreadyOwned);
  QVERIFY(!loser.isRunning());
  resident.stop();
  QDBusConnection::disconnectFromBus(serviceConnectionName);
}

QTEST_GUILESS_MAIN(NetworkResidencyTests)
#include "tst_network_residency.moc"
