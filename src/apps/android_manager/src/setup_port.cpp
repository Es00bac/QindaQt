// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/android_manager/setup_port.h"
namespace QindaQt::AndroidManager {
std::optional<QStringList> softwareSetupArguments(SetupTarget target)
{
    QString atom;
    switch (target) {
    case SetupTarget::Runtime: atom = QStringLiteral("app-containers/waydroid"); break;
    case SetupTarget::Images: atom = QStringLiteral("app-containers/waydroid-vanilla-images"); break;
    default: return std::nullopt;
    }
    return QStringList{QStringLiteral("--page"), QStringLiteral("packages"),
                       QStringLiteral("--select"), atom};
}
} // namespace QindaQt::AndroidManager
