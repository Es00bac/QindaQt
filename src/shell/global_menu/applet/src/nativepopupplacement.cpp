// SPDX-License-Identifier: GPL-3.0-or-later

#include "nativepopupplacement.h"

#include <QtCore/QVariant>

namespace QindaQt::Shell::GlobalMenu
{

namespace
{
// xdg_positioner.constraint_adjustment bits from the xdg-shell protocol:
// slide_x=1, slide_y=2, flip_x=4, flip_y=8. The module must not link
// QtWayland, so the values are restated here; the combination mirrors
// QtWayland's Menu window-type default in createPositioner().
constexpr quint32 slideX = 1;
constexpr quint32 slideY = 2;
constexpr quint32 flipY = 8;
} // namespace

NativePopupPlacement::NativePopupPlacement(QObject *parent)
    : QObject(parent)
{
}

void NativePopupPlacement::configurePopupWindow(QWindow *window,
                                                const QRectF &anchorSceneRect) const
{
    if (window == nullptr || !anchorSceneRect.isValid())
        return;
    window->setProperty("_q_waylandPopupAnchorRect",
                        QVariant::fromValue(anchorSceneRect.toAlignedRect()));
    window->setProperty("_q_waylandPopupAnchor",
                        QVariant::fromValue(Qt::Edges(Qt::BottomEdge | Qt::LeftEdge)));
    window->setProperty("_q_waylandPopupGravity",
                        QVariant::fromValue(Qt::Edges(Qt::BottomEdge | Qt::RightEdge)));
    window->setProperty("_q_waylandPopupConstraintAdjustment",
                        QVariant::fromValue(slideX | slideY | flipY));
}

} // namespace QindaQt::Shell::GlobalMenu
