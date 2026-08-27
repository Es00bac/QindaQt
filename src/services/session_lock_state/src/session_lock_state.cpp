// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/session_lock_state/session_lock_state.h"

namespace QindaQt::Services::SessionLockState {

QString observedServiceName(ObservedService service)
{
    switch (service) {
    case ObservedService::Compositor:
        return QStringLiteral("org.qindaqt.Compositor");
    case ObservedService::FreedesktopScreenSaver:
        return QStringLiteral("org.freedesktop.ScreenSaver");
    case ObservedService::KdeScreenSaver:
        return QStringLiteral("org.kde.screensaver");
    }
    return {};
}

} // namespace QindaQt::Services::SessionLockState
