// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_client.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

#include <optional>

namespace QindaQt::Apps::SettingsPower {

// Route-owned projection and closed intent facade for one borrowed,
// same-thread public PowerClient. The caller owns and outlives the model.
// QML receives copied display values, route-local row IDs, and one opaque
// purpose-built session-actions facade, never a transport or platform adapter.
//
// AGENT-CONTRACT: displayed availability and final dispatch share one
// admission predicate pinned to exact owner/epoch/revision truth. One
// operation or debounce may be live; success stays fenced until a matching
// authoritative snapshot converges. Uncertain work is never replayed.
class PowerSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(bool retryAvailable READ retryAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool sessionActionsSupported READ sessionActionsSupported CONSTANT)
  Q_PROPERTY(QObject *sessionActions READ sessionActions CONSTANT)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText READ operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch READ serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision READ serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QVariantList supplyRows READ supplyRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileRows READ profileRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileHoldRows READ profileHoldRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList internalBrightnessRows READ internalBrightnessRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList keyboardBrightnessRows READ keyboardBrightnessRows NOTIFY viewChanged)

public:
  explicit PowerSettingsModel(Power::PowerClient &client,
                              QObject *sessionActions = nullptr,
                              QObject *parent = nullptr);

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool stale() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool retryAvailable() const noexcept;
  [[nodiscard]] bool sessionActionsSupported() const noexcept;
  [[nodiscard]] QObject *sessionActions() const noexcept;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }
  [[nodiscard]] qulonglong serviceEpoch() const;
  [[nodiscard]] qulonglong serviceRevision() const;
  [[nodiscard]] QVariantList supplyRows() const;
  [[nodiscard]] QVariantList profileRows() const;
  [[nodiscard]] QVariantList profileHoldRows() const;
  [[nodiscard]] QVariantList internalBrightnessRows() const;
  [[nodiscard]] QVariantList keyboardBrightnessRows() const;

  Q_INVOKABLE bool retry();
  Q_INVOKABLE bool requestProfile(const QString &profileId);
  Q_INVOKABLE bool requestKeyboardBrightness(const QString &rowId,
                                             int normalized);

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  enum class Intent { Profile, KeyboardBrightness };
  struct DebouncedBrightness {
    QString rowId;
    int normalized = 0;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
  };
  struct PendingOperation {
    quint64 requestId = 0;
    Intent intent = Intent::Profile;
    QString target;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
    quint32 expectedRaw = 0;
  };
  struct Convergence {
    Intent intent = Intent::Profile;
    QString target;
    QString owner;
    quint64 epoch = 0;
    quint64 minimumRevision = 0;
    quint32 expectedRaw = 0;
  };

  [[nodiscard]] bool hasDisplaySnapshot() const noexcept;
  [[nodiscard]] bool snapshotAdmitsBase() const noexcept;
  [[nodiscard]] QString profileAdmission(const QString &profileId) const;
  [[nodiscard]] QString brightnessAdmission(const QString &rowId,
                                            bool replacingDebounce) const;
  [[nodiscard]] const Power::KeyboardBacklight *findKeyboard(
      const Power::Snapshot &snapshot, const QString &rowId) const;
  void dispatchDebouncedBrightness();
  void handleOperationCompleted(quint64 requestId,
                                const Power::OperationResult &result);
  void synchronizeAuthority();
  void reject(const QString &reason);
  [[nodiscard]] QString failureText(const QString &reason) const;

  Power::PowerClient &m_client;
  QObject *m_sessionActions = nullptr;
  QTimer m_debounceTimer;
  QTimer m_convergenceTimer;
  std::optional<DebouncedBrightness> m_debounce;
  std::optional<PendingOperation> m_pending;
  std::optional<Convergence> m_convergence;
  bool m_retrying = false;
  QString m_errorText;
  QString m_operationStatusText;
};

} // namespace QindaQt::Apps::SettingsPower
