// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

class QWindow;

namespace QindaQt::Test {

struct PluginUnloadResult final
{
    bool grouped = false;
    bool unloadCallSucceeded = false;
    bool serviceRemoved = false;
    bool pluginRemoved = false;
    bool framesRestored = false;
    bool clientsUsable = false;
    QString failure;
    QJsonObject evidence;
};

// AGENT-CONTRACT: This workflow observes post-unload state only through KWin's
// stable core D-Bus API and the client-side QWindows. It must not retain a
// QindaQt endpoint proxy as evidence after UnloadPlugin returns.
[[nodiscard]] PluginUnloadResult exercisePluginUnload(QWindow &primary,
                                                      QWindow &secondary,
                                                      QWindow &tertiary,
                                                      QWindow &quaternary);

} // namespace QindaQt::Test
