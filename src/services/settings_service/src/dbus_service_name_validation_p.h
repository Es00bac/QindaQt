// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Services::SettingsService::Private {

[[nodiscard]] bool validateWellKnownServiceName(const QString &serviceName, QString *error);

} // namespace QindaQt::Services::SettingsService::Private
