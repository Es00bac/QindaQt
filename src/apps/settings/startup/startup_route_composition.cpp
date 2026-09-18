// SPDX-License-Identifier: GPL-3.0-or-later
#include "startup_route_composition.h"

#include <qindaqt/apps/settings_startup/startup_settings_model.h>

namespace QindaQt::Apps::SettingsStartup {

class StartupRouteComposition::Private final {
public:
    Private() : model(std::make_unique<XdgAutostartStore>()) {}

    StartupSettingsModel model;
};

StartupRouteComposition::StartupRouteComposition(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
}

StartupRouteComposition::~StartupRouteComposition() = default;

QObject *StartupRouteComposition::model() const
{
    return &d->model;
}

} // namespace QindaQt::Apps::SettingsStartup
