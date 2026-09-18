// SPDX-License-Identifier: GPL-3.0-or-later
#include "notificationquietingsettingsbridge.h"

#include "qindaqt/services/notification_presentation_policy/notification_interruption_policy.h"
#include "qindaqt/services/settings_client/settings_client.h"

namespace QindaQt::Shell {
namespace {

constexpr auto ScheduleKey = "services.doNotDisturbSchedule";
constexpr auto StartKey = "services.doNotDisturbStartMinutes";
constexpr auto EndKey = "services.doNotDisturbEndMinutes";

} // namespace

NotificationQuietingSettingsBridge::NotificationQuietingSettingsBridge(
    Services::SettingsClient::SettingsClient &client,
    Services::NotificationPresentationPolicy::NotificationInterruptionPolicy &policy)
    : m_policy(policy), m_controller(client)
{
    // AGENT-GUARD: fail quiet before Settings1 establishes its first exact-
    // owner baseline. A persisted true value must never be briefly violated.
    m_policy.setDoNotDisturbEnabled(true);
    QObject::connect(&m_controller,
                     &Services::SettingsClient::DoNotDisturbController::confirmedValue,
                     &m_controller, [this](bool enabled) {
        // Transport loss emits no confirmed value, so the last accepted policy
        // is retained until a replacement owner publishes a full snapshot.
        m_policy.setDoNotDisturbEnabled(enabled);
    });
    // ADR-0212: the schedule rides the same purpose-scoped client. It is read
    // from the snapshot rather than through a commit state machine because the
    // shell only ever reads it -- the Settings route owns writing it.
    QObject::connect(&client, &Services::SettingsClient::SettingsClient::snapshotChanged,
                     &m_controller, [this, &client] { applySchedule(client); });
    applySchedule(client);
}

void NotificationQuietingSettingsBridge::applySchedule(
    Services::SettingsClient::SettingsClient &client)
{
    const auto &snapshot = client.snapshot();
    if (!snapshot.has_value()) {
        // No baseline yet: leave the schedule exactly as it is rather than
        // inventing an "off" the user never chose.
        return;
    }
    const QVariantMap &values = snapshot->values;
    Services::NotificationPresentationPolicy::NotificationInterruptionPolicy::QuietHours
        quietHours;
    quietHours.enabled = values.value(QLatin1String(ScheduleKey)).toBool();
    bool startOk = false;
    bool endOk = false;
    const int start = values.value(QLatin1String(StartKey)).toInt(&startOk);
    const int end = values.value(QLatin1String(EndKey)).toInt(&endOk);
    if (!startOk || !endOk) {
        // A key the schema should have supplied is missing or unreadable: do
        // not quiet on a guess.
        return;
    }
    quietHours.startMinutes = start;
    quietHours.endMinutes = end;
    // setQuietHours refuses an out-of-range window whole; nothing here clamps.
    m_policy.setQuietHours(quietHours);
}

} // namespace QindaQt::Shell
