// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"

namespace QindaQt::ShellClipboardApplet {

QString phaseToString(Phase phase) noexcept
{
    switch (phase) {
    case Phase::Loading:
        return QStringLiteral("loading");
    case Phase::Ready:
        return QStringLiteral("ready");
    case Phase::Degraded:
        return QStringLiteral("degraded");
    case Phase::Unavailable:
        return QStringLiteral("unavailable");
    case Phase::Locked:
        return QStringLiteral("locked");
    case Phase::Disabled:
        return QStringLiteral("disabled");
    }
    return QStringLiteral("unavailable");
}

Phase phaseFromString(const QString &phaseText) noexcept
{
    if (phaseText == QLatin1String("loading")) {
        return Phase::Loading;
    }
    if (phaseText == QLatin1String("ready")) {
        return Phase::Ready;
    }
    if (phaseText == QLatin1String("degraded")) {
        return Phase::Degraded;
    }
    if (phaseText == QLatin1String("unavailable")) {
        return Phase::Unavailable;
    }
    if (phaseText == QLatin1String("locked")) {
        return Phase::Locked;
    }
    if (phaseText == QLatin1String("disabled")) {
        return Phase::Disabled;
    }
    return Phase::Unavailable;
}

} // namespace QindaQt::ShellClipboardApplet
