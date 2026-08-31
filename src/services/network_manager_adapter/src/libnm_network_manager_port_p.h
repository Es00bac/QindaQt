// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_manager_adapter/network_manager_port.h>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <NetworkManager.h>

namespace QindaQt::Network::NetworkManager {

class LibnmNetworkManagerPort final : public NetworkManagerPort {
  Q_OBJECT

public:
  explicit LibnmNetworkManagerPort(QObject *parent = nullptr);
  ~LibnmNetworkManagerPort() override;

  [[nodiscard]] bool start() override;
  void stop() override;
  void submit(quint64 operationId,
              const Service::BackendOperationRequest &request) override;
  void cancel(quint64 operationId) override;

private:
  struct CallbackState;

  static void scanFinished(GObject *source, GAsyncResult *result,
                           gpointer userData);
  static void activationFinished(GObject *source, GAsyncResult *result,
                                 gpointer userData);
  static void deactivationFinished(GObject *source, GAsyncResult *result,
                                   gpointer userData);
  static void radioFinished(GObject *source, GAsyncResult *result,
                            gpointer userData);

  void poll();
  [[nodiscard]] bool ensureClient();
  [[nodiscard]] Facts collectFacts();
  [[nodiscard]] GCancellable *beginAsync(quint64 operationId);
  void completeAsync(quint64 operationId, bool succeeded,
                     const QString &reason = {});
  void submitScan(quint64 operationId,
                  const Service::BackendOperationRequest &request);
  void submitConnect(quint64 operationId,
                     const Service::BackendOperationRequest &request);
  void submitDisconnect(quint64 operationId,
                        const Service::BackendOperationRequest &request);
  void submitRadio(quint64 operationId,
                   const Service::BackendOperationRequest &request);
  [[nodiscard]] NMRemoteConnection *
  connectionForKnownId(const QString &knownNetworkId) const;
  [[nodiscard]] NMDevice *firstWifiDevice() const;
  [[nodiscard]] NMActiveConnection *
  activeForInterface(const QString &interfaceName) const;
  void publishFacts();

  QTimer m_pollTimer;
  NMClient *m_client = nullptr;
  QHash<quint64, GCancellable *> m_pending;
  QString m_observedOwner;
  qint64 m_scanLeaseDeadline = 0;
  bool m_scanInProgress = false;
  bool m_running = false;
  bool m_authorityRetired = false;
};

} // namespace QindaQt::Network::NetworkManager
