// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/session/window_management/window_management_apply_state_service.h"

#include "qindaqt/session/window_management/window_management_bridge.h"

#include <QDBusError>

#include <utility>

namespace QindaQt::Session::WindowManagement {

namespace {

QString phaseName(WindowManagementApplyPhase phase)
{
    switch (phase) {
    case WindowManagementApplyPhase::Unavailable:
        return QStringLiteral("unavailable");
    case WindowManagementApplyPhase::Applying:
        return QStringLiteral("applying");
    case WindowManagementApplyPhase::Applied:
        return QStringLiteral("applied");
    case WindowManagementApplyPhase::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("unavailable");
}

QVariantMap preferenceValues(const WindowManagementPreferences &preferences)
{
    return {{QStringLiteral("windowManagement.focusPolicy"),
             preferences.focusPolicy == FocusPolicy::Click
                 ? QStringLiteral("click")
                 : preferences.focusPolicy == FocusPolicy::FocusFollowsMouse
                     ? QStringLiteral("focus-follows-mouse")
                     : QStringLiteral("focus-under-mouse")},
            {QStringLiteral("windowManagement.dockingModifier"),
             WindowManagementPreferences::dockingModifierName(preferences.dockingModifier)},
            {QStringLiteral("windowManagement.snapDistance"), preferences.snapDistance},
            {QStringLiteral("windowManagement.sessionRestore"), preferences.sessionRestore},
            {QStringLiteral("windowManagement.closeContainerPolicy"),
             WindowManagementPreferences::closeContainerPolicyName(
                 preferences.closeContainerPolicy)}};
}

} // namespace

WindowManagementApplyStateService::WindowManagementApplyStateService(
    WindowManagementBridge &bridge, QDBusConnection bus, QObject *parent)
    : QObject(parent)
    , m_bridge(bridge)
    , m_bus(std::move(bus))
{
    connect(&m_bridge, &WindowManagementBridge::applyStateChanged, this,
            [this](const WindowManagementApplyState &state) {
                Q_EMIT StateChanged(encodeState(state));
            });
}

WindowManagementApplyStateService::~WindowManagementApplyStateService()
{
    stop();
}

bool WindowManagementApplyStateService::start(QString *error)
{
    if (m_started) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    if (!m_bus.isConnected()) {
        if (error != nullptr) {
            *error = QStringLiteral("The session bus is unavailable.");
        }
        return false;
    }
    if (!m_bus.registerObject(QString::fromLatin1(ObjectPath), this,
                              QDBusConnection::ExportAllSlots
                                  | QDBusConnection::ExportAllSignals)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not export WindowManagement1: %1")
                         .arg(m_bus.lastError().message());
        }
        return false;
    }
    if (!m_bus.registerService(QString::fromLatin1(ServiceName))) {
        m_bus.unregisterObject(QString::fromLatin1(ObjectPath));
        if (error != nullptr) {
            *error = QStringLiteral("Could not own WindowManagement1: %1")
                         .arg(m_bus.lastError().message());
        }
        return false;
    }
    m_started = true;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

void WindowManagementApplyStateService::stop()
{
    if (!m_started) {
        return;
    }
    m_bus.unregisterObject(QString::fromLatin1(ObjectPath));
    m_bus.unregisterService(QString::fromLatin1(ServiceName));
    m_started = false;
}

QVariantMap WindowManagementApplyStateService::GetState() const
{
    return encodeState(m_bridge.applyState());
}

void WindowManagementApplyStateService::RetryApply()
{
    m_bridge.retry();
}

QVariantMap WindowManagementApplyStateService::encodeState(
    const WindowManagementApplyState &state)
{
    QVariantMap values;
    if (state.preferences.has_value()) {
        values = preferenceValues(*state.preferences);
    }
    return {{QStringLiteral("wireVersion"), quint32(1)},
            {QStringLiteral("phase"), phaseName(state.phase)},
            {QStringLiteral("settingsOwner"), state.settingsOwner},
            {QStringLiteral("settingsEpoch"), state.settingsEpoch},
            {QStringLiteral("settingsRevision"), QVariant::fromValue(state.settingsRevision)},
            {QStringLiteral("kwinOwner"), state.kwinOwner},
            {QStringLiteral("preferences"), values},
            {QStringLiteral("message"), state.error.left(512)}};
}

} // namespace QindaQt::Session::WindowManagement
