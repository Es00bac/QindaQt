// SPDX-License-Identifier: GPL-3.0-or-later
#include "chromeplancomparison.h"

namespace QindaQt::Compositor::KWinIntegration {
namespace {

using namespace HybridChrome;

// AGENT-GUARD: these sizes pin the layouts compareChromePlans() reads field by
// field (x86_64, Qt 6). When one fails, a hybrid_chrome type gained or lost a
// field: compare the new field below, then update the size. Never just bump
// the number, or a change to that field alone stops reaching the screen.
static_assert(sizeof(ChromeRenderPlan) == 1216, "compare the new ChromeRenderPlan field");
static_assert(sizeof(ChromeStyle) == 328, "compare the new ChromeStyle field");
static_assert(sizeof(ChromeMetrics) == 152, "compare the new ChromeMetrics field");
static_assert(sizeof(ChromePalette) == 176, "compare the new ChromePalette field");
static_assert(sizeof(TabGeometry) == 96, "compare the new TabGeometry field");
static_assert(sizeof(MemberGeometry) == 120, "compare the new MemberGeometry field");
static_assert(sizeof(DividerGeometry) == 96, "compare the new DividerGeometry field");
static_assert(sizeof(WindowButtonGeometry) == 88, "compare the new button field");
static_assert(sizeof(ContainerControlGeometry) == 48, "compare the new control field");

bool samePalette(const ChromePalette &a, const ChromePalette &b)
{
    return a.surface == b.surface && a.surfaceRaised == b.surfaceRaised
        && a.border == b.border && a.text == b.text && a.textMuted == b.textMuted
        && a.accent == b.accent && a.close == b.close && a.minimize == b.minimize
        && a.maximize == b.maximize && a.titleBar == b.titleBar
        && a.titleBarInactive == b.titleBarInactive;
}

bool sameStyle(const ChromeStyle &a, const ChromeStyle &b)
{
    // buttonPainter is deliberately absent; see the header's AGENT-GUARD.
    return a.buttonSide == b.buttonSide && a.tabDirection == b.tabDirection
        && a.buttonStyle == b.buttonStyle && a.hoverGlyphs == b.hoverGlyphs
        && samePalette(a.palette, b.palette) && a.material == b.material
        && a.namedButtonStyle == b.namedButtonStyle && a.buttonSize == b.buttonSize
        && a.buttonSpacing == b.buttonSpacing
        && a.titleDoubleClick == b.titleDoubleClick && a.titleLayout == b.titleLayout
        && a.deckMotionMs == b.deckMotionMs;
}

bool sameMetrics(const ChromeMetrics &a, const ChromeMetrics &b)
{
    return a.outerBorder == b.outerBorder && a.outerResizeMargin == b.outerResizeMargin
        && a.cornerRadius == b.cornerRadius && a.titleBarHeight == b.titleBarHeight
        && a.tabStripHeight == b.tabStripHeight
        && a.titleHorizontalInset == b.titleHorizontalInset
        && a.buttonExtent == b.buttonExtent && a.buttonSpacing == b.buttonSpacing
        && a.buttonClusterInset == b.buttonClusterInset
        && a.containerControlExtent == b.containerControlExtent
        && a.containerControlSpacing == b.containerControlSpacing
        && a.containerControlClusterInset == b.containerControlClusterInset
        && a.tabHorizontalInset == b.tabHorizontalInset && a.tabSpacing == b.tabSpacing
        && a.tabMinimumWidth == b.tabMinimumWidth && a.tabMaximumWidth == b.tabMaximumWidth
        && a.memberTitleHeight == b.memberTitleHeight
        && a.dividerVisualThickness == b.dividerVisualThickness
        && a.dividerHitThickness == b.dividerHitThickness;
}

bool sameButtons(const QVector<WindowButtonGeometry> &a,
                 const QVector<WindowButtonGeometry> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (qsizetype index = 0; index < a.size(); ++index) {
        const auto &left = a[index];
        const auto &right = b[index];
        if (left.action != right.action || left.rect != right.rect
            || left.fillColor != right.fillColor || left.hoverGlyph != right.hoverGlyph
            || left.glyphVisibleWhenIdle != right.glyphVisibleWhenIdle) {
            return false;
        }
    }
    return true;
}

bool sameControls(const QVector<ContainerControlGeometry> &a,
                  const QVector<ContainerControlGeometry> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (qsizetype index = 0; index < a.size(); ++index) {
        if (a[index].control != b[index].control || a[index].rect != b[index].rect
            || a[index].checked != b[index].checked) {
            return false;
        }
    }
    return true;
}

bool sameDividers(const QVector<DividerGeometry> &a, const QVector<DividerGeometry> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (qsizetype index = 0; index < a.size(); ++index) {
        if (a[index].dividerId != b[index].dividerId
            || a[index].orientation != b[index].orientation
            || a[index].visualRect != b[index].visualRect
            || a[index].hitRect != b[index].hitRect) {
            return false;
        }
    }
    return true;
}

void compareTabs(const QVector<TabGeometry> &a, const QVector<TabGeometry> &b,
                 ChromePlanDifferences *differences)
{
    if (a.size() != b.size()) {
        *differences |= ChromePlanDifference::Structure;
        return;
    }
    for (qsizetype index = 0; index < a.size(); ++index) {
        const auto &left = a[index];
        const auto &right = b[index];
        if (left.tabId != right.tabId || left.logicalIndex != right.logicalIndex
            || left.active != right.active) {
            *differences |= ChromePlanDifference::Structure;
            return;
        }
        if (left.title != right.title) {
            *differences |= ChromePlanDifference::TitleText;
        }
        if (left.rect != right.rect || left.deckDepth != right.deckDepth) {
            *differences |= ChromePlanDifference::TabGeometry;
        }
    }
}

void compareMembers(const QVector<MemberGeometry> &a, const QVector<MemberGeometry> &b,
                    ChromePlanDifferences *differences)
{
    if (a.size() != b.size()) {
        *differences |= ChromePlanDifference::Structure;
        return;
    }
    for (qsizetype index = 0; index < a.size(); ++index) {
        const auto &left = a[index];
        const auto &right = b[index];
        if (left.memberId != right.memberId || left.windowRect != right.windowRect
            || left.titleDragRect != right.titleDragRect || left.focused != right.focused) {
            *differences |= ChromePlanDifference::Structure;
            return;
        }
        if (left.title != right.title) {
            *differences |= ChromePlanDifference::UnpaintedText;
        }
    }
}

bool sameScalars(const ChromeRenderPlan &a, const ChromeRenderPlan &b)
{
    return a.containerId == b.containerId && a.devicePixelRatio == b.devicePixelRatio
        && a.borderHairline == b.borderHairline && a.maximized == b.maximized
        && a.shaded == b.shaded && a.containerFocused == b.containerFocused
        && a.memberTitlesVisible == b.memberTitlesVisible
        && a.containerTitle == b.containerTitle
        && a.containerTitleIsGenerated == b.containerTitleIsGenerated
        && a.identityColor == b.identityColor && a.indexBadge == b.indexBadge
        && a.identity == b.identity && a.tabsOverflowed == b.tabsOverflowed
        && a.badgeOverflowCount == b.badgeOverflowCount;
}

bool sameFrames(const ChromeRenderPlan &a, const ChromeRenderPlan &b)
{
    return a.outerFrame == b.outerFrame && a.outerTitleBar == b.outerTitleBar
        && a.outerTitleDragRect == b.outerTitleDragRect && a.tabStrip == b.tabStrip
        && a.contentRect == b.contentRect && a.titleTab == b.titleTab
        && a.deckPiece == b.deckPiece && a.titleLabelRect == b.titleLabelRect
        && a.badgeLabelRect == b.badgeLabelRect && a.indexBadgeRect == b.indexBadgeRect;
}

} // namespace

ChromePlanDifferences compareChromePlans(const ChromeRenderPlan &previous,
                                         const ChromeRenderPlan &next)
{
    ChromePlanDifferences differences;
    if (!sameScalars(previous, next) || !sameFrames(previous, next)
        || !sameMetrics(previous.metrics, next.metrics)
        || !sameStyle(previous.style, next.style)
        || !sameButtons(previous.buttons, next.buttons)
        || !sameControls(previous.controls, next.controls)
        || !sameDividers(previous.dividers, next.dividers)) {
        differences |= ChromePlanDifference::Structure;
    }
    compareTabs(previous.tabs, next.tabs, &differences);
    compareMembers(previous.members, next.members, &differences);
    if (previous.badgeLabelText != next.badgeLabelText) {
        differences |= ChromePlanDifference::TitleText;
    }
    return differences;
}

} // namespace QindaQt::Compositor::KWinIntegration
