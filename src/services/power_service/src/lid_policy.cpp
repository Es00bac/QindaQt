// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/power_service/lid_policy.h>
#include <qindaqt/services/session_actions/session_actions_client.h>
#include <QTimer>
namespace QindaQt::Power {
QStringList LidPolicy::settingsKeys() {
    QStringList keys;
    for (const auto &source : {QStringLiteral("ac"), QStringLiteral("battery"), QStringLiteral("lowBattery")}) {
        keys << QStringLiteral("power.lid.") + source + QStringLiteral(".action")
             << QStringLiteral("power.lid.") + source + QStringLiteral(".dockedAction");
    }
    return keys;
}
LidPolicy::LidPolicy(PowerServiceCoordinator &power,
    Services::SettingsClient::SettingsClient &settings, LidHandlingAuthority &handling,
    Services::SessionActions::SessionActionsClient &actions, QObject *parent)
    : QObject(parent), m_power(power), m_settings(settings), m_handling(handling), m_actions(actions) {
    connect(&power, &PowerServiceCoordinator::snapshotChanged, this, &LidPolicy::observe);
    connect(&settings, &Services::SettingsClient::SettingsClient::stateChanged, this, &LidPolicy::observe);
    connect(&settings, &Services::SettingsClient::SettingsClient::snapshotChanged, this, &LidPolicy::observe);
    connect(&settings, &Services::SettingsClient::SettingsClient::ownerChanged, this, &LidPolicy::observe);
    connect(&settings, &Services::SettingsClient::SettingsClient::writeAdmissionChanged, this, &LidPolicy::observe);
    connect(&handling, &LidHandlingAuthority::admissionChanged, this, &LidPolicy::observe);
    connect(&actions, &Services::SessionActions::SessionActionsClient::availabilityChanged, this, &LidPolicy::observe);
    connect(&actions, &Services::SessionActions::SessionActionsClient::actionFinished, this,
        [this](const Services::SessionActions::SessionActionResult &result) {
            if (!m_pending) return;
            m_pending = false;
            if (result.status == Services::SessionActions::ActionStatus::Uncertain) {
                m_quarantined = true;
                m_armed = false;
                m_handling.setEnabled(false);
            }
        });
}
LidPolicy::~LidPolicy() { m_native = false; m_armed = false; m_handling.setEnabled(false); m_actions.stop(); }
void LidPolicy::setNativeAuthority(bool admitted) {
    if (m_native == admitted) return;
    m_native = admitted;
    observe();
}
QString LidPolicy::preferenceKey(QString *action) const {
    using Services::SettingsClient::ClientState;
    const auto &s = m_settings.snapshot();
    const auto &p = m_power.snapshot();
    if (!m_native || m_quarantined || m_settings.state() != ClientState::Ready || !s
        || s->owner != m_settings.currentOwner()
        || !p.capabilities.testFlag(Capability::Supplies)) return {};
    QString source;
    if (p.source.onBattery) {
        const auto warning = p.composite.warning;
        source = warning == WarningLevel::Low || warning == WarningLevel::Critical || warning == WarningLevel::Action
            ? QStringLiteral("lowBattery") : QStringLiteral("battery");
    } else if (p.source.acPresent) source = QStringLiteral("ac");
    else return {};
    const auto key = QStringLiteral("power.lid.") + source
        + (p.source.docked ? QStringLiteral(".dockedAction") : QStringLiteral(".action"));
    if (!m_settings.canSetUserValue(key)) return {};
    const auto value = s->values.value(key);
    if (value.metaType() != QMetaType::fromType<QString>()) return {};
    const auto selected = value.toString();
    if (!QStringList{QStringLiteral("none"), QStringLiteral("suspend"), QStringLiteral("hibernate"),
                    QStringLiteral("lock"), QStringLiteral("power-off"), QStringLiteral("screen-off")}.contains(selected)) return {};
    if (action) *action = selected;
    return s->owner + QLatin1Char('|') + s->epoch + QLatin1Char('|') + key + QLatin1Char('|') + selected;
}
QString LidPolicy::lineage() const {
    const auto &p = m_power.snapshot();
    const auto key = preferenceKey();
    if (key.isEmpty() || !m_handling.admitted() || !p.capabilities.testFlag(Capability::Lid)
        || p.source.preparingForSleep) return {};
    return key + QLatin1Char('|') + QString::number(p.epoch) + QLatin1Char('|') + QString::number(m_handling.generation());
}
bool LidPolicy::actionAvailable(const QString &action) const {
    if (action == QStringLiteral("none")) return true;
    if (action == QStringLiteral("suspend")) return m_actions.canSuspend();
    if (action == QStringLiteral("hibernate")) return m_actions.canHibernate();
    if (action == QStringLiteral("lock")) return m_actions.canLock();
    if (action == QStringLiteral("power-off")) return m_actions.canPowerOff();
    return false; // screen-off awaits its owning Display public action boundary.
}
void LidPolicy::observe() {
    QString action;
    if (preferenceKey(&action).isEmpty() || !actionAvailable(action)) m_handling.setEnabled(false);
    const auto current = lineage();
    if (current.isEmpty() || current != m_lineage || !actionAvailable(action)) {
        m_armed = false;
        // Synchronous revocation, before a queued Can reply can dispatch.
        if (m_pending) m_actions.stop();
    }
    if (m_pending && !m_power.snapshot().source.lidClosed) m_actions.stop();
    if (m_scheduled) return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this] { m_scheduled = false; reconcile(); });
}
void LidPolicy::reconcile() {
    QString action;
    const auto preference = preferenceKey(&action);
    if (preference.isEmpty()) { m_handling.setEnabled(false); m_actions.stop(); m_armed = false; m_lineage.clear(); return; }
    m_actions.start();
    m_handling.setEnabled(actionAvailable(action));
    const auto current = lineage();
    if (current.isEmpty()) { m_armed = false; m_lineage.clear(); return; }
    if (m_lineage != current) { m_armed = false; m_lineage = current; }
    if (!actionAvailable(action)) { m_armed = false; return; }
    if (!m_power.snapshot().source.lidClosed) { if (!m_pending) m_armed = true; return; }
    if (!m_armed || m_pending) return;
    // AGENT-GUARD: consume before submission, including none/rejection. Closed
    // facts, readiness and preference changes cannot manufacture another edge.
    m_armed = false;
    if (action != QStringLiteral("none") && m_power.snapshot().source.lidPresent
        && lineage() == current) dispatch(action);
}
void LidPolicy::dispatch(const QString &action) {
    m_pending = true;
    const bool accepted = action == QStringLiteral("suspend") ? m_actions.requestSuspend()
        : action == QStringLiteral("hibernate") ? m_actions.requestHibernate()
        : action == QStringLiteral("lock") ? m_actions.requestLock() : m_actions.requestPowerOff();
    if (!accepted) m_pending = false;
}
}
