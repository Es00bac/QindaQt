// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "windowcontainer.h"

#include <QHash>
#include <QRectF>
#include <QSet>

namespace QindaQt::Compositor::KWinIntegration {

struct LayoutGeometry final
{
    QHash<QString, QRectF> frames;
    QSet<QString> visibleWindows;
    QSet<QString> allWindows;
};

class LayoutGeometryPlanner final
{
public:
    [[nodiscard]] static LayoutGeometry plan(const Core::WindowContainer &container,
                                             const QRectF &outerFrame);
    [[nodiscard]] static QSet<QString> windowIds(const Core::WindowContainer &container);
    [[nodiscard]] static QSet<QString> activeWindowIds(
        const Core::WindowContainer &container);
};

} // namespace QindaQt::Compositor::KWinIntegration
