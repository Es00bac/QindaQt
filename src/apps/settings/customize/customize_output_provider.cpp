// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_customize/customize_output_provider.h"

#include <QHash>

namespace QindaQt::Apps::SettingsCustomize {

PrimaryOutputResolution
resolvePrimaryOutput(const CustomizeOutputSnapshot &snapshot)
{
    if (!snapshot.error.isEmpty()) {
        return {{}, snapshot.error};
    }
    QHash<QString, qsizetype> outputCounts;
    for (const auto &output : snapshot.outputs) {
        if (output.id.trimmed().isEmpty()) {
            return {{}, QStringLiteral("Display identity is unavailable")};
        }
        ++outputCounts[output.id];
    }
    if (snapshot.primaryOutputIds.isEmpty()) {
        return {{}, QStringLiteral("The primary display is not currently known")};
    }
    if (snapshot.primaryOutputIds.size() != 1) {
        return {{}, QStringLiteral("More than one display is reported as primary")};
    }
    const QString primaryId = snapshot.primaryOutputIds.constFirst();
    if (primaryId.trimmed().isEmpty()) {
        return {{}, QStringLiteral("The primary display has no stable identity")};
    }
    const qsizetype matches = outputCounts.value(primaryId);
    if (matches == 0) {
        return {{},
                QStringLiteral(
                    "The primary display is absent from the output inventory")};
    }
    if (matches != 1) {
        return {{}, QStringLiteral("The primary display identity is ambiguous")};
    }
    return {primaryId, {}};
}

} // namespace QindaQt::Apps::SettingsCustomize
