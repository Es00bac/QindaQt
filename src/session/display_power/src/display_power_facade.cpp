// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/session/display_power/display_power_facade.h>
#include <qindaqt/session/display_power/scoped_display_power.h>
#include <qindaqt/session/native_lock_runtime/native_lock_runtime.h>
#include <qindaqt/services/power_client/power_client.h>
#include <QPointer>
#include <QRegularExpression>
#include <QUuid>
#include <utility>
namespace QindaQt::Session::DisplayPower {
DisplayPowerFacade::DisplayPowerFacade(ScopedDisplayPower &display, Power::PowerClient &power,
    NativeLockRuntime::Runtime &lock, std::function<std::optional<bool>()> lockBeforeOff, QObject *parent)
    : DisplayPowerPort(parent), m_display(display), m_power(power), m_lock(lock), m_preferences(std::move(lockBeforeOff)) {
    connect(&display, &ScopedDisplayPower::availabilityChanged, this, [this](bool) { Q_EMIT availabilityChanged(available()); });
    connect(&display, &ScopedDisplayPower::powerChanged, this, &DisplayPowerPort::powerChanged);
    connect(&display, &ScopedDisplayPower::acquisitionFinished, this, &DisplayPowerFacade::finished);
    connect(&display, &ScopedDisplayPower::causeEnded, this, [this](const QString &cause, const QString &) {
        if (cause == m_lidCause) {
            m_lidCause.clear();
            if (!m_lidDelivered) { m_lidDelivered = true; Q_EMIT screenOffFinished(m_lidId, false); }
            Q_EMIT screenOffEnded(m_lidId);
        }
    });
    connect(&lock, &NativeLockRuntime::Runtime::suspendDispatchFinished, this, [this](bool dispatched) {
        if (dispatched || m_guarding.isEmpty()) return;
        const auto cause = m_guarding; m_guarding.clear(); finished(cause, false);
    });
    const auto powerChanged = [this] {
        if (!m_lidId.isEmpty() && (!m_power.hasSnapshot() || m_power.owner() != m_lidCaller
            || m_power.snapshot().epoch != m_lidEpoch)) releaseScreenOff(m_lidCaller, m_lidId);
        Q_EMIT availabilityChanged(available());
    };
    connect(&power, &Power::PowerClient::snapshotChanged, this, powerChanged);
    connect(&power, &Power::PowerClient::stateChanged, this, powerChanged);
}
DisplayPowerFacade::~DisplayPowerFacade() {
    requestOn();
    if (!m_lidId.isEmpty()) releaseScreenOff(m_lidCaller, m_lidId);
}
void DisplayPowerFacade::refreshPreferences() {
    if (!available()) { requestOn(); if (!m_lidId.isEmpty()) releaseScreenOff(m_lidCaller, m_lidId); }
    Q_EMIT availabilityChanged(available());
}
bool DisplayPowerFacade::available() const { return m_display.available() && m_preferences && m_preferences().has_value(); }
bool DisplayPowerFacade::canScreenOff() const { return available() && m_power.hasSnapshot() && !m_power.owner().isEmpty(); }
bool DisplayPowerFacade::callerAllowed(const QString &actualCaller) const { return canScreenOff() && actualCaller == m_power.owner(); }
bool DisplayPowerFacade::submit(const QString &cause) {
    const auto preference = m_preferences ? m_preferences() : std::nullopt;
    if (!available() || !preference) return false;
    if (!*preference) return m_display.acquire(cause);
    if (!m_guarding.isEmpty()) return false;
    m_guarding = cause;
    const quint64 serial = ++m_guardSerial;
    const QPointer<DisplayPowerFacade> guard(this);
    const bool accepted = m_lock.requestSuspend([guard, cause, serial] {
        if (!guard || guard->m_guardSerial != serial || guard->m_guarding != cause) return;
        guard->m_guarding.clear();
        if (!guard->available() || !guard->m_display.acquire(cause)) guard->finished(cause, false);
    });
    if (!accepted && m_guarding == cause) { m_guarding.clear(); finished(cause, false); }
    return accepted;
}
void DisplayPowerFacade::releaseCause(const QString &cause) {
    if (cause.isEmpty()) return;
    if (m_guarding == cause) {
        m_guarding.clear(); ++m_guardSerial; m_lock.cancelSuspend();
    }
    m_display.release(cause);
}
void DisplayPowerFacade::requestOff() {
    if (!m_idleCause.isEmpty()) return;
    m_idleCause = QUuid::createUuid().toString(QUuid::Id128);
    if (!submit(m_idleCause)) finished(m_idleCause, false);
}
void DisplayPowerFacade::requestOn() {
    const auto cause = m_idleCause; m_idleCause.clear(); releaseCause(cause);
}
void DisplayPowerFacade::restoreAndStop() {
    requestOn();
    if (!m_lidId.isEmpty()) releaseScreenOff(m_lidCaller, m_lidId);
    m_display.stop(true);
}
bool DisplayPowerFacade::requestScreenOff(const QString &actualCaller, quint64 powerEpoch, const QString &id) {
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{32}$"));
    if (!canScreenOff() || actualCaller != m_power.owner() || powerEpoch == 0
        || powerEpoch != m_power.snapshot().epoch || !pattern.match(id).hasMatch() || !m_lidId.isEmpty()) return false;
    m_lidCaller = actualCaller; m_lidId = id; m_lidEpoch = powerEpoch; m_lidDelivered = false;
    m_lidCause = QUuid::createUuid().toString(QUuid::Id128);
    if (!submit(m_lidCause)) { finished(m_lidCause, false); return false; }
    return true;
}
bool DisplayPowerFacade::releaseScreenOff(const QString &actualCaller, const QString &id) {
    if (actualCaller.isEmpty() || actualCaller != m_lidCaller || id != m_lidId) return false;
    const auto cause = m_lidCause, publicId = m_lidId;
    const bool delivered = m_lidDelivered;
    m_lidCause.clear(); m_lidId.clear(); m_lidCaller.clear(); m_lidEpoch = 0;
    releaseCause(cause);
    if (!delivered) Q_EMIT screenOffFinished(publicId, false);
    Q_EMIT screenOffEnded(publicId); return true;
}
void DisplayPowerFacade::finished(const QString &cause, bool admitted) {
    if (cause == m_lidCause && !m_lidDelivered) {
        m_lidDelivered = true; Q_EMIT screenOffFinished(m_lidId, admitted);
    }
    if (cause == m_idleCause) Q_EMIT requestFinished(admitted);
}
}
