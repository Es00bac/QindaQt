// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwintouchedgereserver.h"

#include <effect/globals.h>
#include <screenedge.h>
#include <workspace.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

KWin::ElectricBorder border(TouchEdge edge)
{
    switch (edge) {
    case TouchEdge::Left: return KWin::ElectricLeft;
    case TouchEdge::Top: return KWin::ElectricTop;
    case TouchEdge::Right: return KWin::ElectricRight;
    case TouchEdge::Bottom: return KWin::ElectricBottom;
    }
    return KWin::ElectricNone;
}

KWin::ScreenEdges *edges()
{
    return KWin::workspace() != nullptr ? KWin::workspace()->screenEdges() : nullptr;
}

} // namespace

bool KWinTouchEdgeReserver::available()
{
    return edges() != nullptr;
}

void KWinTouchEdgeReserver::reserve(TouchEdge edge, QAction *action)
{
    if (auto *screenEdges = edges()) {
        screenEdges->reserveTouch(border(edge), action);
    }
}

void KWinTouchEdgeReserver::unreserve(TouchEdge edge, QAction *action)
{
    if (auto *screenEdges = edges()) {
        screenEdges->unreserveTouch(border(edge), action);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
