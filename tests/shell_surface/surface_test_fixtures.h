// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_solver.h"

#include <QRect>
#include <QString>

#include <utility>

namespace QindaQt::ShellSurface::TestFixtures {

inline Profiles::PanelSpec panel(QString id, Profiles::Edge edge = Profiles::Edge::Top,
                                 Profiles::Layer layer = Profiles::Layer::Above,
                                 int thickness = 30)
{
    Profiles::PanelSpec result;
    result.id = std::move(id);
    result.edge = edge;
    result.layer = layer;
    result.thickness = thickness;
    return result;
}

inline ShellLayout::LogicalOutput output(QString id = QStringLiteral("main"),
                                         QRect geometry = {0, 0, 1920, 1080},
                                         qreal scale = 1.0)
{
    return {std::move(id), geometry, scale};
}

inline ShellLayout::PanelLayoutResult solve(
    const QVector<Profiles::PanelSpec> &panels,
    const QVector<ShellLayout::LogicalOutput> &outputs = {output()})
{
    return ShellLayout::PanelLayoutSolver::solve(panels, outputs);
}

} // namespace QindaQt::ShellSurface::TestFixtures
