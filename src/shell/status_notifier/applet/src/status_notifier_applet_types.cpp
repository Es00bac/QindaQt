// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_types.h"

namespace QindaQt::StatusNotifierApplet {

QString phaseToString(AppletPhase phase) noexcept
{
    switch (phase) {
    case AppletPhase::Loading:
        return QStringLiteral("loading");
    case AppletPhase::Ready:
        return QStringLiteral("ready");
    case AppletPhase::Empty:
        return QStringLiteral("empty");
    case AppletPhase::Degraded:
        return QStringLiteral("degraded");
    case AppletPhase::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

} // namespace QindaQt::StatusNotifierApplet
