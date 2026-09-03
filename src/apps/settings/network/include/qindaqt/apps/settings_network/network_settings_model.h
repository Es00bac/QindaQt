// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_client/network_client.h>
#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtDBus/QDBusConnection>

#include <memory>

namespace QindaQt::Apps::SettingsNetwork {

class NetworkSecretAgentPresence;

// QML-safe, read-only projection and intent facade for one public Network1
// client. The caller owns the client and must keep it alive on this object's
// Qt thread. This model never starts platform objects, stores credentials, or
// manufactures connectivity state from operation replies.
class NetworkSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(bool reloadAvailable READ reloadAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool scanAvailable READ scanAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool credentialEntrySupported READ credentialEntrySupported
                 CONSTANT)
  Q_PROPERTY(bool secretAgentRegistered READ secretAgentRegistered NOTIFY
                 viewChanged)
  Q_PROPERTY(QString secretAgentStatusText READ secretAgentStatusText NOTIFY
                 viewChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText READ operationStatusText NOTIFY
                 viewChanged)
  Q_PROPERTY(QString serviceOwner READ serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch READ serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision READ serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QString connectivityText READ connectivityText NOTIFY viewChanged)
  Q_PROPERTY(QString scanStatusText READ scanStatusText NOTIFY viewChanged)
  Q_PROPERTY(QVariantList radios READ radios NOTIFY viewChanged)
  Q_PROPERTY(QVariantList devices READ devices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList accessPoints READ accessPoints NOTIFY viewChanged)
  Q_PROPERTY(QVariantList knownNetworks READ knownNetworks NOTIFY viewChanged)

public:
  explicit NetworkSettingsModel(
      QindaQt::Network::Client::NetworkClient &client,
      QObject *parent = nullptr);
  NetworkSettingsModel(QindaQt::Network::Client::NetworkClient &client,
                       const QDBusConnection &presenceConnection,
                       QObject *parent = nullptr);
  ~NetworkSettingsModel() override;

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool stale() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool reloadAvailable() const noexcept;
  [[nodiscard]] bool scanAvailable() const;
  [[nodiscard]] constexpr bool credentialEntrySupported() const noexcept {
    return false;
  }
  [[nodiscard]] bool secretAgentRegistered() const noexcept;
  [[nodiscard]] QString secretAgentStatusText() const;

  [[nodiscard]] QString statusText() const;
  [[nodiscard]] QString errorText() const;
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }
  [[nodiscard]] QString serviceOwner() const;
  [[nodiscard]] qulonglong serviceEpoch() const;
  [[nodiscard]] qulonglong serviceRevision() const;
  [[nodiscard]] QString connectivityText() const;
  [[nodiscard]] QString scanStatusText() const;
  [[nodiscard]] QVariantList radios() const;
  [[nodiscard]] QVariantList devices() const;
  [[nodiscard]] QVariantList accessPoints() const;
  [[nodiscard]] QVariantList knownNetworks() const;

  Q_INVOKABLE bool reload();
  Q_INVOKABLE bool requestScan();
  Q_INVOKABLE bool connectKnownNetwork(const QString &knownNetworkId);
  Q_INVOKABLE bool connectVisibleNetwork(const QString &accessPointId);
  Q_INVOKABLE bool disconnectDevice(const QString &deviceInterface);

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  void handleOperationFinished(
      const QindaQt::Network::OperationResult &result);
  void handleOperationUncertain(const QString &message);
  void beginOperationMessage(QindaQt::Network::OperationKind kind);
  void rejectAction(const QString &reason);
  [[nodiscard]] QString actionFailureText(const QString &reason) const;

  QindaQt::Network::Client::NetworkClient &m_client;
  std::unique_ptr<NetworkSecretAgentPresence> m_secretAgentPresence;
  QString m_localError;
  QString m_operationStatusText;
};

} // namespace QindaQt::Apps::SettingsNetwork
