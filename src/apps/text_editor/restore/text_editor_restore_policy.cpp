// SPDX-License-Identifier: GPL-3.0-or-later
#include "text_editor_restore_policy.h"

#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Apps::TextEditor {

using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::CommitOutcome;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;

QStringList TextEditorKeys::scopedKeys() { return {QString(RestoreDocuments)}; }

TextEditorRestorePolicy::TextEditorRestorePolicy(
    QindaQt::Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client,
          &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
          this, &TextEditorRestorePolicy::handleSnapshot);
  connect(&m_client,
          &QindaQt::Services::SettingsClient::SettingsClient::stateChanged,
          this, &TextEditorRestorePolicy::handleClientState);
  connect(&m_client,
          &QindaQt::Services::SettingsClient::SettingsClient::commitFinished,
          this, &TextEditorRestorePolicy::handleCommit);
  connect(&m_client,
          &QindaQt::Services::SettingsClient::SettingsClient::commitUncertain,
          this, &TextEditorRestorePolicy::handleUncertain);
  if (m_client.snapshot().has_value()) {
    handleSnapshot();
  }
}

bool TextEditorRestorePolicy::requestEnabled(const bool enabled) {
  if (m_writePending || !m_baselineReceived ||
      m_client.state() != ClientState::Ready || m_client.writeInFlight()) {
    return false;
  }
  m_writePending = true;
  m_requested = enabled;
  QString error;
  if (!m_client.setUserValue(QString(TextEditorKeys::RestoreDocuments), enabled,
                             &error)) {
    m_writePending = false;
    emit applyFinished(RestorePolicyResult::Failed, error.left(512));
    return false;
  }
  emit policyChanged();
  return true;
}

void TextEditorRestorePolicy::handleSnapshot() {
  const auto snapshot = m_client.snapshot();
  if (!snapshot.has_value()) {
    return;
  }
  const QVariant value =
      snapshot->values.value(QString(TextEditorKeys::RestoreDocuments));
  if (value.metaType().id() != QMetaType::Bool) {
    resetToDefault();
    if (m_writePending) {
      finish(RestorePolicyResult::Failed,
             QStringLiteral("Settings snapshot has an invalid restore policy"));
    }
    return;
  }
  const bool next = value.toBool();
  const bool changed = !m_baselineReceived || next != m_enabled;
  m_enabled = next;
  m_baselineReceived = true;
  if (changed) {
    emit policyChanged();
  }
  if (m_writePending && next == m_requested) {
    finish(RestorePolicyResult::Applied,
           QStringLiteral("Restore policy saved"));
  }
}

void TextEditorRestorePolicy::handleClientState() {
  if (m_client.state() == ClientState::Ready) {
    return;
  }
  // SettingsClient enters Authenticating after every confirmed commit while
  // retaining the old complete snapshot until the mandatory refresh arrives.
  // Owner replacement clears that snapshot, so the same state still fails
  // closed for lineage loss without misclassifying a normal refresh.
  if (m_client.state() == ClientState::Authenticating &&
      m_client.snapshot().has_value()) {
    return;
  }
  resetToDefault();
  if (m_writePending) {
    finish(RestorePolicyResult::Uncertain,
           QStringLiteral("Restore policy outcome is unknown; not replayed"));
  }
}

void TextEditorRestorePolicy::handleCommit(const CommitOutcome &outcome) {
  if (!m_writePending) {
    return;
  }
  if (outcome.status == SettingsWireStatus::Applied) {
    return; // The client's mandatory fresh snapshot confirms final truth.
  }
  if (outcome.status == SettingsWireStatus::Conflict) {
    const QVariant authoritative =
        outcome.currentValues.value(QString(TextEditorKeys::RestoreDocuments));
    if (authoritative.metaType().id() == QMetaType::Bool &&
        authoritative.toBool() == m_requested) {
      return;
    }
    finish(RestorePolicyResult::Conflict,
           outcome.message.isEmpty()
               ? QStringLiteral("Restore policy changed; re-apply explicitly")
               : outcome.message);
    return;
  }
  finish(outcome.status == SettingsWireStatus::EpochMismatch
             ? RestorePolicyResult::Conflict
             : RestorePolicyResult::Failed,
         outcome.message.isEmpty()
             ? QStringLiteral("Restore policy was rejected")
             : outcome.message);
}

void TextEditorRestorePolicy::handleUncertain(const QString &message) {
  if (m_writePending) {
    finish(
        RestorePolicyResult::Uncertain,
        message.isEmpty()
            ? QStringLiteral("Restore policy outcome is unknown; not replayed")
            : message);
  }
}

void TextEditorRestorePolicy::resetToDefault() {
  const bool changed = m_enabled || m_baselineReceived;
  m_enabled = false;
  m_baselineReceived = false;
  if (changed) {
    emit policyChanged();
  }
}

void TextEditorRestorePolicy::finish(const RestorePolicyResult result,
                                     const QString &message) {
  m_writePending = false;
  emit policyChanged();
  emit applyFinished(result, message.left(512));
}

} // namespace QindaQt::Apps::TextEditor
