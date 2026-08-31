// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_manager_adapter/network_manager_port.h>

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include <NetworkManager.h>

namespace QindaQt::Network::NetworkManager {

struct ScanLeaseState final {
  void begin(qint64 deadline) noexcept {
    deadlineMilliseconds = deadline;
    inProgress = true;
  }

  void complete(bool succeeded, bool cancelled) noexcept {
    inProgress = false;
    if (!succeeded && !cancelled) {
      // AGENT-GUARD: A definite libnm failure did not mint freshness. A
      // cancellation is uncertain because NetworkManager may already have
      // accepted the scan, so only that path conservatively keeps the lease.
      deadlineMilliseconds = 0;
    }
  }

  void applyTo(Facts &facts, qint64 nowMilliseconds) noexcept;

  qint64 deadlineMilliseconds = 0;
  bool inProgress = false;
};

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
  struct OwnerWatchState;

  static void scanFinished(GObject *source, GAsyncResult *result,
                           gpointer userData);
  static void activationFinished(GObject *source, GAsyncResult *result,
                                 gpointer userData);
  static void deactivationFinished(GObject *source, GAsyncResult *result,
                                   gpointer userData);
  static void radioFinished(GObject *source, GAsyncResult *result,
                            gpointer userData);
  static void ownerChanged(GObject *source, GParamSpec *property,
                           gpointer userData);
  static void destroyOwnerWatch(gpointer userData, GClosure *closure);

  void poll();
  [[nodiscard]] bool ensureClient();
  [[nodiscard]] bool observeAuthorityOwner(NMClient *client,
                                           quint64 generation);
  void disconnectOwnerWatch();
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
  gulong m_ownerNotifyHandler = 0;
  QHash<quint64, GCancellable *> m_pending;
  QString m_observedOwner;
  ScanLeaseState m_scanLease;
  quint64 m_runGeneration = 0;
  bool m_running = false;
  bool m_authorityRetired = false;
};

} // namespace QindaQt::Network::NetworkManager
