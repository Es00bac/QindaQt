// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/compositor_names/compositor_names.h"

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
    // QindaQt's compositor, qindaqt-kwin (ADR-0291).
    QString kwinExecutable = QString(CompositorNames::executable);
    QString socketName = QString(CompositorNames::waylandSocketPrefix) + QStringLiteral("0");
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
