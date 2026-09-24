// SPDX-License-Identifier: GPL-3.0-or-later
#include "windows_route_composition.h"
#include "windows_session_apply_status_client.h"

#include <qindaqt/apps/settings_windows/windows_settings_model.h>
#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsWindows {

class WindowsRouteComposition::Private final {
public:
    Private()
        : settingsTransport(QDBusConnection::sessionBus())
        , settingsClient(settingsTransport, WindowsKeys::scopedKeys())
        , model(settingsClient)
        , applyStateClient(QDBusConnection::sessionBus())
    {
        connect(&applyStateClient, &WindowsSessionApplyStatusClient::statusChanged,
                &model, &WindowsSettingsModel::setSessionApplyStatus);
        connect(&model, &WindowsSettingsModel::retrySessionApplyRequested,
                &applyStateClient, &WindowsSessionApplyStatusClient::retryApply);
        applyStateClient.start();
        // AGENT-NOTE: a failed start is the model's Unavailable truth (with
        // Retry), never a construction failure: the route must stay resident
        // under authority loss like every other Settings page.
        QString ignoredError;
        const bool started = settingsClient.start(&ignoredError);
        Q_UNUSED(started);
    }

    Services::SettingsClient::QtSettingsTransport settingsTransport;
    Services::SettingsClient::SettingsClient settingsClient;
    WindowsSettingsModel model;
    WindowsSessionApplyStatusClient applyStateClient;
};

WindowsRouteComposition::WindowsRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

WindowsRouteComposition::~WindowsRouteComposition() = default;

QObject *WindowsRouteComposition::model() const
{
    return &d->model;
}

} // namespace QindaQt::Apps::SettingsWindows
