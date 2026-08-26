// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "pluginunloadworkflow.h"

#include <QString>

class QWindow;

namespace QindaQt::Test {

// Groups two live clients only through the Hybrid pointer path, then unloads
// the binary KWin plugin. Post-unload assertions use KWin core and client APIs
// because retaining the removed QindaQt service would be circular evidence.
[[nodiscard]] PluginUnloadResult exerciseHybridPointerPluginUnload(
    QWindow &primary,
    QWindow &secondary,
    QWindow &page,
    QWindow &bystander,
    const QString &dotoolPath);

} // namespace QindaQt::Test
