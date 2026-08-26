// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSize>
#include <QString>

namespace QindaQt::Session {

enum class Backend {
    Drm,
    NestedWayland,
    Virtual,
};

struct SessionOptions final
{
    Backend backend = Backend::Drm;
    QString kwinExecutable = QStringLiteral("kwin_wayland");
    QString socketName = QStringLiteral("qindaqt-0");
    QSize outputSize{1920, 1080};
    double scale = 1.0;
    int outputCount = 1;
    bool xwayland = true;
    bool lockscreen = true;
    bool globalShortcuts = true;
    bool replace = false;
    QString parentWaylandDisplay;
    QString testScenario;
    QString sessionExecutable;
    QString pluginRoot;
};

} // namespace QindaQt::Session
