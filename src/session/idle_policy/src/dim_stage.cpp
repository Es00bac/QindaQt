// SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
// SPDX-License-Identifier: GPL-2.0-or-later
#include <qindaqt/session/idle_policy/dim_stage.h>
#include <qindaqt/platform/idle_observation/idle_observation.h>
#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/power_protocol/power_backlight_selection.h>
namespace QindaQt::Session::IdlePolicy {
DimStage::DimStage(Platform::Idle::IdleObservation &idle, Power::PowerClient &power,
    Preferences preferences, QObject *parent)
    : QObject(parent), m_idle(idle), m_power(power), m_preferences(std::move(preferences)) {
    connect(&idle, &Platform::Idle::IdleObservation::changed, this, &DimStage::apply);
    connect(&idle, &Platform::Idle::IdleObservation::activity, this, [this] { m_consumed = false; restore(); });
    connect(&power, &Power::PowerClient::snapshotChanged, this, &DimStage::apply);
    connect(&power, &Power::PowerClient::stateChanged, this, &DimStage::apply);
    connect(&power, &Power::PowerClient::idleInhibitorStateChanged, this, &DimStage::apply);
    connect(&power, &Power::PowerClient::operationCompleted, this, [this](quint64 id, const Power::OperationResult &result) {
        if (id != m_operation || !m_operation) return;
        m_operation = 0;
        m_minRevision = result.observedRevision;
        if (result.status == Power::OperationStatus::Uncertain) { m_quarantined = true; clearOwned(); }
        else if (m_restoring || result.status != Power::OperationStatus::Succeeded) clearOwned();
        else if (m_restoreWanted) restore();
    });
}
DimStage::~DimStage() { stop(); }
void DimStage::start() { if (m_started) return; m_started = true; apply(); m_idle.refresh(); }
void DimStage::stop() { m_started = false; m_timeout = 0; m_idle.setTimeout(0); m_idle.revoke(); restore(); }
void DimStage::refreshPreferences() { apply(); }
void DimStage::clearOwned() { m_device = {}; m_minRevision = 0; m_before = m_dim = 0; m_restoreWanted = m_restoring = false; }
bool DimStage::suppressed() const {
    return !m_power.hasIdleInhibitorState() ||
        m_power.activeIdleInhibitorScopes().testFlag(Power::IdleInhibitorScope::DisplayOff);
}
void DimStage::restore() {
    if (!m_device.isValid()) return;
    if (m_operation) { m_restoreWanted = true; return; }
    if (!m_power.hasSnapshot() || m_power.owner() != m_owner || m_power.snapshot().epoch != m_epoch) { clearOwned(); return; }
    // A method receipt can precede its authenticated readback snapshot.
    // Defer restoration until that revision arrives; never infer new value.
    if (m_power.snapshot().revision < m_minRevision) { m_restoreWanted = true; return; }
    for (const auto &device : m_power.snapshot().internalBacklights) {
        if (device.handle != m_device) continue;
        if (!device.observedKnown || device.observed != m_dim || m_power.operationPending() ||
            Power::internalBrightnessAdmission(m_power.snapshot().internalBacklights, device.handle.opaqueId) != Power::InternalBrightnessAdmission::Admitted) {
            clearOwned(); return;
        }
        m_restoring = true; m_restoreWanted = false;
        m_operation = m_power.setInternalBrightness(m_device, m_before);
        if (!m_operation) clearOwned();
        return;
    }
    clearOwned();
}
void DimStage::apply() {
    if (!m_started) return;
    if (m_restoreWanted && !m_operation) restore();
    const auto next = m_preferences ? m_preferences() : std::nullopt;
    if (next != m_current) { restore(); m_current = next; }
    const bool armed = next && next->dimEnabled && next->dimSeconds > 0 && !m_quarantined &&
        !suppressed() && m_power.hasSnapshot() && !m_power.owner().isEmpty();
    const int timeout = armed ? next->dimSeconds * 1000 : 0;
    if (timeout != m_timeout) { m_timeout = timeout; m_idle.setTimeout(timeout); }
    if (!armed || !m_idle.available() || !m_idle.idle()) { restore(); return; }
    if (m_consumed || m_operation || m_device.isValid() || m_power.operationPending()) return;
    m_consumed = true;
    const auto snapshot = m_power.snapshot();
    for (const auto &device : snapshot.internalBacklights) {
        if (Power::internalBrightnessAdmission(snapshot.internalBacklights, device.handle.opaqueId) != Power::InternalBrightnessAdmission::Admitted) continue;
        // PowerDevil's DimDisplay policy uses 30 percent of the configured
        // current value. Preserve that behavior without increasing low values.
        const auto dim = static_cast<quint32>(static_cast<quint64>(device.observed) * 3 / 10);
        if (dim >= device.observed) return;
        m_owner = m_power.owner(); m_epoch = snapshot.epoch; m_device = device.handle;
        m_before = device.observed; m_dim = dim; m_restoring = m_restoreWanted = false;
        m_operation = m_power.setInternalBrightness(m_device, dim);
        if (!m_operation) clearOwned();
        return;
    }
}
}
