// SPDX-License-Identifier: GPL-3.0-or-later

#include "../network_service/support/fake_network_backend.h"

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_qt_transport/qt_network_transport.h>
#include <qindaqt/services/network_service/resident_network_service.h>

#include <QtCore/QProcess>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>
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
    clientName = QStringLiteral("qindaqt-network-transport-%1")
                     .arg(QUuid::createUuid().toString(QUuid::Id128));
    client = QDBusConnection::connectToBus(address, clientName);
    return !address.isEmpty() && client.isConnected();
  }

  ~PrivateBus() {
    if (!clientName.isEmpty()) {
      QDBusConnection::disconnectFromBus(clientName);
    }
    daemon.terminate();
    if (!daemon.waitForFinished(1'000)) {
      daemon.kill();
      daemon.waitForFinished();
    }
  }

  QProcess daemon;
  QString address;
  QString clientName;
  QDBusConnection client{QStringLiteral("invalid")};
};

std::unique_ptr<ResidentNetworkService>
resident(const QDBusConnection &connection, const QString &serviceName,
         FakeNetworkBackend **backendOut) {
  auto backend = std::make_unique<FakeNetworkBackend>();
  *backendOut = backend.get();
  return std::make_unique<ResidentNetworkService>(std::move(backend),
                                                  connection, serviceName);
}

} // namespace

class LegacyNetworkObject final : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Network1")
public:
  Snapshot snapshot;
public Q_SLOTS:
  Q_SCRIPTABLE QByteArray GetSnapshot() const {
    return encodeSnapshot(snapshot).payload;
  }
};
class HiddenTransportTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void actualOldServiceUnknownMethod();
  void newServicePendingAndReply();
};
void HiddenTransportTests::actualOldServiceUnknownMethod() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString name = QStringLiteral("org.qindaqt.HiddenOld.p%1")
                           .arg(QCoreApplication::applicationPid());
  const QString connectionName = bus.clientName + QStringLiteral("-old");
  auto server = QDBusConnection::connectToBus(bus.address, connectionName);
  LegacyNetworkObject old;
  const auto observation = readyNetworkObservation();
  old.snapshot.owner = server.baseService();
  old.snapshot.epoch = 99;
  old.snapshot.revision = 1;
  old.snapshot.availability = Availability::Ready;
  old.snapshot.capabilities = observation.capabilities;
  old.snapshot.radios = observation.radios;
  old.snapshot.devices = observation.devices;
  old.snapshot.knownNetworks = observation.knownNetworks;
  QVERIFY(server.registerObject(QString::fromLatin1(kObjectPath), &old,
                                QDBusConnection::ExportScriptableSlots));
  QVERIFY(server.registerService(name));
  QtNetworkTransport transport(bus.client, name);
  NetworkClient client(transport);
  QSignalSpy completed(&client, &NetworkClient::operationFinished);
  QSignalSpy uncertain(&client, &NetworkClient::operationUncertain);
  QVERIFY(client.start());
  QTRY_COMPARE(client.state(), ClientState::Ready);
  QVERIFY(client.connectHiddenNetwork({QStringLiteral("wlan0"),
                                       QStringLiteral("hidden"),
                                       SecuritySuite::Wpa2Personal}));
  QTRY_COMPARE(completed.size(), 1);
  QCOMPARE(completed.first().at(0).value<OperationResult>().status,
           OperationStatus::Unsupported);
  QCOMPARE(completed.first().at(0).value<OperationResult>().kind,
           OperationKind::ConnectKnownNetwork);
  QCOMPARE(uncertain.size(), 0);
  QVERIFY(!client.operationInFlight());
  client.stop();
  server.unregisterService(name);
  server.unregisterObject(QString::fromLatin1(kObjectPath));
  QDBusConnection::disconnectFromBus(connectionName);
}
void HiddenTransportTests::newServicePendingAndReply() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString name = QStringLiteral("org.qindaqt.HiddenNew.p%1")
                           .arg(QCoreApplication::applicationPid());
  const QString connectionName = bus.clientName + QStringLiteral("-new");
  auto server = QDBusConnection::connectToBus(bus.address, connectionName);
  FakeNetworkBackend *backend = nullptr;
  auto service = resident(server, name, &backend);
  QCOMPARE(service->start(), NetworkServiceStartStatus::Started);
  backend->publish(readyNetworkObservation());
  QtNetworkTransport transport(bus.client, name);
  QSignalSpy owners(&transport, &NetworkTransport::ownerChanged);
  QSignalSpy snapshots(&transport, &NetworkTransport::snapshotReceived);
  QSignalSpy completed(&transport, &NetworkTransport::operationReceived);
  QVERIFY(transport.start());
  QTRY_VERIFY(!owners.isEmpty());
  const QString owner = owners.last().at(0).toString();
  transport.requestSnapshot(1, owner);
  QTRY_COMPARE(snapshots.size(), 1);
  const QByteArray original = snapshots.first().at(2).toByteArray();
  Snapshot decoded;
  QVERIFY(decodeSnapshot(original, decoded).succeeded());
  transport.requestOperation(
      2, owner, decoded.epoch, decoded.revision,
      OperationKind::ConnectKnownNetwork,
      {{QStringLiteral("deviceInterface"), QStringLiteral("wlan0")},
       {QStringLiteral("ssid"), QStringLiteral(" hidden ")},
       {QStringLiteral("security"), quint32(SecuritySuite::Wpa3Personal)}});
  QTRY_COMPARE(backend->calls.size(), 1);
  QVERIFY(backend->calls.first().request.hiddenJoin);
  QCOMPARE(backend->calls.first().request.hiddenJoin->ssid,
           QStringLiteral(" hidden "));
  transport.requestSnapshot(3, owner);
  QTRY_COMPARE(snapshots.size(), 2);
  QCOMPARE(snapshots.last().at(2).toByteArray(), original);
  backend->finish(backend->calls.first().operationId,
                  {BackendOperationStatus::Succeeded, {}, {}});
  QTRY_COMPARE(completed.size(), 1);
  OperationResult result;
  QVERIFY(decodeOperationResult(completed.first().at(2).toByteArray(), result)
              .succeeded());
  QCOMPARE(result.kind, OperationKind::ConnectKnownNetwork);
  QCOMPARE(result.status, OperationStatus::Succeeded);
  QSignalSpy failures(&transport, &NetworkTransport::requestFailed);
  for (const QString &bad :
       {QString(33, u'x'), QString(17, QChar(0x00e9)), QString(QChar(0xd800)),
        QStringLiteral("line\nbreak")}) {
    transport.requestOperation(
        4, owner, decoded.epoch, decoded.revision,
        OperationKind::ConnectKnownNetwork,
        {{QStringLiteral("deviceInterface"), QStringLiteral("wlan0")},
         {QStringLiteral("ssid"), bad},
         {QStringLiteral("security"), quint32(SecuritySuite::Wpa2Personal)}});
  }
  QCOMPARE(failures.size(), 4);
  QCOMPARE(backend->calls.size(), 1);
  transport.stop();
  service->stop();
  service.reset();
  QDBusConnection::disconnectFromBus(connectionName);
}
QTEST_GUILESS_MAIN(HiddenTransportTests)
#include "tst_network_hidden_transport.moc"
