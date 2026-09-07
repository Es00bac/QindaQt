// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"
#include "qindaqt/hybrid_constraints/constraint_solution.h"

#include "windowcontainer.h"

#include <QRectF>
#include <QString>

#include <functional>
#include <optional>

namespace QindaQt::Compositor::KWinIntegration {

struct HybridChromePlanOptions final
{
    qreal devicePixelRatio = 1.0;
    bool maximized = false;
    bool shaded = false;
    // Authoritative only when shaded: the strip's current logical frame
    // (HybridContainerPlacementController::shadedFrame), independent of the
    // real (frozen, untouched) committed layout's solution/outerFrame. When
    // shaded, build() uses this instead of solution entirely and produces a
    // row-only plan with no member holes or dividers.
    QRectF shadedOuterFrame;
    // KWin's active native window snapshot. The builder remains pure; the
    // session supplies ownership after sampling Workspace::activeWindow().
    bool containerFocused = false;
    QString focusedMemberId;
    bool memberTitlesVisible = true;
    // ContainerAppearance rename override; empty means "no override" (see
    // ChromeLayoutRequest::containerTitle).
    QString containerTitle;
    HybridChrome::ChromeMetrics metrics;
    HybridChrome::ChromeStyle style = HybridChrome::ChromeStyle::qindaMacOS({});
};

using HybridWindowTitleLookup = std::function<QString(const QString &windowId)>;

// Converts one already-committed active-page solution into an immutable chrome
// plan. It has no KWin or QWidget dependency, so integration code can stage a
// complete plan map before changing any visible overlay.
class HybridChromePlanBuilder final
{
public:
    // AGENT-CONTRACT: solution must have been produced for container's active
    // page with content insets matching options.metrics. A mismatch fails the
    // complete build; publishing offset paint and hit-test geometry is unsafe.
    [[nodiscard]] static std::optional<HybridChrome::ChromeRenderPlan> build(
        const Core::WindowContainer &container,
        const HybridConstraints::ConstraintSolution &solution,
        const HybridChromePlanOptions &options,
        const HybridWindowTitleLookup &titleLookup,
        QString *error = nullptr);
};

} // namespace QindaQt::Compositor::KWinIntegration
