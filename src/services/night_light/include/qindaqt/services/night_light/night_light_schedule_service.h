// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/night_light/night_light_schedule.h>

#include <QDBusConnection>
#include <QObject>
#include <QStringList>
#include <optional>

namespace QindaQt::Services::SettingsClient { class SettingsClient; }
namespace QindaQt::DisplayClient { class Client; }

namespace QindaQt::Services::NightLight {

enum class ScheduleServiceStart { Started, NameAlreadyOwned, Failed };
[[nodiscard]] const QStringList &nightLightSettingKeys();

// Resident session-bus authority. It borrows one ready SettingsClient and one
// constructing session-bus connection; both must outlive this service.
class NightLightScheduleService final : public QObject {
    Q_OBJECT
public:
    NightLightScheduleService(const QDBusConnection &connection,
                              QindaQt::Services::SettingsClient::SettingsClient &settings,
                              QindaQt::DisplayClient::Client &display,
                              QObject *parent = nullptr);
    ~NightLightScheduleService() override;
    [[nodiscard]] ScheduleServiceStart start(QString *error = nullptr);
    void stop();

private:
    void refreshSettings();
    void publish();
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::NightLight
