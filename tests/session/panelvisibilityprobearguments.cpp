// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilityprobearguments.h"

#include <utility>

namespace QindaQt::Test::PanelVisibilityProbe {

bool parseArguments(const QStringList &arguments, Arguments *result, QString *error)
{
    if (result == nullptr || arguments.size() < 3) {
        if (error != nullptr) {
            *error = QStringLiteral("invalid panel probe arguments");
        }
        return false;
    }
    Arguments parsed;
    parsed.captureTool = arguments.at(1);
    parsed.captureLibraryPath = arguments.at(2);
    if (arguments.size() == 3) {
        *result = std::move(parsed);
        return true;
    }
    if (arguments.size() == 7 && arguments.at(3) == QStringLiteral("--control-file")
        && arguments.at(5) == QStringLiteral("--title") && !arguments.at(6).isEmpty()) {
        parsed.controlFile = arguments.at(4);
        parsed.title = arguments.at(6);
        *result = std::move(parsed);
        return true;
    }
    if (arguments.size() != 5 || arguments.at(3) != QStringLiteral("--control-file")) {
        if (error != nullptr) {
            *error = QStringLiteral("invalid panel probe arguments");
        }
        return false;
    }
    parsed.controlFile = arguments.at(4);
    *result = std::move(parsed);
    return true;
}

} // namespace QindaQt::Test::PanelVisibilityProbe
