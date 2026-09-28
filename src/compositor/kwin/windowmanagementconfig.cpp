// SPDX-License-Identifier: GPL-3.0-or-later
#include "windowmanagementconfig.h"

namespace QindaQt::Compositor::KWinIntegration {

std::optional<Qt::KeyboardModifiers> WindowManagementConfig::dockingModifiersFor(
    const QString &name)
{
    // AGENT-CONTRACT: Shift stays in the docking chord. The bare modifier is
    // the ADR-0282 window-management modifier (containerchords.h): it claims a
    // press only over a container and leaves every other window to KWin's own
    // CommandAll move, so the two judges never claim the same press.
    const QString trimmed = name.trimmed().toLower();
    if (trimmed == QLatin1String("disabled")) {
        return std::nullopt;
    }
    if (trimmed == QLatin1String("alt")) {
        return Qt::AltModifier | Qt::ShiftModifier;
    }
    if (trimmed == QLatin1String("control")) {
        return Qt::ControlModifier | Qt::ShiftModifier;
    }
    return Qt::MetaModifier | Qt::ShiftModifier;
}

WindowManagementConfig WindowManagementConfig::fromEntries(const QString &dockingModifier,
                                                           const QString &closeContainerPolicy,
                                                           const QString &sessionRestore)
{
    WindowManagementConfig config;
    config.dockingModifiers = dockingModifiersFor(dockingModifier);
    const QString policy = closeContainerPolicy.trimmed().toLower();
    if (policy == QLatin1String("close-all")) {
        config.closeDecision = ContainerCloseDecision::CloseAll;
    } else if (policy == QLatin1String("ungroup")) {
        config.closeDecision = ContainerCloseDecision::Ungroup;
    } else {
        config.closeDecision = std::nullopt;
    }
    const QString restore = sessionRestore.trimmed().toLower();
    config.sessionRestore = !(restore == QLatin1String("false") || restore == QLatin1String("0")
                              || restore == QLatin1String("no") || restore == QLatin1String("off"));
    return config;
}

} // namespace QindaQt::Compositor::KWinIntegration
