// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/notification_surface_layout.h"

#include <algorithm>

namespace QindaQt::ShellSurface {
namespace {

constexpr int DesiredPopupWidth = 400;
constexpr int PopupHeaderHeight = 38;
constexpr int PopupCardHeight = 146;
constexpr int MaximumVisiblePopups = 3;
constexpr QSize DesiredCenterSize{440, 640};
constexpr int MinimumSurfaceWidth = 240;
constexpr int MinimumCenterHeight = 240;

bool validMargins(const QMargins &margins)
{
    return margins.left() >= 0 && margins.top() >= 0 && margins.right() >= 0 &&
        margins.bottom() >= 0 && margins.left() <= 4'096 &&
        margins.top() <= 4'096 && margins.right() <= 4'096 &&
        margins.bottom() <= 4'096;
}

QSize availableSize(const QSize &output, const QMargins &margins)
{
    return {output.width() - margins.left() - margins.right(),
            output.height() - margins.top() - margins.bottom()};
}

} // namespace

NotificationSurfaceLayoutResult NotificationSurfaceLayoutPlanner::plan(
    const QSize &logicalOutputSize, const QMargins &popupMargins,
    const QMargins &centerMargins, int popupCount)
{
    if (logicalOutputSize.width() <= 0 || logicalOutputSize.height() <= 0 ||
        logicalOutputSize.width() > 16'384 ||
        logicalOutputSize.height() > 16'384 || !validMargins(popupMargins) ||
        !validMargins(centerMargins) || popupCount < 0 || popupCount > 32) {
        return {{}, QStringLiteral("notification layout input is invalid")};
    }
    const QSize popupAvailable = availableSize(logicalOutputSize, popupMargins);
    const QSize centerAvailable = availableSize(logicalOutputSize, centerMargins);
    const int visibleCount = std::clamp(std::max(1, popupCount), 1,
                                        MaximumVisiblePopups);
    const int minimumPopupHeight = PopupHeaderHeight + PopupCardHeight;
    if (popupAvailable.width() < MinimumSurfaceWidth ||
        popupAvailable.height() < minimumPopupHeight ||
        centerAvailable.width() < MinimumSurfaceWidth ||
        centerAvailable.height() < MinimumCenterHeight) {
        return {{}, QStringLiteral("logical output is too small for notifications")};
    }
    return {NotificationSurfaceLayout{
                .popupSize =
                    {std::min(DesiredPopupWidth, popupAvailable.width()),
                     std::min(PopupHeaderHeight + PopupCardHeight * visibleCount,
                              popupAvailable.height())},
                .centerSize =
                    {std::min(DesiredCenterSize.width(), centerAvailable.width()),
                     std::min(DesiredCenterSize.height(), centerAvailable.height())},
                .visiblePopupCount = visibleCount},
            {}};
}

} // namespace QindaQt::ShellSurface
