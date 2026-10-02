// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/critical_battery_policy.h>
#include <qindaqt/services/session_actions/session_actions_client.h>
namespace QindaQt::Power {
using Services::SessionActions::ActionStatus;
QStringList CriticalBatteryPolicy::settingsKeys() {
    return {QStringLiteral("power.critical.action"), QStringLiteral("power.critical.countdownSeconds")};
}
CriticalBatteryPolicy::CriticalBatteryPolicy(PowerServiceCoordinator &power,
    Services::SettingsClient::SettingsClient &settings, CriticalNotification &notification,
    Services::SessionActions::SessionActionsClient &actions, QObject *parent)
    : QObject(parent), m_power(power), m_settings(settings),
      m_notification(notification), m_actions(actions) {
    m_tick.setInterval(1000);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &CriticalBatteryPolicy::reconcile);
    connect(&power, &PowerServiceCoordinator::snapshotChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::stateChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::snapshotChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::ownerChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&settings, &Services::SettingsClient::SettingsClient::writeAdmissionChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&actions, &Services::SessionActions::SessionActionsClient::availabilityChanged, this, &CriticalBatteryPolicy::schedule);
    connect(&actions, &Services::SessionActions::SessionActionsClient::actionFinished, this,
        [this](const Services::SessionActions::SessionActionResult &result) {
            if (!m_actionPending) return;
            m_actionPending = false;
            if (result.status == ActionStatus::Uncertain) m_quarantined = true;
            m_phase = Phase::Spent;
        });
    connect(&notification, &CriticalNotification::shown, this, [this] {
        if (m_phase != Phase::Announcing || !currentAttempt()) { withdraw(); return; }
        m_phase = Phase::Counting;
        m_remaining = m_seconds;
        m_elapsed.start();
        m_tick.start();
    });
    connect(&notification, &CriticalNotification::cancelled, this, &CriticalBatteryPolicy::withdraw);
    connect(&notification, &CriticalNotification::failed, this, &CriticalBatteryPolicy::withdraw);
    connect(&notification, &CriticalNotification::closed, this, [this] {
        if (m_phase == Phase::Closing && currentAttempt()) dispatch();
    });
}
CriticalBatteryPolicy::~CriticalBatteryPolicy() { withdraw(); }
void CriticalBatteryPolicy::setNativeAuthority(bool admitted) {
    if (m_authority == admitted) return;
    m_authority = admitted;
    schedule();
}
void CriticalBatteryPolicy::schedule() {
    // Revoke synchronously before a queued Can* reply can dispatch. Coalescing
    // observation must never postpone withdrawal of policy admission.
    if (m_attempted && !currentAttempt()) withdraw();
    if (m_scheduled) return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this] { m_scheduled = false; reconcile(); });
}
QString CriticalBatteryPolicy::preferenceKey() const {
    const auto &s = m_settings.snapshot();
    if (m_settings.state() != Services::SettingsClient::ClientState::Ready || !s
        || s->owner != m_settings.currentOwner()
        || !m_settings.canSetUserValue(QStringLiteral("power.critical.action"))) return {};
    const auto action = s->values.value(QStringLiteral("power.critical.action"));
    const auto seconds = s->values.value(QStringLiteral("power.critical.countdownSeconds"));
    const int type = seconds.metaType().id();
    if (action.metaType() != QMetaType::fromType<QString>()
        || (type != QMetaType::Int && type != QMetaType::UInt
            && type != QMetaType::LongLong && type != QMetaType::ULongLong)) return {};
    const auto a = action.toString();
    bool ok = false;
    const auto n = seconds.toLongLong(&ok);
    if (!ok || n < 5 || n > 300 || (a != QStringLiteral("none")
        && a != QStringLiteral("suspend") && a != QStringLiteral("hibernate")
        && a != QStringLiteral("power-off"))) return {};
    return s->owner + QLatin1Char('|') + s->epoch + QLatin1Char('|') + a
        + QLatin1Char('|') + QString::number(n);
}
bool CriticalBatteryPolicy::actionAvailable(const QString &action) const {
    if (action == QStringLiteral("suspend")) return m_actions.canSuspend();
    if (action == QStringLiteral("hibernate")) return m_actions.canHibernate();
    if (action == QStringLiteral("power-off")) return m_actions.canPowerOff();
    return false;
}
bool CriticalBatteryPolicy::currentAttempt() const {
    const auto &p = m_power.snapshot();
    return m_authority && !m_quarantined && p.epoch == m_powerEpoch
        && p.capabilities.testFlag(Capability::Supplies) && p.source.onBattery
        && p.composite.present && p.composite.warning == WarningLevel::Action
        && preferenceKey() == m_preferenceKey && actionAvailable(m_action);
}
void CriticalBatteryPolicy::withdraw() {
    m_tick.stop();
    m_phase = Phase::Spent;
    m_notification.close();
    // AGENT-CONTRACT: SessionActions stop retires delayed Can callbacks before
    // dispatch and returns Uncertain after dispatch. Never queue a replacement.
    if (m_actionPending) m_actions.stop();
}
void CriticalBatteryPolicy::reconcile() {
    if (m_authority && !m_quarantined) m_actions.start();
    const auto &p = m_power.snapshot();
    const bool supplies = p.capabilities.testFlag(Capability::Supplies);
    const bool recovered = supplies && ((!p.source.onBattery && p.source.acPresent)
        || (p.source.onBattery && p.composite.present && p.composite.warning != WarningLevel::Unknown
            && p.composite.warning != WarningLevel::Action));
    if (recovered) {
        if (m_attempted) withdraw();
        m_attempted = false;
        m_phase = Phase::Idle;
        return;
    }
    if (m_attempted) {
        if (!currentAttempt()) { withdraw(); return; }
        if (m_phase != Phase::Counting) return;
        const int remaining = qMax(0, m_seconds - int(m_elapsed.elapsed() / 1000));
        if (remaining == 0) {
            m_tick.stop();
            m_phase = Phase::Closing;
            m_notification.close();
        } else if (remaining != m_remaining) {
            m_remaining = remaining;
            m_notification.update(remaining);
        }
        return;
    }
    if (!m_authority || m_quarantined || !supplies || !p.source.onBattery
        || !p.composite.present || p.composite.warning != WarningLevel::Action) return;
    const auto key = preferenceKey();
    if (key.isEmpty()) return;
    const auto &values = m_settings.snapshot()->values;
    const auto action = values.value(QStringLiteral("power.critical.action")).toString();
    if (!actionAvailable(action)) return;
    m_actions.start();
    m_attempted = true;
    m_preferenceKey = key;
    m_powerEpoch = p.epoch;
    m_action = action;
    m_seconds = values.value(QStringLiteral("power.critical.countdownSeconds")).toInt();
    m_phase = Phase::Announcing;
    m_notification.show(action, m_seconds);
}
void CriticalBatteryPolicy::dispatch() {
    m_phase = Phase::Action;
    m_actionPending = true;
    const bool accepted = m_action == QStringLiteral("suspend") ? m_actions.requestSuspend()
        : m_action == QStringLiteral("hibernate") ? m_actions.requestHibernate()
        : m_actions.requestPowerOff();
    if (!accepted) { m_actionPending = false; m_phase = Phase::Spent; }
}
}
