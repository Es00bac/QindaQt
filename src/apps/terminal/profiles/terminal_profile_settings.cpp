// SPDX-License-Identifier: GPL-3.0-or-later
#include "profiles/terminal_profile_settings.h"

#include "qindaqt/services/settings_client/settings_client.h"

#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>
#include <optional>

namespace QindaQt::Apps::Terminal {
namespace {

using QindaQt::Services::SettingsClient::ClientState;
using QindaQt::Services::SettingsClient::CommitOutcome;
using QindaQt::Services::SettingsClient::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;

// The id format accepted for services.terminalDefaultProfile: the built-in id,
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
          {QStringLiteral("message"), message.left(512)}};
}

struct DecodedSettings final {
  QList<TerminalProfile> profiles;
  QString defaultProfileId;
  bool restoreTabs = false;
};

std::optional<DecodedSettings> decodeSettings(const QVariantMap &values) {
  const QVariant profilesValue = values.value(QString(TerminalKeys::Profiles));
  const QVariant defaultValue =
      values.value(QString(TerminalKeys::DefaultProfile));
  const QVariant restoreValue =
      values.value(QString(TerminalKeys::RestoreTabs));
  if (profilesValue.metaType().id() != QMetaType::QString ||
      defaultValue.metaType().id() != QMetaType::QString ||
      restoreValue.metaType().id() != QMetaType::Bool) {
    return std::nullopt;
  }
  const ProfileListCodecResult decoded =
      decodeTerminalProfiles(profilesValue.toString());
  if (!decoded.ok) {
    return std::nullopt;
  }
  QString defaultId = defaultValue.toString();
  if (defaultId.isEmpty()) {
    defaultId = builtinDefaultProfileId();
  }
  const bool defaultKnown =
      defaultId == builtinDefaultProfileId() ||
      std::any_of(decoded.profiles.begin(), decoded.profiles.end(),
                  [&defaultId](const TerminalProfile &profile) {
                    return profile.id == defaultId;
                  });
  if (!isAdmissibleDefaultProfileId(defaultId) || !defaultKnown) {
    return std::nullopt;
  }
  return DecodedSettings{decoded.profiles, defaultId, restoreValue.toBool()};
}

} // namespace

QStringList TerminalKeys::scopedKeys() {
  return {QString(Profiles), QString(DefaultProfile), QString(RestoreTabs)};
}

TerminalProfileSettings::TerminalProfileSettings(SettingsClient &client,
                                                 QObject *parent)
    : QObject(parent), m_client(client),
      m_defaultProfileId(builtinDefaultProfileId()) {
  connect(&m_client, &SettingsClient::snapshotChanged, this,
          [this] { handleSnapshot(); });
  connect(&m_client, &SettingsClient::stateChanged, this,
          [this] { handleClientState(); });
  connect(
      &m_client, &SettingsClient::commitFinished, this,
      [this](const CommitOutcome &outcome) { handleCommitFinished(outcome); });
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
  if (m_ledgerActive || !m_baselineReceived ||
      m_client.state() != ClientState::Ready ||
      !m_client.snapshot().has_value() || m_client.writeInFlight()) {
    return false;
  }
  if (!validateTerminalProfileList(profiles).ok ||
      !isAdmissibleDefaultProfileId(defaultProfileId)) {
    return false;
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
  m_waitingForSnapshot = false;
  m_sequenceAborted = false;
  writeNextQueued();
  return true;
}

void TerminalProfileSettings::handleSnapshot() {
  const auto snapshot = m_client.snapshot();
  if (!snapshot.has_value()) {
    return;
  }
  const auto decoded = decodeSettings(snapshot->values);
  if (!decoded.has_value()) {
    resetToBuiltins();
    if (m_ledgerActive && m_pendingKey.isEmpty()) {
      const QString key = m_queue.isEmpty() ? QString(TerminalKeys::RestoreTabs)
                                            : m_queue.first().key;
      abortLedger(key, QStringLiteral("failed"),
                  QStringLiteral("Settings snapshot contains invalid "
                                 "terminal profile data"));
    }
    return;
  }
  const bool hadLineage = !m_confirmedOwner.isEmpty();
  const bool lineageChanged =
      hadLineage && (snapshot->owner != m_confirmedOwner ||
                     snapshot->epoch != m_confirmedEpoch);
  const bool changed = !m_baselineReceived ||
                       decoded->profiles != m_userProfiles ||
                       decoded->defaultProfileId != m_defaultProfileId ||
                       decoded->restoreTabs != m_restoreTabs;
  m_userProfiles = decoded->profiles;
  m_defaultProfileId = decoded->defaultProfileId;
  m_restoreTabs = decoded->restoreTabs;
  m_baselineReceived = true;
  m_confirmedOwner = snapshot->owner;
  m_confirmedEpoch = snapshot->epoch;
  if (changed) {
    emit profilesChanged();
  }
  if (lineageChanged && m_ledgerActive) {
    const QString key = m_queue.isEmpty() ? QString(TerminalKeys::RestoreTabs)
                                          : m_queue.first().key;
    abortLedger(key, QStringLiteral("conflict"),
                QStringLiteral("Settings authority changed; explicit "
                               "re-apply is required"));
    return;
  }
  // The client re-syncs after every commit; the next queued write may only
  // carry the fresh base revision.
  m_waitingForSnapshot = false;
  writeNextQueued();
}

void TerminalProfileSettings::handleClientState() {
  if (m_client.state() == ClientState::Ready) {
    return;
  }
  // The terminal has no authority to retain user profiles across transport
  // loss or owner replacement. Existing sessions keep their copied profile,
  // while new-session policy immediately falls back to the built-in value.
  resetToBuiltins();
  if ((m_client.state() == ClientState::Unavailable ||
       m_client.state() == ClientState::Degraded) &&
      m_ledgerActive && m_pendingKey.isEmpty()) {
    m_queue.clear();
    m_waitingForSnapshot = false;
    m_sequenceAborted = true;
    finalizeLedger();
  }
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
  if (m_waitingForSnapshot || !m_baselineReceived ||
      m_client.state() != ClientState::Ready || m_client.writeInFlight()) {
    return;
  }
  const QueuedWrite write = m_queue.takeFirst();
  QString error;
  if (!m_client.setUserValue(write.key, write.value, &error)) {
    abortLedger(write.key, QStringLiteral("failed"),
                error.isEmpty() ? QStringLiteral("Write was refused") : error);
    return;
  }
  m_pendingKey = write.key;
}

void TerminalProfileSettings::handleCommitFinished(
    const CommitOutcome &outcome) {
  if (!m_ledgerActive || m_pendingKey.isEmpty()) {
    return;
  }
  const QString key = m_pendingKey;
  m_pendingKey.clear();
  const QVariant intended = m_intendedValues.value(key);

  switch (outcome.status) {
  case SettingsWireStatus::Applied:
    m_ledgerEntries.insert(
        key, ledgerEntry(QStringLiteral("applied"), outcome.message));
    // Next write waits for the automatic post-commit refresh (handleSnapshot
    // drives writeNextQueued); the final snapshot also confirms the value.
    m_waitingForSnapshot = true;
    break;
  case SettingsWireStatus::Conflict: {
    // Conflict whose authoritative value already matches the draft counts
    // as applied, awaiting snapshot confirmation; any other conflict keeps
    // the confirmed state and aborts (explicit re-apply required).
    const QVariant authoritative = outcome.currentValues.value(key);
    if (authoritative.isValid() && authoritative == intended) {
      m_ledgerEntries.insert(
          key, ledgerEntry(QStringLiteral("applied"), outcome.message));
      m_waitingForSnapshot = true;
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
  m_waitingForSnapshot = false;
  m_sequenceAborted = true;
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
  if (const auto snapshot = m_client.snapshot(); !m_sequenceAborted &&
                                                 snapshot.has_value() &&
                                                 !m_intendedValues.isEmpty()) {
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
  m_waitingForSnapshot = false;
  m_sequenceAborted = false;
  m_ledgerEntries.clear();
  m_intendedValues.clear();
  emit applyFinished(ledger);
}

void TerminalProfileSettings::resetToBuiltins() {
  const bool changed = m_baselineReceived || !m_userProfiles.isEmpty() ||
                       m_defaultProfileId != builtinDefaultProfileId() ||
                       m_restoreTabs;
  m_userProfiles.clear();
  m_defaultProfileId = builtinDefaultProfileId();
  m_restoreTabs = false;
  m_baselineReceived = false;
  if (changed) {
    emit profilesChanged();
  }
}

} // namespace QindaQt::Apps::Terminal
