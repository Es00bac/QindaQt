// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinpointercornerreserver.h"

#include <effect/globals.h>
#include <screenedge.h>
#include <workspace.h>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

KWin::ScreenEdges *edges()
{
    return KWin::workspace() != nullptr ? KWin::workspace()->screenEdges() : nullptr;
}

} // namespace

bool KWinPointerCornerReserver::available() { return edges() != nullptr; }

void KWinPointerCornerReserver::reserve(QObject *object, const char *callback)
{
    if (auto *screenEdges = edges()) {
        screenEdges->reserve(KWin::ElectricTopLeft, object, callback);
    }
}

void KWinPointerCornerReserver::unreserve(QObject *object)
{
    if (auto *screenEdges = edges()) {
        screenEdges->unreserve(KWin::ElectricTopLeft, object);
    }
}

} // namespace QindaQt::Compositor::KWinIntegration
