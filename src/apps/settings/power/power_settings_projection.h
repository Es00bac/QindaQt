// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QString>
#include <QtCore/QVariantList>

namespace QindaQt::Apps::SettingsPower::Projection {

[[nodiscard]] QString keyboardRowId(const Power::Snapshot &snapshot,
                                    const Power::Handle &handle);
[[nodiscard]] QVariantList supplies(const Power::Snapshot &snapshot);
[[nodiscard]] QVariantList profileHolds(const Power::Snapshot &snapshot);
[[nodiscard]] QVariantList internalBrightness(const Power::Snapshot &snapshot);
[[nodiscard]] QVariantList keyboardBrightness(const Power::Snapshot &snapshot);

} // namespace QindaQt::Apps::SettingsPower::Projection
