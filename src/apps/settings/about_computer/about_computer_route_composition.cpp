// SPDX-License-Identifier: GPL-3.0-or-later
#include "about_computer_route_composition.h"

#include <qindaqt/apps/settings_about_computer/about_computer_info_reader.h>
#include <qindaqt/apps/settings_about_computer/about_computer_settings_model.h>

#include <memory>

namespace QindaQt::Apps::SettingsAboutComputer {

class AboutComputerRouteComposition::Private final {
public:
  Private() : model(std::make_unique<SystemAboutComputerInfoSource>()) {}

  AboutComputerSettingsModel model;
};

AboutComputerRouteComposition::AboutComputerRouteComposition(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>()) {}

AboutComputerRouteComposition::~AboutComputerRouteComposition() = default;

QObject *AboutComputerRouteComposition::model() const { return &d->model; }

} // namespace QindaQt::Apps::SettingsAboutComputer
