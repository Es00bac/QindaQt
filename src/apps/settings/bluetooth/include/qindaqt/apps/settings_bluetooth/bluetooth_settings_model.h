// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/bluetooth_client/bluetooth_client.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <optional>

namespace QindaQt::Apps::SettingsBluetooth {

// Route-owned, QML-safe projection of one same-thread public BluetoothClient.
// The caller owns the client and must keep it alive longer than this object.
// All values are copies; neither Bluetooth addresses nor transport objects
// cross this boundary.
//
// AGENT-CONTRACT: One admission predicate supplies both displayed availability
// and dispatch. A request pins exact owner/epoch/revision, and a successful
// reply keeps all controls fenced until an authoritative snapshot converges.
// This model owns at most one caller-scoped discovery lease; route departure
// requests one serialized release and never automatically replays uncertainty.
class BluetoothSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(bool routeActive READ routeActive NOTIFY viewChanged)
  Q_PROPERTY(bool discoveryLeaseHeld READ discoveryLeaseHeld NOTIFY viewChanged)
  Q_PROPERTY(bool departureReleasePending READ departureReleasePending NOTIFY viewChanged)
  Q_PROPERTY(bool pairingSupported READ pairingSupported CONSTANT)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText READ operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(QString serviceOwner READ serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch READ serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision READ serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QVariantList adapters READ adapters NOTIFY viewChanged)
  Q_PROPERTY(QVariantList devices READ devices NOTIFY viewChanged)

public:
  explicit BluetoothSettingsModel(
      QindaQt::Bluetooth::BluetoothClient &client,
      QObject *parent = nullptr);
  ~BluetoothSettingsModel() override;

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool routeActive() const noexcept { return m_routeActive; }
  [[nodiscard]] bool departureReleasePending() const noexcept;
  [[nodiscard]] bool discoveryLeaseHeld() const noexcept {
    return m_discoveryLease.has_value();
  }
  [[nodiscard]] constexpr bool pairingSupported() const noexcept {
    return false;
  }
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }
  [[nodiscard]] QString serviceOwner() const;
  [[nodiscard]] qulonglong serviceEpoch() const;
  [[nodiscard]] qulonglong serviceRevision() const;
  [[nodiscard]] QVariantList adapters() const;
  [[nodiscard]] QVariantList devices() const;

  Q_INVOKABLE void setRouteActive(bool active);
  Q_INVOKABLE bool requestAdapterPower(const QString &adapterId, bool powered);
  Q_INVOKABLE bool requestDiscovery(const QString &adapterId, bool enabled);
  Q_INVOKABLE bool requestDeviceConnection(const QString &deviceId,
                                           bool connected);

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  struct PendingOperation {
    quint64 requestId = 0;
    QString owner;
    QindaQt::Bluetooth::OperationRequest request;
    quint64 epoch = 0;
    quint64 revision = 0;
  };
  struct SuccessConvergence {
    QString owner;
    quint64 epoch = 0;
    quint64 minimumRevision = 0;
  };

  [[nodiscard]] bool exactSnapshotReady() const noexcept;
  [[nodiscard]] QString admissionReason(
      const QindaQt::Bluetooth::OperationRequest &request) const;
  [[nodiscard]] bool dispatch(
      const QindaQt::Bluetooth::OperationRequest &request);
  [[nodiscard]] std::optional<QindaQt::Bluetooth::Adapter>
  findAdapter(const QString &rowId) const;
  [[nodiscard]] std::optional<QindaQt::Bluetooth::Device>
  findDevice(const QString &rowId) const;
  void handleOperationCompleted(
      quint64 requestId,
      const QindaQt::Bluetooth::OperationResult &result);
  void synchronizeAuthority();
  void retireLeaseFromCurrentTruth();
  void tryAutomaticRelease();
  void reject(const QString &reason);
  [[nodiscard]] QString failureText(
      const QindaQt::Bluetooth::OperationResult &result) const;

  QindaQt::Bluetooth::BluetoothClient &m_client;
  bool m_routeActive = false;
  bool m_releaseRequested = false;
  bool m_automaticReleaseBlocked = false;
  std::optional<PendingOperation> m_pending;
  std::optional<SuccessConvergence> m_convergence;
  std::optional<QindaQt::Bluetooth::Handle> m_discoveryLease;
  QString m_discoveryLeaseOwner;
  quint64 m_discoveryLeaseMinimumRevision = 0;
  QString m_errorText;
  QString m_operationStatusText;
};

} // namespace QindaQt::Apps::SettingsBluetooth
