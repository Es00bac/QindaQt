// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-header consumer for the Network N0 boundary. It links only the
// staged public libraries and exercises the documented compile-and-run
// surface: identity normalization, canonical codec round trip, model gating,
// lease truth, and client construction over a minimal transport. A crash or
// wrong value exits nonzero.

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_client/network_transport.h>
#include <qindaqt/services/network_model/network_model.h>
#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_protocol/network_codec.h>
#include <qindaqt/services/network_protocol/network_identity.h>
#include <qindaqt/services/network_protocol/network_validation.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <cstdio>
#include <memory>

using namespace QindaQt::Network;

namespace {

class NullTransport final : public Client::NetworkTransport {
public:
  bool start(QString *error = nullptr) override {
    if (error != nullptr) {
      *error = QString();
    }
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64, const QString &) override {}
  void requestOperation(quint64, const QString &, quint64, quint64,
                        OperationKind, const QVariantMap &) override {}
};

int fail(const char *what) {
  std::fprintf(stderr, "network installed consumer failed: %s\n", what);
  return 1;
}

} // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);

  const SsidIdentity cafe = normalizeSsid(QByteArray("Cafe"));
  if (!cafe.valid || cafe.hidden || cafe.text != QStringLiteral("Cafe")) {
    return fail("ssid normalization");
  }
  const QString networkId =
      knownNetworkId(QByteArray("Cafe"), SecuritySuite::Wpa2Personal);
  if (networkId.size() != 64) {
    return fail("known-network id derivation");
  }

  Snapshot snapshot;
  snapshot.owner = QStringLiteral(":1.9");
  snapshot.epoch = 3;
  snapshot.revision = 1;
  snapshot.availability = Availability::Ready;
  snapshot.capabilities = Capability::Connectivity | Capability::Scan
                          | Capability::KnownNetworkControl
                          | Capability::RadioControl;
  snapshot.connectivity = ConnectivityKind::Portal;
  snapshot.radios = {Radio{RadioKind::Wifi, true, true, true}};
  snapshot.devices = {
      Device{QStringLiteral("wlan0"), DeviceKind::Wifi, DeviceState::Disconnected}};
  snapshot.knownNetworks = {KnownNetwork{networkId, QStringLiteral("Cafe"),
                                         false, SecuritySuite::Wpa2Personal,
                                         true}};
  if (!validateSnapshot(snapshot).accepted) {
    return fail("snapshot validation");
  }

  const EncodeResult encoded = encodeSnapshot(snapshot);
  if (!encoded.succeeded()) {
    return fail("snapshot encoding");
  }
  Snapshot decoded;
  if (!decodeSnapshot(encoded.payload, decoded).succeeded() || decoded != snapshot) {
    return fail("snapshot codec round trip");
  }

  Model::NetworkModel model;
  if (!model.applySnapshot(snapshot).accepted) {
    return fail("model accepted snapshot");
  }
  const Model::ModelState state = model.projection();
  if (!state.hasSnapshot || state.connectivity != ConnectivityKind::Portal
      || !state.scanCapable) {
    return fail("model projection");
  }
  if (!model.requestScan(RequestScanIntent{30'000}).allowed) {
    return fail("scan intent admission");
  }
  const Model::ModelState busy = model.projection(true);
  if (!busy.scanBusy) {
    return fail("busy projection");
  }

  NullTransport transport;
  Client::NetworkClient client(transport);
  if (client.state() != Client::ClientState::Unavailable) {
    return fail("client initial state");
  }
  if (!client.start()) {
    return fail("client start");
  }
  client.stop();

  QTimer::singleShot(0, &application, &QCoreApplication::quit);
  return QCoreApplication::exec();
}
