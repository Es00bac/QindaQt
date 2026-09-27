// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
class QGuiApplication;
class QWindow;
namespace QindaQt::Compositor::TestSupport {
bool provePanelOwnerFeedback(QWindow &panel, QString *failure);
int runPanelOwnerPeer(QGuiApplication &application);
}
