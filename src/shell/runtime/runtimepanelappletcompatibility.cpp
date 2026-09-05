// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimepanelappletcompatibility.h"

#include <algorithm>

namespace QindaQt::Shell {
namespace {

bool isLegacyTaskList(const QString &plugin)
{
    return plugin == QLatin1StringView("grouped-task-list")
        || plugin == QLatin1StringView("dock-task-list")
        || plugin == QLatin1StringView("centered-task-list");
}

} // namespace

QVector<Profiles::AppletSpec> RuntimePanelAppletCompatibility::normalize(
    const QVector<Profiles::AppletSpec> &applets)
{
    const bool hasCanonicalTaskList = std::any_of(
        applets.cbegin(), applets.cend(), [](const Profiles::AppletSpec &applet) {
            return applet.plugin == QLatin1StringView("task-list");
        });
    QVector<Profiles::AppletSpec> normalized;
    normalized.reserve(applets.size());
    for (auto applet : applets) {
        if (applet.plugin == QLatin1StringView("application-launcher")) {
            applet.plugin = QStringLiteral("launcher");
        } else if (isLegacyTaskList(applet.plugin)) {
            if (hasCanonicalTaskList) {
                continue;
            }
            applet.plugin = QStringLiteral("task-list");
        }
        normalized.append(std::move(applet));
    }
    return normalized;
}

} // namespace QindaQt::Shell
