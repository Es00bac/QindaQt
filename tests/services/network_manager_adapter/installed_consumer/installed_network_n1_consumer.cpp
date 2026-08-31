// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_manager_adapter/network_manager_backend.h>
#include <qindaqt/services/network_qt_transport/qt_network_transport.h>

#include <QtCore/QCoreApplication>

#include <memory>

using namespace QindaQt::Network;
using namespace QindaQt::Network::Client;
using namespace QindaQt::Network::NetworkManager;
using namespace QindaQt::Network::Service;

namespace {

class InstalledFakePort final : public NetworkManagerPort {
public:
  bool start() override { return true; }
  void stop() override {}
  void submit(quint64 operationId,
              const BackendOperationRequest &request) override {
    Q_UNUSED(operationId)
    Q_UNUSED(request)
  }
  void cancel(quint64 operationId) override { Q_UNUSED(operationId) }
};

} // namespace

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  auto port = std::make_unique<InstalledFakePort>();
  NetworkManagerBackend backend(std::move(port));
  const quint64 generation = backend.start();
  backend.stop();

  QDBusConnection missing(QStringLiteral("installed-network-missing-bus"));
  QtNetworkTransport transport(missing);
  QString error;
  const bool unexpectedlyStarted = transport.start(&error);
  transport.stop();
  return generation != 0 && !unexpectedlyStarted && !error.isEmpty() ? 0 : 1;
}
