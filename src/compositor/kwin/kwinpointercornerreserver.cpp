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
    if (object == nullptr || callback == nullptr)
        return;
    // AGENT-CONTRACT (ADR-0242): KWin invokes a bool(ElectricBorder) slot;
    // the presentation-neutral gesture has a bool() slot. Keep that Qt meta
    // call in this KWin-only adapter, so the pure gesture stays decoupled.
    m_target = object;
    m_callback = QByteArray(callback);
    if (auto *screenEdges = edges()) {
        screenEdges->reserve(KWin::ElectricTopLeft, this, "edgeTriggered");
    }
}

void KWinPointerCornerReserver::unreserve(QObject *object)
{
    if (m_target != object)
        return;
    if (auto *screenEdges = edges()) {
        screenEdges->unreserve(KWin::ElectricTopLeft, this);
    }
    m_target.clear();
    m_callback.clear();
}

bool KWinPointerCornerReserver::edgeTriggered(KWin::ElectricBorder border)
{
    if (border != KWin::ElectricTopLeft || m_target.isNull()
        || m_callback.isEmpty()) {
        return false;
    }
    bool consumed = false;
    const bool invoked = QMetaObject::invokeMethod(
        m_target.data(), m_callback.constData(), Q_RETURN_ARG(bool, consumed));
    return invoked && consumed;
}

} // namespace QindaQt::Compositor::KWinIntegration
