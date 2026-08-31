// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_client/network_transport.h>

#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Network::Client {

// Exact-owner Qt D-Bus implementation of the N0 NetworkTransport seam.
// Retains a value handle to the named connection, which must stay registered
// for this object's lifetime. All methods and signals are confined to the
// constructing Qt thread. D-Bus failures become bounded public reason codes;
// raw broker/service messages never cross into NetworkClient diagnostics.
class QtNetworkTransport final : public NetworkTransport {
  Q_OBJECT

public:
  explicit QtNetworkTransport(const QDBusConnection &connection,
                              QString serviceName = {},
                              QObject *parent = nullptr);
  ~QtNetworkTransport() override;

  [[nodiscard]] bool start(QString *error = nullptr) override;
  void stop() override;
  void requestSnapshot(quint64 token, const QString &owner) override;
  void requestOperation(quint64 token, const QString &owner, quint64 epoch,
                        quint64 revision, OperationKind kind,
                        const QVariantMap &parameters) override;

private Q_SLOTS:
  void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                             const QString &newOwner);
  void onChanged(quint64 epoch, quint64 revision);
  void onBusDisconnected();

private:
  void queryInitialOwner();
  void requestActivation();
  void setOwner(const QString &owner);
  void fail(quint64 token, const QString &owner, const QString &reason);

  class Private;
  std::unique_ptr<Private> d;
};

} // namespace QindaQt::Network::Client
