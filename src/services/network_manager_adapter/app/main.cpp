// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_manager_adapter/network_manager_backend.h>
#include <qindaqt/services/network_service/resident_network_service.h>

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>

#include <memory>

using namespace QindaQt::Network::NetworkManager;
using namespace QindaQt::Network::Service;

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(
      QStringLiteral("qindaqt-network-service"));
  QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
  QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

  QDBusConnection sessionConnection = QDBusConnection::sessionBus();
  // AGENT-GUARD: This process and its service epoch belong to exactly the
  // constructing session bus. Reconnecting in place would let stale owner
  // lineage cross into a new broker; activation must construct a new process.
  if (!sessionConnection.connect(
          QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
          QStringLiteral("org.freedesktop.DBus.Local"),
          QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
    qCritical("Network1 could not bind constructing-bus lifetime");
    return 1;
  }

  auto backend = std::make_unique<NetworkManagerBackend>();
  ResidentNetworkService service(std::move(backend), sessionConnection);
  QObject::connect(
      &service, &ResidentNetworkService::restartRequired, &application,
      [&application] {
        // A NetworkManager owner replacement invalidates the public epoch.
        // Exit as a failure so both the hardened systemd unit and D-Bus
        // activation create a new Network1 owner rather than reusing it.
        application.exit(75);
      },
      Qt::QueuedConnection);
  const NetworkServiceStartStatus status = service.start();
  if (status != NetworkServiceStartStatus::Started) {
    qCritical("Network1 startup failed with status %u",
              static_cast<unsigned int>(status));
    return 1;
  }
  QObject::connect(&application, &QCoreApplication::aboutToQuit, &service,
                   &ResidentNetworkService::stop);
  return QCoreApplication::exec();
}
