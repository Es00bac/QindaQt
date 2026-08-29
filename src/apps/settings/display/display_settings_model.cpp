// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_settings_model.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QUuid>
#include <algorithm>

namespace QindaQt::Apps::SettingsDisplay {

DisplaySettingsModel::DisplaySettingsModel(
    QindaQt::DisplayClient::Client &client,
    QindaQt::DisplayClient::Coordinator &coordinator,
    QObject *parent)
    : QObject(parent), m_client(client), m_coordinator(coordinator) {
  connect(&m_client, &QindaQt::DisplayClient::Client::stateChanged, this,
          &DisplaySettingsModel::onClientStateChanged);
  connect(&m_client, &QindaQt::DisplayClient::Client::snapshotChanged, this,
          &DisplaySettingsModel::onSnapshotChanged);

  connect(&m_coordinator, &QindaQt::DisplayClient::Coordinator::stateChanged,
          this, &DisplaySettingsModel::onCoordinatorStateChanged);
  connect(&m_coordinator,
          &QindaQt::DisplayClient::Coordinator::transactionFinished, this,
          &DisplaySettingsModel::onTransactionFinished);

  m_countdownTimer.setInterval(1'000);
  m_countdownTimer.setSingleShot(false);
  connect(&m_countdownTimer, &QTimer::timeout, this,
          &DisplaySettingsModel::onCountdownTick);

  if (m_client.hasSnapshot()) {
    if (const auto opt = m_client.snapshot(); opt.has_value()) {
      onSnapshotChanged(*opt);
    }
  }
}

DisplaySettingsModel::~DisplaySettingsModel() {
  m_countdownTimer.stop();
}

bool DisplaySettingsModel::loading() const noexcept {
  return m_client.state() == QindaQt::DisplayClient::ClientState::Starting;
}

bool DisplaySettingsModel::ready() const noexcept {
  return m_client.state() == QindaQt::DisplayClient::ClientState::Ready;
}

bool DisplaySettingsModel::busy() const noexcept {
  return m_client.state() == QindaQt::DisplayClient::ClientState::Busy ||
         inTransaction();
}

bool DisplaySettingsModel::unavailable() const noexcept {
  return m_client.state() == QindaQt::DisplayClient::ClientState::Unavailable ||
         m_client.state() == QindaQt::DisplayClient::ClientState::Stopped;
}

bool DisplaySettingsModel::degraded() const noexcept {
  return m_client.state() == QindaQt::DisplayClient::ClientState::Degraded;
}

bool DisplaySettingsModel::canEdit() const noexcept {
  return (m_client.state() == QindaQt::DisplayClient::ClientState::Ready ||
          (m_client.state() == QindaQt::DisplayClient::ClientState::Degraded &&
           m_snapshot.has_value())) &&
         !inTransaction();
}

QString DisplaySettingsModel::statusText() const {
  if (!m_statusText.isEmpty()) {
    return m_statusText;
  }
  if (unavailable()) {
    return QCoreApplication::translate("DisplaySettings",
                                       "Display service unavailable.");
  }
  if (degraded()) {
    return QCoreApplication::translate("DisplaySettings",
                                       "Display service operating in degraded state.");
  }
  if (loading()) {
    return QCoreApplication::translate("DisplaySettings",
                                       "Connecting to display service...");
  }
  return {};
}

QString DisplaySettingsModel::errorText() const {
  if (!m_transientError.isEmpty()) {
    return m_transientError;
  }
  if (!m_errorText.isEmpty()) {
    return m_errorText;
  }
  if (!m_draftValid && !m_draftErrorMessage.isEmpty()) {
    return m_draftErrorMessage;
  }
  return {};
}

QVariantList DisplaySettingsModel::outputs() const {
  QVariantList list;
  list.reserve(m_draftOutputs.size());
  for (const auto &out : m_draftOutputs) {
    list.append(out.toVariantMap());
  }
  return list;
}

QString DisplaySettingsModel::selectedOutputId() const {
  return m_selectedOutputId;
}

QVariantMap DisplaySettingsModel::selectedOutput() const {
  const auto *out = findDraftOutput(m_selectedOutputId);
  if (out != nullptr) {
    return out->toVariantMap();
  }
  return {};
}

bool DisplaySettingsModel::draftDirty() const noexcept {
  return m_draftDirty;
}

bool DisplaySettingsModel::draftValid() const noexcept {
  return m_draftValid;
}

QString DisplaySettingsModel::draftErrorMessage() const {
  return m_draftErrorMessage;
}

QVariantMap DisplaySettingsModel::fieldErrors() const {
  return m_fieldErrors;
}

QVariantList DisplaySettingsModel::warnings() const {
  return m_warnings;
}

bool DisplaySettingsModel::applyAvailable() const {
  return canEdit() && m_draftDirty && m_draftValid && !inTransaction() &&
         m_client.state() == QindaQt::DisplayClient::ClientState::Ready;
}

bool DisplaySettingsModel::inTransaction() const noexcept {
  const auto state = m_coordinator.state();
  return state == QindaQt::DisplayClient::CoordinatorState::Staging ||
         state == QindaQt::DisplayClient::CoordinatorState::Previewing ||
         state == QindaQt::DisplayClient::CoordinatorState::AwaitingConfirmation ||
         state == QindaQt::DisplayClient::CoordinatorState::Confirming ||
         state == QindaQt::DisplayClient::CoordinatorState::Cancelling;
}

bool DisplaySettingsModel::awaitingConfirmation() const noexcept {
  return m_coordinator.state() ==
         QindaQt::DisplayClient::CoordinatorState::AwaitingConfirmation;
}

int DisplaySettingsModel::transactionRemainingSeconds() const noexcept {
  return m_transactionRemainingSeconds;
}

QString DisplaySettingsModel::transactionStatusText() const {
  return m_transactionStatusText;
}

QString DisplaySettingsModel::activeTransactionId() const {
  return m_activeTransactionId;
}

void DisplaySettingsModel::setSelectedOutputId(const QString &stableId) {
  if (m_selectedOutputId == stableId) {
    return;
  }
  if (findDraftOutput(stableId) == nullptr) {
    return;
  }
  m_selectedOutputId = stableId;
  Q_EMIT selectedOutputIdChanged(m_selectedOutputId);
  Q_EMIT selectedOutputChanged();
}



bool DisplaySettingsModel::applyDraft() {
  if (!applyAvailable()) {
    return false;
  }
  if (!m_snapshot.has_value()) {
    return false;
  }

  const Display::Candidate candidate = buildCandidateFromDraft();
  const auto validation =
      DisplayTopology::validateAndNormalize(*m_snapshot, candidate);
  if (!validation.accepted() || validation.noOp) {
    return false;
  }

  const QString transactionId = QStringLiteral("settings-display-%1")
                                    .arg(QUuid::createUuid().toString(QUuid::Id128));
  m_activeTransactionId = transactionId;
  const bool ok =
      m_coordinator.begin(transactionId, validation.normalizedCandidate);
  if (!ok) {
    m_activeTransactionId.clear();
    m_transientError = QCoreApplication::translate(
        "DisplaySettings", "Failed to start display configuration transaction.");
    Q_EMIT stateChanged();
    return false;
  }

  m_transactionStatusText = QCoreApplication::translate(
      "DisplaySettings", "Applying display configuration preview...");
  m_transientError.clear();
  Q_EMIT transactionChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::confirmTransaction() {
  if (!awaitingConfirmation()) {
    return false;
  }
  return m_coordinator.confirm();
}

bool DisplaySettingsModel::revertTransaction() {
  if (!inTransaction()) {
    return false;
  }
  return m_coordinator.cancel();
}

void DisplaySettingsModel::retry() {
  m_transientError.clear();
  m_errorText.clear();
  m_client.refresh();
}

void DisplaySettingsModel::onClientStateChanged(
    QindaQt::DisplayClient::ClientState state, const QString &reasonCode) {
  Q_UNUSED(reasonCode);
  if (state == QindaQt::DisplayClient::ClientState::Unavailable ||
      state == QindaQt::DisplayClient::ClientState::Stopped) {
    m_snapshot.reset();
    m_draftOutputs.clear();
    m_selectedOutputId.clear();
    m_draftDirty = false;
    m_draftValid = true;
    m_draftErrorMessage.clear();
    m_fieldErrors.clear();
    m_warnings.clear();
    m_activeTransactionId.clear();
    m_countdownTimer.stop();
    m_transactionRemainingSeconds = 0;
    Q_EMIT outputsChanged();
    Q_EMIT selectedOutputIdChanged(m_selectedOutputId);
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT transactionChanged();
  }
  Q_EMIT stateChanged();
}

void DisplaySettingsModel::onSnapshotChanged(
    const QindaQt::Display::Snapshot &snapshot) {
  m_snapshot = snapshot;
  if (!m_draftDirty && !inTransaction()) {
    resetDraftToSnapshot();
  } else {
    // Revalidate existing draft against updated snapshot
    validateDraft();
  }

  updateCountdownFromSnapshot();

  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
}

void DisplaySettingsModel::onCoordinatorStateChanged(
    QindaQt::DisplayClient::CoordinatorState state, const QString &reasonCode) {
  Q_UNUSED(reasonCode);
  switch (state) {
  case QindaQt::DisplayClient::CoordinatorState::Idle:
    m_countdownTimer.stop();
    m_transactionRemainingSeconds = 0;
    break;
  case QindaQt::DisplayClient::CoordinatorState::Unavailable:
    m_countdownTimer.stop();
    m_transactionRemainingSeconds = 0;
    break;
  case QindaQt::DisplayClient::CoordinatorState::Staging:
    m_transactionStatusText = QCoreApplication::translate(
        "DisplaySettings", "Staging display configuration...");
    break;
  case QindaQt::DisplayClient::CoordinatorState::Previewing:
    m_transactionStatusText = QCoreApplication::translate(
        "DisplaySettings", "Applying display preview...");
    break;
  case QindaQt::DisplayClient::CoordinatorState::AwaitingConfirmation:
    if (m_transactionRemainingSeconds <= 0) {
      m_transactionRemainingSeconds = 15;
    }
    m_transactionStatusText =
        QCoreApplication::translate(
            "DisplaySettings",
            "Preview active. Reverting in %1 seconds if not confirmed.")
            .arg(m_transactionRemainingSeconds);
    m_countdownTimer.start();
    break;
  case QindaQt::DisplayClient::CoordinatorState::Confirming:
    m_transactionStatusText = QCoreApplication::translate(
        "DisplaySettings", "Confirming display configuration...");
    break;
  case QindaQt::DisplayClient::CoordinatorState::Cancelling:
    m_transactionStatusText = QCoreApplication::translate(
        "DisplaySettings", "Reverting display configuration...");
    break;
  case QindaQt::DisplayClient::CoordinatorState::Confirmed:
  case QindaQt::DisplayClient::CoordinatorState::Reverted:
  case QindaQt::DisplayClient::CoordinatorState::Uncertain:
  case QindaQt::DisplayClient::CoordinatorState::NoOp:
    m_countdownTimer.stop();
    m_transactionRemainingSeconds = 0;
    break;
  }

  Q_EMIT transactionChanged();
  Q_EMIT stateChanged();
}

void DisplaySettingsModel::onTransactionFinished(
    const QString &transactionId,
    QindaQt::DisplayClient::CoordinatorOutcome outcome,
    const QString &reasonCode) {
  Q_UNUSED(transactionId);
  m_activeTransactionId.clear();
  m_countdownTimer.stop();
  m_transactionRemainingSeconds = 0;

  switch (outcome) {
  case QindaQt::DisplayClient::CoordinatorOutcome::Confirmed:
    m_statusText = QCoreApplication::translate(
        "DisplaySettings", "Display settings applied successfully.");
    m_transientError.clear();
    resetDraftToSnapshot();
    break;
  case QindaQt::DisplayClient::CoordinatorOutcome::Reverted:
    m_statusText = QCoreApplication::translate(
        "DisplaySettings",
        "Display settings reverted to previous configuration.");
    m_transientError.clear();
    resetDraftToSnapshot();
    break;
  case QindaQt::DisplayClient::CoordinatorOutcome::NoOp:
    m_statusText = QCoreApplication::translate("DisplaySettings",
                                               "No changes to apply.");
    m_transientError.clear();
    resetDraftToSnapshot();
    break;
  case QindaQt::DisplayClient::CoordinatorOutcome::Uncertain:
    m_statusText.clear();
    m_transientError =
        QCoreApplication::translate(
            "DisplaySettings", "Display transaction outcome uncertain: %1")
            .arg(reasonCode.isEmpty()
                     ? QStringLiteral("service unavailable")
                     : reasonCode);
    resetDraftToSnapshot();
    break;
  }

  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT transactionChanged();
  Q_EMIT stateChanged();
}

void DisplaySettingsModel::onCountdownTick() {
  if (m_transactionRemainingSeconds > 0) {
    --m_transactionRemainingSeconds;
    m_transactionStatusText =
        QCoreApplication::translate(
            "DisplaySettings",
            "Preview active. Reverting in %1 seconds if not confirmed.")
            .arg(m_transactionRemainingSeconds);
    Q_EMIT transactionCountdownChanged();
    Q_EMIT transactionChanged();
  }
}

void DisplaySettingsModel::updateCountdownFromSnapshot() {
  if (!m_snapshot.has_value()) {
    return;
  }
  for (const auto &tx : m_snapshot->transactions) {
    if (tx.state == Display::TransactionState::AwaitingConfirmation) {
      if (m_transactionRemainingSeconds <= 0) {
        m_transactionRemainingSeconds = 15;
      }
      break;
    }
  }
}

} // namespace QindaQt::Apps::SettingsDisplay
