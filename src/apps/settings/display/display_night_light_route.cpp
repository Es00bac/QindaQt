// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_night_light_route.h>

#include <qindaqt/apps/settings_display/display_night_light_model.h>
#include <qindaqt/services/night_light/night_light_config_port.h>
#include <qindaqt/services/night_light/night_light_state_port.h>
#include <qindaqt/services/night_light/night_time_schedule_monitor.h>

#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Apps::SettingsDisplay {

namespace {

Q_LOGGING_CATEGORY(lcDisplayNightLightRoute,
                   "qindaqt.settings.display.nightlight", QtInfoMsg)

QString configFilePath(const QString &fileName)
{
    // KSharedConfig::openConfig resolves bare rc names against the generic
    // config location, so kwinrc and knighttimerc both live here.
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QLatin1Char('/') + fileName;
}

} // namespace

class DisplayNightLightRoute::Private {
public:
    Private()
    {
        const QDBusConnection session = QDBusConnection::sessionBus();
        model = std::make_unique<DisplayNightLightModel>(configPort,
                                                         statePort,
                                                         scheduleMonitor);
        // AGENT-GUARD: Without a connected session bus the ports never start,
        // so no D-Bus machinery runs on busless hosts (Main.qml rows run
        // QT_FATAL_WARNINGS=1); the model renders fail-closed unavailable
        // truth, which is exactly its construction state.
        if (session.isConnected()) {
            statePort.start();
            scheduleMonitor.start();
        } else {
            qCInfo(lcDisplayNightLightRoute,
                   "no session bus; the night light section renders "
                   "unavailable truth");
        }
    }

    QindaQt::Services::NightLight::QtConfigNightLightPort configPort{
        configFilePath(QStringLiteral("kwinrc")),
        configFilePath(QStringLiteral("knighttimerc")),
        QDBusConnection::sessionBus()};
    QindaQt::Services::NightLight::QtNightLightStatePort statePort{
        QDBusConnection::sessionBus()};
    QindaQt::Services::NightLight::QtNightTimeScheduleMonitor scheduleMonitor{
        QDBusConnection::sessionBus()};
    std::unique_ptr<DisplayNightLightModel> model;
};

DisplayNightLightRoute::DisplayNightLightRoute(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

DisplayNightLightRoute::~DisplayNightLightRoute() = default;

QObject *DisplayNightLightRoute::model() const
{
    return d->model.get();
}

} // namespace QindaQt::Apps::SettingsDisplay
