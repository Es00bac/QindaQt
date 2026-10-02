// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/idle_policy/idle_suspend_stage.h>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/power_client.h>
#include <QPointer>
namespace QindaQt::Session::IdlePolicy {
IdleSuspendStage::IdleSuspendStage(Platform::Idle::IdleObservation &idle, Power::PowerClient &power,
    IdleSleepPort &sleep, Preferences preferences, QObject *parent)
    : QObject(parent), m_idle(idle), m_power(power), m_sleep(sleep), m_preferences(std::move(preferences)) {
    connect(&idle, &Platform::Idle::IdleObservation::changed, this, &IdleSuspendStage::idleChanged);
    connect(&idle, &Platform::Idle::IdleObservation::activity, this, [this] { cancel(); m_consumed = false; });
    connect(&power, &Power::PowerClient::snapshotChanged, this, &IdleSuspendStage::apply);
    connect(&power, &Power::PowerClient::stateChanged, this, &IdleSuspendStage::apply);
    connect(&power, &Power::PowerClient::idleInhibitorStateChanged, this, &IdleSuspendStage::apply);
    connect(&sleep, &IdleSleepPort::availabilityChanged, this, &IdleSuspendStage::apply);
    connect(&sleep, &IdleSleepPort::requestFinished, this, [this](quint64 token, bool uncertain) {
        if (!m_token || m_token != token) return;
        m_token = 0; m_quarantined = m_quarantined || uncertain;
        apply();
    });
}
IdleSuspendStage::~IdleSuspendStage() { stop(); }
void IdleSuspendStage::start() { if (m_started) return; m_started = true; apply(); m_idle.refresh(); }
void IdleSuspendStage::stop() { m_started = false; cancel(); m_timeout = 0; m_idle.setTimeout(0); m_idle.revoke(); }
void IdleSuspendStage::refreshPreferences() { apply(); }
void IdleSuspendStage::cancel() {
    ++m_serial; m_querying = false;
    const auto token = m_token; m_token = 0;
    if (token) m_sleep.cancel(token);
}
bool IdleSuspendStage::suppressed() const {
    return !m_power.hasIdleInhibitorState() ||
        m_power.activeIdleInhibitorScopes().testFlag(Power::IdleInhibitorScope::IdleSuspend);
}
void IdleSuspendStage::apply() {
    if (!m_started) return;
    const auto next = m_preferences ? m_preferences() : std::nullopt;
    const auto owner = m_power.owner();
    const auto epoch = m_power.hasSnapshot() ? m_power.snapshot().epoch : 0;
    if (next != m_current || owner != m_owner || epoch != m_epoch) cancel();
    m_current = next; m_owner = owner; m_epoch = epoch;
    const bool configured = next && next->suspendAction != QStringLiteral("none") && next->suspendSeconds > 0;
    // A submitted own request temporarily makes canSuspend false; that is not
    // authority loss. Actual loss reaches requestFinished/cancel through port.
    const bool armed = configured && !m_quarantined && epoch != 0 && !owner.isEmpty() &&
        !suppressed() && (m_sleep.available() || m_token != 0 || m_submitting);
    const int timeout = armed ? next->suspendSeconds * 1000 : 0;
    if (!armed) cancel();
    if (timeout != m_timeout) { m_timeout = timeout; m_idle.setTimeout(timeout); }
    if (armed) idleChanged();
}
void IdleSuspendStage::idleChanged() {
    if (!m_started) return;
    if (!m_idle.available() || !m_idle.idle()) { cancel(); return; }
    if (m_timeout <= 0 || m_consumed || m_querying || m_token || suppressed() || !m_sleep.available()) return;
    m_consumed = true; m_querying = true;
    const auto serial = ++m_serial;
    const auto current = m_current; const auto owner = m_owner; const auto epoch = m_epoch;
    m_sleep.query(current->suspendAction, [self = QPointer<IdleSuspendStage>(this), serial, current, owner, epoch](bool capable) {
        if (!self || serial != self->m_serial) return;
        self->m_querying = false;
        if (!capable || !self->m_started || !self->m_idle.available() || !self->m_idle.idle() ||
            self->suppressed() || self->m_preferences() != current || self->m_power.owner() != owner ||
            !self->m_power.hasSnapshot() || self->m_power.snapshot().epoch != epoch) return;
        self->m_submitting = true;
        const auto token = self->m_sleep.request(current->suspendAction);
        self->m_submitting = false;
        if (serial != self->m_serial) { if (token) self->m_sleep.cancel(token); return; }
        self->m_token = token;
    });
}
}
