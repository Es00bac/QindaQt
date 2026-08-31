// SPDX-License-Identifier: GPL-3.0-or-later

#include "../network_service/support/fake_network_backend.h"

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

class QtNetworkTransportTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void exactOwnerLifecycleAndCanonicalPayload();
  void refusesCredentialShapedParametersBeforeBus();
  void invalidConnectionFailsAtomically();
};

void QtNetworkTransportTests::exactOwnerLifecycleAndCanonicalPayload() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.t%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString firstName = bus.clientName + QStringLiteral("-first");
  QDBusConnection firstConnection =
      QDBusConnection::connectToBus(bus.address, firstName);
  QVERIFY(firstConnection.isConnected());
  FakeNetworkBackend *firstBackend = nullptr;
  auto first = resident(firstConnection, serviceName, &firstBackend);
  QCOMPARE(first->start(), NetworkServiceStartStatus::Started);
  firstBackend->publish(readyNetworkObservation());

  QtNetworkTransport transport(bus.client, serviceName);
  QSignalSpy owners(&transport, &NetworkTransport::ownerChanged);
  QSignalSpy snapshots(&transport, &NetworkTransport::snapshotReceived);
  QVERIFY(transport.start());
  QTRY_VERIFY_WITH_TIMEOUT(!owners.isEmpty(), 5'000);
  const QString firstOwner = owners.constLast().at(0).toString();
  QCOMPARE(firstOwner, firstConnection.baseService());
  transport.requestSnapshot(11, firstOwner);
  QTRY_COMPARE_WITH_TIMEOUT(snapshots.size(), 1, 5'000);
  Snapshot decoded;
  QVERIFY(decodeSnapshot(snapshots.first().at(2).toByteArray(), decoded)
              .succeeded());
  QCOMPARE(decoded.owner, firstOwner);
  QCOMPARE(decoded.availability, Availability::Ready);

  first->stop();
  first.reset();
  QDBusConnection::disconnectFromBus(firstName);
  QTRY_VERIFY_WITH_TIMEOUT(!owners.isEmpty() &&
                               owners.constLast().at(0).toString().isEmpty(),
                           5'000);

  const QString secondName = bus.clientName + QStringLiteral("-second");
  QDBusConnection secondConnection =
      QDBusConnection::connectToBus(bus.address, secondName);
  QVERIFY(secondConnection.isConnected());
  FakeNetworkBackend *secondBackend = nullptr;
  auto second = resident(secondConnection, serviceName, &secondBackend);
  QCOMPARE(second->start(), NetworkServiceStartStatus::Started);
  secondBackend->publish(readyNetworkObservation());
  QTRY_COMPARE_WITH_TIMEOUT(owners.constLast().at(0).toString(),
                            secondConnection.baseService(), 5'000);
  QVERIFY(secondConnection.baseService() != firstOwner);
  transport.stop();
  second->stop();
  QDBusConnection::disconnectFromBus(secondName);
}

void QtNetworkTransportTests::refusesCredentialShapedParametersBeforeBus() {
  PrivateBus bus;
  QVERIFY(bus.start());
  const QString serviceName = QStringLiteral("org.qindaqt.NetworkTest.s%1")
                                  .arg(QCoreApplication::applicationPid());
  const QString serviceConnectionName =
      bus.clientName + QStringLiteral("-service");
  QDBusConnection serviceConnection =
      QDBusConnection::connectToBus(bus.address, serviceConnectionName);
  FakeNetworkBackend *backend = nullptr;
  auto service = resident(serviceConnection, serviceName, &backend);
  QCOMPARE(service->start(), NetworkServiceStartStatus::Started);
  backend->publish(readyNetworkObservation());

  QtNetworkTransport transport(bus.client, serviceName);
  QSignalSpy owners(&transport, &NetworkTransport::ownerChanged);
  QSignalSpy failures(&transport, &NetworkTransport::requestFailed);
  QVERIFY(transport.start());
  QTRY_VERIFY_WITH_TIMEOUT(!owners.isEmpty(), 5'000);
  const QString owner = owners.constLast().at(0).toString();
  QVariantMap parameters;
  parameters.insert(QStringLiteral("knownNetworkId"),
                    readyNetworkObservation().knownNetworks.first().id);
  parameters.insert(QStringLiteral("password"), QStringLiteral("do-not-send"));
  transport.requestOperation(9, owner, service->coordinator()->snapshot().epoch,
                             service->coordinator()->snapshot().revision,
                             OperationKind::ConnectKnownNetwork, parameters);
  QCOMPARE(failures.size(), 1);
  QCOMPARE(failures.first().at(2).toString(),
           QStringLiteral("operation-parameters-contain-secrets"));
  QVERIFY(!failures.first().at(3).toString().contains(
      QStringLiteral("do-not-send")));
  QCOMPARE(backend->calls.size(), 0);
  service->stop();
  QDBusConnection::disconnectFromBus(serviceConnectionName);
}

void QtNetworkTransportTests::invalidConnectionFailsAtomically() {
  QDBusConnection invalid(QStringLiteral("missing-network-test-bus"));
  QtNetworkTransport transport(invalid);
  QString error;
  QVERIFY(!transport.start(&error));
  QVERIFY(!error.isEmpty());
  transport.stop();
}

QTEST_GUILESS_MAIN(QtNetworkTransportTests)
#include "tst_qt_network_transport.moc"
