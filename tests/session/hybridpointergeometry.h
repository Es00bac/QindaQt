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
// A point inside area, inside output, and outside occluder. Shared by
// hybridpointerraise.cpp's covered-group regression and the shaded-strip
// partial-occlusion raise proof, which both need "click the bit of shared
// chrome an unrelated window does not cover."
[[nodiscard]] std::optional<QPointF> exposedPoint(const QRectF &area,
                                                  const QRectF &occluder,
                                                  const QRectF &output);
// A point on the shared group title before any covering window exists. Mirrors
// hybridpointerraise.cpp's private inferredGroupOuterFrame/sharedTitleRect
// metrics (one outer border, then the 29px shared title row above active
// member frames; tabs share that same row rather than adding a second
// stacked one) for callers that need this before coverAndRaiseGroup runs.
[[nodiscard]] QPointF sharedTitleCenter(const ObservedWindow &first,
                                        const ObservedWindow &second);

} // namespace QindaQt::Test
