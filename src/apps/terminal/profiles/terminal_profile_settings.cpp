// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile_settings.h"

#include "qindaqt/services/settings_client/settings_client.h"

#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>

namespace QindaQt::Apps::Terminal {
namespace {

using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::CommitOutcome;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;

// The id format accepted for terminal.defaultProfile: the built-in id,
// empty (built-in default), or a user profile id shape.
bool isAdmissibleDefaultProfileId(const QString &id) {
  if (id.isEmpty() || id == builtinDefaultProfileId()) {
    return true;
  }
  static const QRegularExpression pattern(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,31}$"));
  return pattern.match(id).hasMatch();
}

QVariantMap ledgerEntry(const QString &result, const QString &message) {
  return {{QStringLiteral("result"), result},
          {QStringLiteral("message"), message}};
}

} // namespace

QStringList TerminalKeys::scopedKeys() {
  return {QString(Profiles), QString(DefaultProfile), QString(RestoreTabs)};
}

TerminalProfileSettings::TerminalProfileSettings(SettingsClient &client,
                                                 QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client, &SettingsClient::snapshotChanged, this,
          [this] { handleSnapshot(); });
  connect(&m_client, &SettingsClient::commitFinished, this,
          [this](const CommitOutcome &outcome) {
            handleCommitFinished(QVariant::fromValue(outcome));
          });
  connect(&m_client, &SettingsClient::commitUncertain, this,
          [this](const QString &message) { handleCommitUncertain(message); });
  // A baseline may already be installed before this controller connects.
  if (m_client.snapshot().has_value()) {
    handleSnapshot();
  }
}

TerminalProfileSettings::~TerminalProfileSettings() = default;

TerminalProfile TerminalProfileSettings::defaultProfile() const {
  for (const TerminalProfile &profile : m_userProfiles) {
    if (profile.id == m_defaultProfileId) {
      return profile;
    }
  }
  return builtinDefaultProfile();
}

bool TerminalProfileSettings::applyProfiles(
    const QList<TerminalProfile> &profiles, const QString &defaultProfileId,
    bool restoreTabs) {
  if (m_ledgerActive ||
      m_client.state() != ClientState::Ready ||
      !m_client.snapshot().has_value() || m_client.writeInFlight()) {
    return false;
  }
  if (profiles.size() > TerminalProfile::kMaxUserProfiles ||
      !isAdmissibleDefaultProfileId(defaultProfileId)) {
    return false;
  }
  for (const TerminalProfile &profile : profiles) {
    if (!validateTerminalProfile(profile).ok) {
      return false;
    }
  }
  // defaultProfile must reference a listed profile or the built-in default.
  if (defaultProfileId != builtinDefaultProfileId() &&
      !defaultProfileId.isEmpty() &&
      std::none_of(profiles.begin(), profiles.end(),
                   [&defaultProfileId](const TerminalProfile &profile) {
                     return profile.id == defaultProfileId;
                   })) {
    return false;
  }

  bool encodeOk = false;
  const QString encoded = encodeTerminalProfiles(profiles, &encodeOk);
  if (!encodeOk) {
    return false;
  }

  m_intendedValues = {{QString(TerminalKeys::Profiles), encoded},
                      {QString(TerminalKeys::DefaultProfile), defaultProfileId},
                      {QString(TerminalKeys::RestoreTabs), restoreTabs}};
  m_queue = {{QString(TerminalKeys::Profiles), encoded},
             {QString(TerminalKeys::DefaultProfile), defaultProfileId},
             {QString(TerminalKeys::RestoreTabs), QVariant(restoreTabs)}};
  m_ledgerEntries.clear();
  m_ledgerActive = true;
  writeNextQueued();
  return true;
}

void TerminalProfileSettings::handleSnapshot() {
  const auto snapshot = m_client.snapshot();
  if (!snapshot.has_value()) {
    return;
  }
  bool changed = !m_baselineReceived;
  m_baselineReceived = true;

  // Fail-closed decode: a malformed value never replaces confirmed state.
  const QVariant profilesValue =
      snapshot->values.value(QString(TerminalKeys::Profiles));
  if (profilesValue.metaType().id() == QMetaType::QString) {
    const auto decoded = decodeTerminalProfiles(profilesValue.toString());
    if (decoded.ok && decoded.profiles != m_userProfiles) {
      m_userProfiles = decoded.profiles;
      changed = true;
    }
  }
  const QVariant defaultValue =
      snapshot->values.value(QString(TerminalKeys::DefaultProfile));
  if (defaultValue.metaType().id() == QMetaType::QString) {
    const QString id = defaultValue.toString();
    if (isAdmissibleDefaultProfileId(id) && id != m_defaultProfileId) {
      m_defaultProfileId = id;
      changed = true;
    }
  }
  const QVariant restoreValue =
      snapshot->values.value(QString(TerminalKeys::RestoreTabs));
  if (restoreValue.metaType().id() == QMetaType::Bool) {
    const bool policy = restoreValue.toBool();
    if (policy != m_restoreTabs) {
      m_restoreTabs = policy;
      changed = true;
    }
  }
  if (changed) {
    emit profilesChanged();
  }
  // The client re-syncs after every commit; the next queued write may only
  // carry the fresh base revision.
  writeNextQueued();
}

void TerminalProfileSettings::writeNextQueued() {
  if (!m_ledgerActive || m_queue.isEmpty()) {
    finalizeLedger();
    return;
  }
  if (!m_pendingKey.isEmpty()) {
    // A commit reply is still outstanding; only its outcome may advance
    // the sequence (an invalidation snapshot must not double-send).
    return;
  }
  if (m_client.state() != ClientState::Ready || m_client.writeInFlight()) {
    return;
  }
  const QueuedWrite write = m_queue.takeFirst();
  QString error;
  if (!m_client.setUserValue(write.key, write.value, &error)) {
    abortLedger(write.key, QStringLiteral("failed"),
                error.isEmpty() ? QStringLiteral("Write was refused")
                                : error);
    return;
  }
  m_pendingKey = write.key;
}

void TerminalProfileSettings::handleCommitFinished(const QVariant &variant) {
  if (!m_ledgerActive || m_pendingKey.isEmpty()) {
    return;
  }
  const CommitOutcome outcome = variant.value<CommitOutcome>();
  const QString key = m_pendingKey;
  m_pendingKey.clear();
  const QVariant intended = m_intendedValues.value(key);

  switch (outcome.status) {
  case SettingsWireStatus::Applied:
    m_ledgerEntries.insert(key, ledgerEntry(QStringLiteral("applied"),
                                            outcome.message));
    // Next write waits for the automatic post-commit refresh (handleSnapshot
    // drives writeNextQueued); the final snapshot also confirms the value.
    break;
  case SettingsWireStatus::Conflict: {
    // Conflict whose authoritative value already matches the draft counts
    // as applied, awaiting snapshot confirmation; any other conflict keeps
    // the confirmed state and aborts (explicit re-apply required).
    const QVariant authoritative = outcome.currentValues.value(key);
    if (authoritative.isValid() && authoritative == intended) {
      m_ledgerEntries.insert(key, ledgerEntry(QStringLiteral("applied"),
                                              outcome.message));
    } else {
      abortLedger(key, QStringLiteral("conflict"),
                  outcome.message.isEmpty()
                      ? QStringLiteral("Value changed on the service; re-apply "
                                       "against the fresh baseline")
                      : outcome.message);
    }
    break;
  }
  case SettingsWireStatus::EpochMismatch:
    abortLedger(key, QStringLiteral("conflict"),
                QStringLiteral("Settings service restarted during the "
                               "commit; re-apply against the fresh "
                               "baseline"));
    break;
  default:
    // Confirmed rejections (validation, read-only layer, persistence,
    // unknown key, malformed request, revision exhaustion) are terminal
    // answers, never retried silently.
    abortLedger(key, QStringLiteral("failed"),
                outcome.message.isEmpty()
                    ? QStringLiteral("Commit was rejected")
                    : outcome.message);
    break;
  }
}

void TerminalProfileSettings::handleCommitUncertain(const QString &message) {
  if (!m_ledgerActive || m_pendingKey.isEmpty()) {
    return;
  }
  const QString key = m_pendingKey;
  m_pendingKey.clear();
  // ADR-0028 no-replay: the write may or may not have landed; the client
  // discarded it. Only an explicit new apply may resubmit.
  abortLedger(key, QStringLiteral("uncertain"),
              message.isEmpty() ? QStringLiteral("Commit outcome is unknown "
                                                 "(transport loss or "
                                                 "timeout)")
                                : message);
}

void TerminalProfileSettings::abortLedger(const QString &key,
                                          const QString &result,
                                          const QString &message) {
  m_ledgerEntries.insert(key, ledgerEntry(result, message));
  m_queue.clear();
  finalizeLedger();
}

void TerminalProfileSettings::finalizeLedger() {
  if (!m_ledgerActive) {
    return;
  }
  const bool sequenceDone = m_queue.isEmpty() && m_pendingKey.isEmpty();
  if (!sequenceDone) {
    return;
  }
  // Final truth check against the freshest snapshot: a differing value is
  // a conflict even when every commit reply said Applied.
  if (const auto snapshot = m_client.snapshot();
      snapshot.has_value() && !m_intendedValues.isEmpty()) {
    for (auto it = m_intendedValues.begin(); it != m_intendedValues.end();
         ++it) {
      const QVariant actual = snapshot->values.value(it.key());
      if (actual.isValid() && actual != it.value()) {
        m_ledgerEntries.insert(
            it.key(),
            ledgerEntry(QStringLiteral("conflict"),
                        QStringLiteral("Service value differs from the "
                                       "applied draft")));
      }
    }
  }
  QVariantList ledger;
  const QStringList order = TerminalKeys::scopedKeys();
  for (const QString &key : order) {
    QVariantMap entry = m_ledgerEntries.value(key).toMap();
    if (entry.isEmpty()) {
      entry = ledgerEntry(QStringLiteral("not-attempted"),
                          QStringLiteral("Sequence aborted before this "
                                         "key"));
    }
    entry.insert(QStringLiteral("key"), key);
    ledger.append(entry);
  }
  m_ledgerActive = false;
  m_ledgerEntries.clear();
  m_intendedValues.clear();
  emit applyFinished(ledger);
}

} // namespace QindaQt::Apps::Terminal
