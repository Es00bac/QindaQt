// SPDX-License-Identifier: GPL-3.0-or-later
#include "notification_schedule_model.h"

#include "qindaqt/services/settings_client/settings_client.h"

#include <QVariant>

namespace QindaQt::Apps::SettingsNotifications {
namespace {

constexpr auto ScheduleKey = "services.doNotDisturbSchedule";
constexpr auto StartKey = "services.doNotDisturbStartMinutes";
constexpr auto EndKey = "services.doNotDisturbEndMinutes";

} // namespace

NotificationScheduleModel::NotificationScheduleModel(
    Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client) {
  connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged, this,
          &NotificationScheduleModel::applySnapshot);
  connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged, this,
          &NotificationScheduleModel::applySnapshot);
  applySnapshot();
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

void NotificationScheduleModel::applySnapshot() {
  const auto &snapshot = m_client.snapshot();
  const bool available =
      snapshot.has_value() &&
      m_client.state() != Services::SettingsClient::ClientState::Unavailable;
  bool enabled = m_enabled;
  int start = m_startMinutes;
  int end = m_endMinutes;
  if (available) {
    const QVariantMap &values = snapshot->values;
    enabled = values.value(QLatin1String(ScheduleKey)).toBool();
    bool startOk = false;
    bool endOk = false;
    const int nextStart = values.value(QLatin1String(StartKey)).toInt(&startOk);
    const int nextEnd = values.value(QLatin1String(EndKey)).toInt(&endOk);
    // A value the schema should have supplied is missing or unreadable: keep
    // showing the last good one rather than a guess.
    if (startOk && isMinuteOfDay(nextStart)) {
      start = nextStart;
    }
    if (endOk && isMinuteOfDay(nextEnd)) {
      end = nextEnd;
    }
  }
  if (available == m_available && enabled == m_enabled && start == m_startMinutes &&
      end == m_endMinutes) {
    return;
  }
  m_available = available;
  m_enabled = enabled;
  m_startMinutes = start;
  m_endMinutes = end;
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::write(const QString &key, const QVariant &value) {
  QString error;
  if (m_client.setUserValue(key, value, &error)) {
    if (!m_errorText.isEmpty()) {
      m_errorText.clear();
      Q_EMIT viewChanged();
    }
    return;
  }
  // AGENT-GUARD: nothing published moves. The snapshot is still the truth, and
  // the page keeps showing it with the reason attached.
  m_errorText = error.left(512);
  Q_EMIT viewChanged();
}

void NotificationScheduleModel::setScheduleEnabled(const bool enabled) {
  if (!m_available || enabled == m_enabled) {
    return;
  }
  // AGENT-GUARD: the schema types this key boolean. Writing 1/0 would be
  // refused by normalization, and the control would silently never move.
  write(QLatin1String(ScheduleKey), enabled);
}

void NotificationScheduleModel::setStart(const int hour, const int minute) {
  if (!m_available || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    return;
  }
  const int minutes = hour * 60 + minute;
  if (minutes == m_startMinutes) {
    return;
  }
  write(QLatin1String(StartKey), minutes);
}

void NotificationScheduleModel::setEnd(const int hour, const int minute) {
  if (!m_available || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
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
