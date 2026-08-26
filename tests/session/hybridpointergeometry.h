// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "compositorprobeclient.h"

#include <QPointF>
#include <QRectF>
#include <QString>

#include <optional>

namespace QindaQt::Test {

struct ProbeWindowTitles;

struct DockGestureGeometry final
{
    QString sourceTitle;
    QString targetTitle;
    QString dropZone;
    QPointF sourcePoint;
    QPointF dropPoint;
};

struct SplitEvidence final
{
    bool valid = false;
    QString orientation;
    qreal dividerGap = 0.0;
};

[[nodiscard]] std::optional<DockGestureGeometry> chooseDockGesture(
    const WindowInventory &inventory,
    const ProbeWindowTitles &titles,
    const QRectF &output,
    QString *error);
[[nodiscard]] QString bystanderTitle(const ProbeWindowTitles &titles,
                                     const DockGestureGeometry &gesture);
[[nodiscard]] std::optional<QPointF> emptyDesktopPoint(
    const WindowInventory &inventory,
    const QStringList &groupedTitles,
    const QRectF &output,
    QString *error);
[[nodiscard]] SplitEvidence splitEvidence(const QRectF &first,
                                          const QRectF &second);

} // namespace QindaQt::Test
