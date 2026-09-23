// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/idle_display_settings.h>

#include <QMetaType>
#include <QVariant>

namespace QindaQt::Apps::SettingsPower {
namespace {
constexpr auto idleKey = "power.idleDisplayOffMinutes";
using Services::SettingsClient::ClientState;
using Services::SettingsClient::CommitOutcome;
using Services::SettingsProtocol::SettingsWireStatus;
using Session::DesktopControls::IdleDisplayPreferences;

bool exactPersistedMinutes(const QVariant &value, qint64 *minutes) {
  const int type = value.metaType().id();
  if (type != QMetaType::Int && type != QMetaType::UInt &&
      type != QMetaType::LongLong && type != QMetaType::ULongLong) {
    return false;
  }
  bool ok = false;
  const qint64 number = value.toLongLong(&ok);
  if (!ok) return false;
  *minutes = number;
  return true;
}
} // namespace

IdleDisplaySettingsModel::IdleDisplaySettingsModel(
    Session::DesktopControls::Settings1IdlePreferences &preferences,
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_preferences(preferences), m_client(client) {
  m_readbackTimeout.setSingleShot(true);
  m_readbackTimeout.setInterval(5'000);
  connect(&m_readbackTimeout, &QTimer::timeout, this, [this] {
    if (m_pending && m_waitingForReadback) {
      markUncertain(tr("Settings accepted the change, but a fresh display-off value could not be confirmed."));
      m_client.refresh();
    }
  });
  connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
          this, [this] {
            ++m_snapshotSequence;
            applySnapshot(true);
          });
  connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
          this, &IdleDisplaySettingsModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::ownerChanged,
          this, &IdleDisplaySettingsModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
          this, &IdleDisplaySettingsModel::changed);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
          this, &IdleDisplaySettingsModel::handleCommit);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
          this, &IdleDisplaySettingsModel::markUncertain);
  applySnapshot(false);
}

IdleDisplaySettingsModel::~IdleDisplaySettingsModel() = default;

bool IdleDisplaySettingsModel::canEdit() const {
  return m_available && !m_pending &&
         m_client.canSetUserValue(QLatin1StringView(idleKey));
}

bool IdleDisplaySettingsModel::enabled() const noexcept {
  return m_hasConfirmed && m_confirmedMinutes > 0;
}

int IdleDisplaySettingsModel::minutes() const noexcept {
  return m_hasConfirmed ? m_lastPositiveMinutes : 0;
}

void IdleDisplaySettingsModel::publishStatus() {
  if (m_pending) {
    m_statusText = m_waitingForReadback
        ? tr("Checking the saved display-off preference…")
        : tr("Saving display-off preference…");
  } else if (!m_hasConfirmed) {
    m_statusText = tr("Display-off preference not confirmed. The desktop may use its documented default until Settings is available.");
  } else if (!m_available) {
    m_statusText = tr("Display-off preference unavailable. Last confirmed value is shown; changes are disabled.");
  } else if (m_confirmedMinutes > 0) {
    m_statusText = tr("%n minute(s) of inactivity turns the display off.",
                      nullptr, m_lastPositiveMinutes);
  } else {
    m_statusText = tr("The display stays on.");
  }
  Q_EMIT changed();
}

void IdleDisplaySettingsModel::applySnapshot(bool fresh) {
  const auto &snapshot = m_client.snapshot();
  qint64 persisted = 0;
  const bool accepted = snapshot.has_value() &&
      m_client.state() == ClientState::Ready &&
      snapshot->owner == m_client.currentOwner() &&
      exactPersistedMinutes(snapshot->values.value(QLatin1String(idleKey)),
                            &persisted);
  m_available = accepted;
  if (accepted) {
    // A disabled value carries no retained positive timeout. Do not reuse a
    // local choice from an unrelated Settings1 owner/epoch when it returns.
    if (snapshot->owner != m_confirmedOwner ||
        snapshot->epoch != m_confirmedEpoch) {
      m_lastPositiveMinutes = IdleDisplayPreferences::defaultTimeoutMinutes();
    }
    m_confirmedOwner = snapshot->owner;
    m_confirmedEpoch = snapshot->epoch;
    m_confirmedMinutes = persisted;
    m_hasConfirmed = true;
    if (persisted > 0) {
      m_lastPositiveMinutes =
          IdleDisplayPreferences::fromMinutes(persisted).minutes;
    }
  }
  if (m_pending && m_waitingForReadback && fresh) {
    if (!accepted || snapshot->owner != m_writeOwner ||
        snapshot->epoch != m_writeEpoch) {
      markUncertain(tr("Settings changed before the display-off preference could be confirmed."));
    } else if (m_snapshotSequence >= m_requiredSnapshotSequence &&
               snapshot->revision >= m_revisionFloor) {
      m_pending = false;
      m_waitingForReadback = false;
      m_readbackTimeout.stop();
      if (persisted == m_requestedMinutes) {
        m_errorText.clear();
        m_conflict = false;
        m_uncertain = false;
      } else {
        m_conflict = true;
        m_errorText = tr("The saved display-off value differs from your choice.");
      }
      m_writeOwner.clear();
      m_writeEpoch.clear();
    }
    // A same-lineage snapshot older than Applied's revision stays pending.
  }
  publishStatus();
}

void IdleDisplaySettingsModel::handleClientState() {
  if (m_pending &&
      (m_client.currentOwner() != m_writeOwner ||
       m_client.state() == ClientState::Unavailable ||
       m_client.state() == ClientState::Degraded)) {
    markUncertain(tr("Settings changed before the display-off preference could be confirmed."));
  }
  applySnapshot(false);
}

void IdleDisplaySettingsModel::handleCommit(const CommitOutcome &outcome) {
  if (!m_pending || m_waitingForReadback) return;
  if (outcome.status == SettingsWireStatus::Applied) {
    if (m_client.currentOwner() != m_writeOwner || !m_client.snapshot() ||
        m_client.snapshot()->epoch != m_writeEpoch) {
      markUncertain(tr("Settings changed before the display-off preference could be confirmed."));
      return;
    }
    m_revisionFloor = outcome.revisionAfter;
    m_waitingForReadback = true;
    m_requiredSnapshotSequence = m_snapshotSequenceAtDispatch + 1;
    m_readbackTimeout.start();
    applySnapshot(false);
    return;
  }
  m_pending = false;
  m_waitingForReadback = false;
  m_readbackTimeout.stop();
  m_conflict = outcome.status == SettingsWireStatus::Conflict ||
               outcome.status == SettingsWireStatus::EpochMismatch;
  const QString reason = outcome.message.isEmpty()
      ? Services::SettingsProtocol::settingsWireStatusName(outcome.status)
      : outcome.message.left(512);
  m_errorText = m_conflict
      ? tr("The display-off preference changed elsewhere: %1").arg(reason)
      : tr("The display-off preference was refused: %1").arg(reason);
  m_writeOwner.clear();
  m_writeEpoch.clear();
  publishStatus();
}

void IdleDisplaySettingsModel::markUncertain(const QString &reason) {
  if (!m_pending) return;
  m_pending = false;
  m_waitingForReadback = false;
  m_readbackTimeout.stop();
  m_uncertain = true;
  m_conflict = false;
  m_errorText = tr("The display-off change may have happened. %1 Refresh to see the current value; no write was retried.")
                    .arg(reason.left(512));
  m_writeOwner.clear();
  m_writeEpoch.clear();
  publishStatus();
}

bool IdleDisplaySettingsModel::setEnabled(bool enabled) {
  if (m_pending) return false;
  if (!canEdit()) {
    m_errorText = tr("The display-off preference is not ready for this change.");
    publishStatus();
    return false;
  }
  if (enabled == this->enabled()) return true;
  const int minutes = m_lastPositiveMinutes >= 1
      ? m_lastPositiveMinutes
      : IdleDisplayPreferences::defaultTimeoutMinutes();
  return submit(enabled ? minutes : -1);
}

bool IdleDisplaySettingsModel::setMinutes(int minutes) {
  if (m_pending) return false;
  if (minutes < 1 || minutes > IdleDisplayPreferences::maximumTimeoutMinutes()) {
    m_errorText = tr("Choose a timeout between 1 and %1 minutes.")
                      .arg(IdleDisplayPreferences::maximumTimeoutMinutes());
    publishStatus();
    return false;
  }
  if (!canEdit()) {
    m_errorText = tr("The display-off preference is not ready for this change.");
    publishStatus();
    return false;
  }
  if (enabled() && minutes == m_lastPositiveMinutes) return true;
  return submit(minutes);
}

bool IdleDisplaySettingsModel::retry() {
  if (m_pending) return false;
  m_preferences.refresh();
  return true;
}

bool IdleDisplaySettingsModel::submit(qint64 persistedMinutes) {
  m_writeOwner = m_client.currentOwner();
  m_writeEpoch = m_client.snapshot()->epoch;
  m_requestedMinutes = persistedMinutes;
  m_snapshotSequenceAtDispatch = m_snapshotSequence;
  m_pending = true;
  m_waitingForReadback = false;
  m_conflict = false;
  m_uncertain = false;
  m_errorText.clear();
  publishStatus();
  QString error;
  if (m_client.setUserValue(QLatin1StringView(idleKey),
                            QVariant::fromValue(persistedMinutes), &error)) {
    return true;
  }
  m_pending = false;
  m_writeOwner.clear();
  m_writeEpoch.clear();
  m_errorText = error.isEmpty()
      ? tr("The display-off preference could not be submitted.")
      : error.left(512);
  publishStatus();
  return false;
}

} // namespace QindaQt::Apps::SettingsPower
