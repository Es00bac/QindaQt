// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_display/display_settings_values.h>
#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/display_coordinator.h>
#include <qindaqt/services/display_protocol/display_types.h>
#include <qindaqt/services/display_topology/topology.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <optional>

namespace QindaQt::Apps::SettingsDisplay {

class DisplaySettingsModel final : public QObject {
  Q_OBJECT

  // State & liveness
  Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY stateChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY stateChanged)
  Q_PROPERTY(bool canEdit READ canEdit NOTIFY stateChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)

  // Output inventory & selection
  Q_PROPERTY(QVariantList outputs READ outputs NOTIFY outputsChanged)
  Q_PROPERTY(QString selectedOutputId READ selectedOutputId WRITE setSelectedOutputId NOTIFY selectedOutputIdChanged)
  Q_PROPERTY(QVariantMap selectedOutput READ selectedOutput NOTIFY selectedOutputChanged)

  // Draft state & validation
  Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
  Q_PROPERTY(bool draftValid READ draftValid NOTIFY draftChanged)
  Q_PROPERTY(QString draftErrorMessage READ draftErrorMessage NOTIFY draftChanged)
  Q_PROPERTY(QVariantMap fieldErrors READ fieldErrors NOTIFY draftChanged)
  Q_PROPERTY(QVariantList warnings READ warnings NOTIFY draftChanged)
  Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY stateChanged)

  // Reversible transaction & preview
  Q_PROPERTY(bool inTransaction READ inTransaction NOTIFY transactionChanged)
  Q_PROPERTY(bool awaitingConfirmation READ awaitingConfirmation NOTIFY transactionChanged)
  Q_PROPERTY(int transactionRemainingSeconds READ transactionRemainingSeconds NOTIFY transactionCountdownChanged)
  Q_PROPERTY(QString transactionStatusText READ transactionStatusText NOTIFY transactionChanged)
  Q_PROPERTY(QString activeTransactionId READ activeTransactionId NOTIFY transactionChanged)

public:
  explicit DisplaySettingsModel(
      QindaQt::DisplayClient::Client &client,
      QindaQt::DisplayClient::Coordinator &coordinator,
      QObject *parent = nullptr);
  ~DisplaySettingsModel() override;

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool canEdit() const noexcept;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] QString errorText() const;

  [[nodiscard]] QVariantList outputs() const;
  [[nodiscard]] QString selectedOutputId() const;
  [[nodiscard]] QVariantMap selectedOutput() const;

  [[nodiscard]] bool draftDirty() const noexcept;
  [[nodiscard]] bool draftValid() const noexcept;
  [[nodiscard]] QString draftErrorMessage() const;
  [[nodiscard]] QVariantMap fieldErrors() const;
  [[nodiscard]] QVariantList warnings() const;
  [[nodiscard]] bool applyAvailable() const;

  [[nodiscard]] bool inTransaction() const noexcept;
  [[nodiscard]] bool awaitingConfirmation() const noexcept;
  [[nodiscard]] int transactionRemainingSeconds() const noexcept;
  [[nodiscard]] QString transactionStatusText() const;
  [[nodiscard]] QString activeTransactionId() const;

  // Invokables
  Q_INVOKABLE void setSelectedOutputId(const QString &stableId);
  Q_INVOKABLE bool setOutputEnabled(const QString &stableId, bool enabled);
  Q_INVOKABLE bool setOutputPrimary(const QString &stableId);
  Q_INVOKABLE bool setOutputMode(const QString &stableId, const QString &modeId);
  Q_INVOKABLE bool setOutputScale(const QString &stableId, double scale);
  Q_INVOKABLE bool setOutputTransform(const QString &stableId, const QString &transformStr);
  Q_INVOKABLE bool setOutputPosition(const QString &stableId, int x, int y);

  Q_INVOKABLE bool cancelDraft();
  Q_INVOKABLE bool applyDraft();
  Q_INVOKABLE bool confirmTransaction();
  Q_INVOKABLE bool revertTransaction();
  Q_INVOKABLE void retry();

Q_SIGNALS:
  void stateChanged();
  void outputsChanged();
  void selectedOutputIdChanged(const QString &selectedOutputId);
  void selectedOutputChanged();
  void draftChanged();
  void transactionChanged();
  void transactionCountdownChanged();

private Q_SLOTS:
  void onClientStateChanged(QindaQt::DisplayClient::ClientState state,
                            const QString &reasonCode);
  void onSnapshotChanged(const QindaQt::Display::Snapshot &snapshot);
  void onCoordinatorStateChanged(QindaQt::DisplayClient::CoordinatorState state,
                                const QString &reasonCode);
  void onTransactionFinished(const QString &transactionId,
                             QindaQt::DisplayClient::CoordinatorOutcome outcome,
                             const QString &reasonCode);
  void onCountdownTick();

private:
  void resetDraftToSnapshot();
  void validateDraft();
  [[nodiscard]] Display::Candidate buildCandidateFromDraft() const;
  [[nodiscard]] OutputDraft *findDraftOutput(const QString &stableId);
  [[nodiscard]] const OutputDraft *findDraftOutput(const QString &stableId) const;
  void updateCountdownFromSnapshot();

  QindaQt::DisplayClient::Client &m_client;
  QindaQt::DisplayClient::Coordinator &m_coordinator;

  std::optional<Display::Snapshot> m_snapshot;
  QList<OutputDraft> m_draftOutputs;
  QString m_selectedOutputId;

  bool m_draftDirty = false;
  bool m_draftValid = true;
  QString m_draftErrorMessage;
  QVariantMap m_fieldErrors;
  QVariantList m_warnings;

  QString m_statusText;
  QString m_errorText;
  QString m_transientError;

  QString m_activeTransactionId;
  QString m_transactionStatusText;
  int m_transactionRemainingSeconds = 0;
  QTimer m_countdownTimer;
};

} // namespace QindaQt::Apps::SettingsDisplay
