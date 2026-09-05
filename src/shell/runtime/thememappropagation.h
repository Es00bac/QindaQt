// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointer>
#include <QQuickWindow>
#include <QVariantMap>

namespace QindaQt::Shell {

// Pushes the confirmed theme map onto every live window's `theme` property and
// prunes destroyed entries. Shared by the panel window factory and the
// notification window controller so a live appearance.theme/fonts.family
// preference change reaches existing surfaces identically; newly created
// windows receive the callers' already-updated cached map.
inline void propagateThemeMapToWindows(const QVariantMap &theme,
                                       QList<QPointer<QQuickWindow>> &windows)
{
    for (auto it = windows.begin(); it != windows.end();) {
        if (it->isNull()) {
            it = windows.erase(it);
            continue;
        }
        (*it)->setProperty("theme", theme);
        ++it;
    }
}

} // namespace QindaQt::Shell
