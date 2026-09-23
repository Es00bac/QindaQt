// SPDX-License-Identifier: GPL-3.0-or-later
#include "notification_schedule_model.h"

#include "qindaqt/services/settings_client/settings_client.h"

#include <QVariant>

namespace QindaQt::Apps::SettingsNotifications {
namespace {

constexpr auto ScheduleKey = "services.doNotDisturbSchedule";
constexpr auto StartKey = "services.doNotDisturbStartMinutes";
constexpr auto EndKey = "services.doNotDisturbEndMinutes";

bool exactMinute(const QVariant &value, int *minutes) {
  const int type = value.metaType().id();
  if (type != QMetaType::Int && type != QMetaType::UInt &&
      type != QMetaType::LongLong && type != QMetaType::ULongLong) {
    return false;
  }
  bool ok = false;
  const qlonglong number = value.toLongLong(&ok);
  if (!ok || number < 0 || number >= NotificationScheduleModel::minutesPerDay) {
    return false;
  }
  *minutes = int(number);
  return true;
}

} // namespace

NotificationScheduleModel::NotificationScheduleModel(
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          [this] { applySnapshot(true); });
  connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged, this,
          &NotificationScheduleModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::ownerChanged, this,
          &NotificationScheduleModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::writeInFlightChanged,
          this, &NotificationScheduleModel::viewChanged);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
          this, &NotificationScheduleModel::handleCommit);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
          this, &NotificationScheduleModel::handleUncertain);
  applySnapshot();
}

bool NotificationScheduleModel::canEdit() const {
  return m_available && m_client.state() == Services::SettingsClient::ClientState::Ready &&
         !m_pending && !m_client.writeInFlight();
}

QString NotificationScheduleModel::statusText() const {
  if (m_pending) {
    return m_waitingForReadback ? tr("Checking the saved quiet-hours setting…")
                                : tr("Saving quiet hours…");
  }
  if (m_conflict) {
    return tr("Quiet hours changed elsewhere; current values are shown.");
  }
  if (m_uncertain) {
    return tr("The save result is unknown. Check the current values before trying again.");
  }
  return {};
}

bool NotificationScheduleModel::isMinuteOfDay(const int minutes) noexcept {
  return minutes >= 0 && minutes < minutesPerDay;
}

QString NotificationScheduleModel::formatMinutes(const int minutes) {
  if (!isMinuteOfDay(minutes)) {
    return {};
  }
  return QStringLiteral("%1:%2")
      .arg(minutes / 60, 2, 10, QLatin1Char('0'))
      .arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

QString NotificationScheduleModel::startText() const {
  return formatMinutes(m_startMinutes);
}

QString NotificationScheduleModel::endText() const {
  return formatMinutes(m_endMinutes);
}

QString NotificationScheduleModel::summaryText() const {
  if (!m_available) {
    return tr("The settings service is unavailable, so the schedule cannot be "
              "changed here.");
  }
  if (!m_enabled) {
    return tr("Notifications are never quieted on a schedule.");
  }
  // AGENT-GUARD: equal ends are an empty window, not a silent machine. Say so,
  // rather than showing a schedule that quiets nothing.
  if (m_startMinutes == m_endMinutes) {
    return tr("Start and end are the same time, so nothing is quieted. Choose "
              "two different times.");
  }
  return m_startMinutes > m_endMinutes
             ? tr("Quiet from %1 tonight until %2 tomorrow. Urgent "
                  "notifications still come through.")
                   .arg(startText(), endText())
             : tr("Quiet from %1 until %2. Urgent notifications still come "
                  "through.")
                   .arg(startText(), endText());
}

void NotificationScheduleModel::applySnapshot(bool fresh) {
  const auto &snapshot = m_client.snapshot();
  bool available = snapshot.has_value() &&
                   m_client.state() == Services::SettingsClient::ClientState::Ready &&
                   snapshot->owner == m_client.currentOwner();
  bool enabled = m_enabled;
  int start = m_startMinutes;
  int end = m_endMinutes;
  if (available) {
    const QVariantMap &values = snapshot->values;
    const QVariant schedule = values.value(QLatin1String(ScheduleKey));
    int nextStart = 0;
    int nextEnd = 0;
    // The scoped client validates transport shape, not these schema types.
    // Coercible strings and booleans are not schedule-minute authority.
    available = schedule.metaType().id() == QMetaType::Bool &&
                exactMinute(values.value(QLatin1String(StartKey)), &nextStart) &&
                exactMinute(values.value(QLatin1String(EndKey)), &nextEnd);
    if (available) {
      enabled = schedule.toBool();
      start = nextStart;
      end = nextEnd;
    }
  }
  bool resultChanged = false;
  // AGENT-GUARD: an Applied reply is admission to readback, not a value. Only
  // a fresh snapshot at or after that revision may complete this schedule save.
  if (m_pending && m_waitingForReadback && fresh) {
    m_pending = false;
    m_waitingForReadback = false;
    if (!available || snapshot->owner != m_writeOwner ||
        snapshot->epoch != m_writeEpoch || snapshot->revision < m_readbackRevision) {
      m_uncertain = true;
      m_errorText = tr("The saved quiet-hours value could not be confirmed.");
    } else if (snapshot->values.value(m_writeKey) != m_requestedValue) {
      m_conflict = true;
      m_errorText = tr("The saved quiet-hours value differs from your choice.");
    }
    m_writeKey.clear();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    resultChanged = true;
  }
  if (!resultChanged && available == m_available && enabled == m_enabled &&
      start == m_startMinutes && end == m_endMinutes) {
    return;
  }
  m_available = available;
  m_enabled = enabled;
  m_startMinutes = start;
  m_endMinutes = end;
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::handleClientState() {
  bool retired = false;
  if (m_pending && (m_client.currentOwner() != m_writeOwner ||
      m_client.state() == Services::SettingsClient::ClientState::Unavailable ||
      m_client.state() == Services::SettingsClient::ClientState::Degraded)) {
    // Owner replacement fences this operation even when an old snapshot is
    // retained. The client never replays an uncertain commit.
    m_pending = false;
    m_waitingForReadback = false;
    m_uncertain = true;
    m_errorText = tr("The settings service changed before quiet hours could be confirmed.");
    m_writeKey.clear();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    retired = true;
  }
  applySnapshot();
  if (retired) Q_EMIT viewChanged();
}

void NotificationScheduleModel::handleCommit(
    const Services::SettingsClient::CommitOutcome &outcome) {
  // AGENT-CONTRACT: SettingsClient emits untagged results for its one mutation
  // lane. Only the model that admitted the write may consume this signal.
  if (!m_pending || m_waitingForReadback) {
    return;
  }
  using Services::SettingsProtocol::SettingsWireStatus;
  if (outcome.status == SettingsWireStatus::Applied) {
    m_waitingForReadback = true;
    m_readbackRevision = outcome.revisionAfter;
  } else {
    m_pending = false;
    m_writeKey.clear();
    m_writeOwner.clear();
    m_writeEpoch.clear();
    m_conflict = outcome.status == SettingsWireStatus::Conflict;
    m_errorText = outcome.message.left(512);
    if (m_errorText.isEmpty()) {
      m_errorText = tr("Quiet-hours save was refused (%1).")
                        .arg(Services::SettingsProtocol::settingsWireStatusName(outcome.status));
    }
  }
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::handleUncertain(const QString &message) {
  if (!m_pending) {
    return;
  }
  m_pending = false;
  m_waitingForReadback = false;
  m_writeKey.clear();
  m_writeOwner.clear();
  m_writeEpoch.clear();
  m_uncertain = true;
  m_errorText = message.isEmpty() ? tr("Quiet-hours save result is unknown.")
                                  : message.left(512);
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::write(const QString &key, const QVariant &value) {
  if (!canEdit()) {
    return;
  }
  m_writeOwner = m_client.currentOwner();
  m_writeEpoch = m_client.snapshot()->epoch;
  m_writeKey = key;
  m_requestedValue = value;
  m_pending = true;
  m_waitingForReadback = false;
  m_conflict = false;
  m_uncertain = false;
  m_errorText.clear();
  QString error;
  if (m_client.setUserValue(key, value, &error)) {
    Q_EMIT viewChanged();
    return;
  }
  // AGENT-GUARD: nothing published moves. The snapshot is still the truth, and
  // the page keeps showing it with the reason attached.
  m_pending = false;
  m_writeKey.clear();
  m_writeOwner.clear();
  m_writeEpoch.clear();
  m_errorText = error.left(512);
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::setScheduleEnabled(const bool enabled) {
  if (!canEdit() || enabled == m_enabled) {
    return;
  }
  // AGENT-GUARD: the schema types this key boolean. Writing 1/0 would be
  // refused by normalization, and the control would silently never move.
  write(QLatin1String(ScheduleKey), enabled);
}

void NotificationScheduleModel::setStart(const int hour, const int minute) {
  if (!canEdit() || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    return;
  }
  const int minutes = hour * 60 + minute;
  if (minutes == m_startMinutes) {
    return;
  }
  write(QLatin1String(StartKey), minutes);
}

void NotificationScheduleModel::setEnd(const int hour, const int minute) {
  if (!canEdit() || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    return;
  }
  const int minutes = hour * 60 + minute;
  if (minutes == m_endMinutes) {
    return;
  }
  write(QLatin1String(EndKey), minutes);
}

void NotificationScheduleModel::clearError() {
  if (m_errorText.isEmpty()) {
    return;
  }
  m_errorText.clear();
  Q_EMIT viewChanged();
}

} // namespace QindaQt::Apps::SettingsNotifications
