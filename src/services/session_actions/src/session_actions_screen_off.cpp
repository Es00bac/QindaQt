// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/session_actions/session_actions_client.h>
#include "session_actions_internal.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>
#include <unistd.h>
namespace QindaQt::Services::SessionActions {
namespace {
const QString Path = QStringLiteral("/org/qindaqt/ScreenPower1");
const QString Interface = QStringLiteral("org.qindaqt.ScreenPower1");
QDBusMessage screenCall(const QString &owner, const QString &method, const QList<QVariant> &args = {}) {
    auto call = QDBusMessage::createMethodCall(owner, Path, Interface, method); call.setArguments(args); return call;
}
}
QString SessionActionsClient::currentScreenPowerOwner() const {
    const auto owner = Detail::serviceOwner(m_sessionBus, "org.qindaqt.ScreenPower1");
    if (owner.isEmpty() || owner != Detail::serviceOwner(m_sessionBus, Detail::SessionService) || !m_sessionBus.interface()) return {};
    const auto uid = m_sessionBus.interface()->serviceUid(owner);
    return uid.isValid() && uid.value() == static_cast<uint>(::getuid()) ? owner : QString{};
}
void SessionActionsClient::refreshScreenOffAvailability() {
    const auto owner = currentScreenPowerOwner();
    if (!m_screenOwner.isEmpty() && owner != m_screenOwner) releaseScreenOff();
    const auto publish = [this](bool available) {
        if (m_screenOffAvailable == available) return;
        m_screenOffAvailable = available; Q_EMIT availabilityChanged();
    };
    if (owner.isEmpty()) { publish(false); return; }
    const auto serial = m_refreshSerial;
    auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(screenCall(owner, QStringLiteral("CanScreenOff")), 750), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, serial, owner, publish] {
        const QDBusPendingReply<bool> reply = *watcher; watcher->deleteLater();
        if (!m_running || serial != m_refreshSerial) return;
        publish(currentScreenPowerOwner() == owner && !reply.isError() && reply.value());
    });
}
bool SessionActionsClient::requestScreenOff(quint64 powerEpoch) {
    if (!m_running || !m_screenOffAvailable || powerEpoch == 0 || m_pending || m_screenPending || !m_screenId.isEmpty()) return false;
    const auto owner = currentScreenPowerOwner(); if (owner.isEmpty()) return false;
    m_screenOwner = owner; m_screenId = QUuid::createUuid().toString(QUuid::Id128); m_screenPending = true;
    const auto serial = ++m_screenSerial; const auto id = m_screenId;
    m_screenDeadline.start(); Q_EMIT pendingChanged();
    auto *query = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(screenCall(owner, QStringLiteral("CanScreenOff")), 750), this);
    connect(query, &QDBusPendingCallWatcher::finished, this, [this, query, owner, serial, id, powerEpoch] {
        const QDBusPendingReply<bool> admission = *query; query->deleteLater();
        if (!m_running || serial != m_screenSerial || !m_screenPending) return;
        if (currentScreenPowerOwner() != owner || admission.isError() || !admission.value()) {
            finishScreenOff(ActionStatus::Rejected); return;
        }
        auto *mutation = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(screenCall(owner, QStringLiteral("ScreenOff"),
            {QVariant::fromValue(powerEpoch), id}), ActionTimeoutMilliseconds + 1000), this);
        connect(mutation, &QDBusPendingCallWatcher::finished, this, [this, mutation, serial, owner] {
            const QDBusPendingReply<bool> result = *mutation; mutation->deleteLater();
            if (!m_running || serial != m_screenSerial || !m_screenPending) return;
            finishScreenOff(currentScreenPowerOwner() != owner || result.isError() ? ActionStatus::Uncertain
                : result.value() ? ActionStatus::Succeeded : ActionStatus::Rejected);
        });
    });
    return true;
}
void SessionActionsClient::finishScreenOff(ActionStatus status) {
    if (!m_screenPending) return;
    m_screenPending = false; m_screenDeadline.stop(); Q_EMIT pendingChanged();
    if (status != ActionStatus::Succeeded) releaseScreenOff();
    Q_EMIT screenOffFinished(status);
}
void SessionActionsClient::releaseScreenOff() {
    const auto owner = m_screenOwner, id = m_screenId; const bool pending = m_screenPending;
    m_screenOwner.clear(); m_screenId.clear(); m_screenPending = false; ++m_screenSerial; m_screenDeadline.stop();
    if (!owner.isEmpty() && !id.isEmpty() && m_sessionBus.isConnected())
        static_cast<void>(m_sessionBus.asyncCall(screenCall(owner, QStringLiteral("ReleaseScreenOff"), {id}), 750));
    if (pending) { Q_EMIT pendingChanged(); Q_EMIT screenOffFinished(ActionStatus::Unavailable); }
}
void SessionActionsClient::screenPowerChanged(const QDBusMessage &message) {
    if (m_running && message.signature().isEmpty() && message.service() == currentScreenPowerOwner()) scheduleRefresh();
}
void SessionActionsClient::screenPowerEnded(const QDBusMessage &message) {
    if (!m_running || message.service() != m_screenOwner || message.service() != currentScreenPowerOwner()
        || message.signature() != QStringLiteral("s") || message.arguments().size() != 1
        || message.arguments().first().toString() != m_screenId) return;
    if (m_screenPending) finishScreenOff(ActionStatus::Rejected);
    else { m_screenOwner.clear(); m_screenId.clear(); ++m_screenSerial; }
    Q_EMIT screenOffEnded();
}
}
