// SPDX-License-Identifier: GPL-3.0-or-later
#include "pointercorner.h"

namespace QindaQt::Compositor::KWinIntegration {

PointerCornerReserver::~PointerCornerReserver() = default;

PointerCornerGesture::PointerCornerGesture(PointerCornerReserver &reserver,
                                           QObject *parent)
    : QObject(parent), m_reserver(reserver)
{
    rearm();
}

PointerCornerGesture::~PointerCornerGesture()
{
    if (m_reserved) {
        // KWin requires reserve/unreserve to balance; an unbalanced call
        // leaves the edge permanently active or permanently dead.
        m_reserver.unreserve(this);
        m_reserved = false;
    }
}

void PointerCornerGesture::rearm()
{
    m_reserver.reserve(this, "cornerTriggered");
    m_reserved = true;
}

QString PointerCornerGesture::edgeName() { return QStringLiteral("top-left"); }

QString PointerCornerGesture::action() { return QStringLiteral("overview"); }

bool PointerCornerGesture::cornerTriggered()
{
    Q_EMIT triggered(edgeName(), action());
    return true;
}

} // namespace QindaQt::Compositor::KWinIntegration
