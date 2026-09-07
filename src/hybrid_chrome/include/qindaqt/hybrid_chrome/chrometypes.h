// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QColor>
#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <optional>

namespace QindaQt::HybridChrome {
Q_NAMESPACE

enum class ButtonSide {
    Left,
    Right,
};
Q_ENUM_NS(ButtonSide)

enum class TabVisualDirection {
    LeftToRight,
    RightToLeft,
};
Q_ENUM_NS(TabVisualDirection)

enum class ButtonStyle {
    Symbols,
    TrafficLights,
};
Q_ENUM_NS(ButtonStyle)

enum class WindowAction {
    Close,
    Minimize,
    Maximize,
    Restore,
};
Q_ENUM_NS(WindowAction)

enum class ContainerControl {
    ToggleMemberTitles,
    ManagementMenu,
    // Rolls the whole container up to a compact, still-movable title strip or
    // unrolls it back to its saved frame. Deliberately distinct from
    // WindowAction::Minimize/Restore: shading never minimizes/hides members
    // and the container is never sent to the dock as one iconified entry.
    ToggleShade,
};
Q_ENUM_NS(ContainerControl)

enum class DividerOrientation {
    Vertical,
    Horizontal,
};
Q_ENUM_NS(DividerOrientation)

enum class HitKind {
    None,
    WindowButton,
    ContainerControl,
    Tab,
    Divider,
    MemberTitleDrag,
    OuterTitleDrag,
    OuterResize,
    Client,
};
Q_ENUM_NS(HitKind)

enum class DragPhase {
    Begin,
    Update,
    Commit,
    Cancel,
};
Q_ENUM_NS(DragPhase)

struct ChromePalette final
{
    QColor surface = QColor(QStringLiteral("#e7efec"));
    QColor surfaceRaised = QColor(QStringLiteral("#f7faf9"));
    QColor border = QColor(QStringLiteral("#9dafa9"));
    QColor text = QColor(QStringLiteral("#17231f"));
    QColor textMuted = QColor(QStringLiteral("#60716c"));
    QColor accent = QColor(QStringLiteral("#4daf98"));
    QColor close = QColor(QStringLiteral("#ff5f57"));
    QColor minimize = QColor(QStringLiteral("#febc2e"));
    QColor maximize = QColor(QStringLiteral("#28c840"));

    [[nodiscard]] bool isValid(QString *error = nullptr) const;
};

struct ChromeStyle final
{
    ButtonSide buttonSide = ButtonSide::Right;
    TabVisualDirection tabDirection = TabVisualDirection::LeftToRight;
    ButtonStyle buttonStyle = ButtonStyle::Symbols;
    bool hoverGlyphs = false;
    ChromePalette palette;

    // AGENT-CONTRACT: Palette values come from the resolved theme. This
    // factory owns Qinda macOS behavior without duplicating theme color data.
    [[nodiscard]] static ChromeStyle qindaMacOS(ChromePalette palette);
    [[nodiscard]] static ChromeStyle standard(ButtonSide side, ChromePalette palette = {});
};

struct ChromeMetrics final
{
    qreal outerBorder = 1.0;
    qreal outerResizeMargin = 6.0;
    qreal cornerRadius = 12.0;
    // A grouped container has one compact shared row: traffic lights, tabs,
    // and the remaining outer drag region occupy this same vertical space.
    qreal titleBarHeight = 28.0;
    qreal tabStripHeight = 32.0;
    qreal titleHorizontalInset = 10.0;
    qreal buttonExtent = 14.0;
    qreal buttonSpacing = 8.0;
    qreal buttonClusterInset = 12.0;
    qreal containerControlExtent = 16.0;
    qreal containerControlSpacing = 6.0;
    qreal containerControlClusterInset = 10.0;
    qreal tabHorizontalInset = 8.0;
    qreal tabSpacing = 4.0;
    qreal tabMinimumWidth = 72.0;
    qreal tabMaximumWidth = 220.0;
    // Kept equal to QindaDecoration's compact native title bar until theme metrics
    // cross the compositor/decoration boundary as one resolved value.
    qreal memberTitleHeight = 24.0;
    qreal dividerVisualThickness = 2.0;
    qreal dividerHitThickness = 10.0;

    [[nodiscard]] bool isValid(QString *error = nullptr) const;
    [[nodiscard]] qreal physicalHairline(qreal devicePixelRatio) const;
};

struct ChromeTabSpec final
{
    QString tabId;
    QString title;
    bool active = false;
};

struct ChromeMemberSpec final
{
    QString memberId;
    QString title;
    // AGENT-CONTRACT: This is the constraint solver's actual windowFrame,
    // never its gap-filling tileFrame. Native KDecoration owns every pixel in
    // the member title bar, including fixed/max-size centering slack.
    QRectF windowRect;
    // Focus is sampled from KWin's active native member, not from the
    // selected page. The shared renderer may use this for a paint-only cue
    // outside the transparent member frame.
    bool focused = false;
};

struct ChromeDividerSpec final
{
    QString dividerId;
    DividerOrientation orientation = DividerOrientation::Vertical;
    qreal position = 0.0;
    qreal spanStart = 0.0;
    qreal spanEnd = 0.0;
};

struct ChromeLayoutRequest final
{
    // AGENT-CONTRACT: All rectangles and divider coordinates share the outer
    // frame's logical coordinate system; platform adapters convert only once.
    QString containerId;
    QRectF outerRect;
    qreal devicePixelRatio = 1.0;
    bool maximized = false;
    // Whole-container roll-up state; see ContainerControl::ToggleShade. Does
    // not change how members/tabs are computed here, only whether the
    // optional containerTitle is painted (shaded always shows it when set).
    bool shaded = false;
    // AGENT-CONTRACT: These values are a snapshot of KWin focus ownership.
    // A container can retain an active page while another window owns focus.
    bool containerFocused = false;
    bool memberTitlesVisible = true;
    // User-chosen rename override (see ContainerAppearance). Empty means no
    // override: the shared row paints no container-level title text, leaving
    // tabs as the only page-identity presentation, exactly as before this
    // field existed.
    QString containerTitle;
    ChromeMetrics metrics;
    ChromeStyle style;
    QVector<ChromeTabSpec> tabs;
    QVector<ChromeMemberSpec> members;
    QVector<ChromeDividerSpec> dividers;
};

struct WindowButtonGeometry final
{
    WindowAction action = WindowAction::Close;
    QRectF rect;
    QColor fillColor;
    QString hoverGlyph;
    bool glyphVisibleWhenIdle = true;
};

struct ContainerControlGeometry final
{
    ContainerControl control = ContainerControl::ToggleMemberTitles;
    QRectF rect;
    bool checked = false;
};

struct TabGeometry final
{
    QString tabId;
    QString title;
    qsizetype logicalIndex = -1;
    QRectF rect;
    bool active = false;
};

struct MemberGeometry final
{
    QString memberId;
    QString title;
    QRectF windowRect;
    // Geometry-only native-decoration locator. The shared renderer may use it
    // for modified-pointer target resolution, but must neither paint nor mask
    // this region into the overlay.
    QRectF titleDragRect;
    bool focused = false;
};

struct DividerGeometry final
{
    QString dividerId;
    DividerOrientation orientation = DividerOrientation::Vertical;
    QRectF visualRect;
    QRectF hitRect;
};

struct ChromeRenderPlan final
{
    QString containerId;
    qreal devicePixelRatio = 1.0;
    qreal borderHairline = 1.0;
    bool maximized = false;
    bool shaded = false;
    bool containerFocused = false;
    bool memberTitlesVisible = true;
    QString containerTitle;
    ChromeMetrics metrics;
    ChromeStyle style;
    QRectF outerFrame;
    QRectF outerTitleBar;
    QRectF outerTitleDragRect;
    QRectF tabStrip;
    QRectF contentRect;
    QVector<WindowButtonGeometry> buttons;
    QVector<ContainerControlGeometry> controls;
    // AGENT-GUARD: Keep stable logical order. RTL changes rect assignment only;
    // reversing this vector corrupts keyboard traversal and persisted indices.
    QVector<TabGeometry> tabs;
    QVector<MemberGeometry> members;
    QVector<DividerGeometry> dividers;
    bool tabsOverflowed = false;
};

struct ChromeHitTarget final
{
    HitKind kind = HitKind::None;
    QString stableId;
    qsizetype logicalIndex = -1;
    std::optional<WindowAction> action;
    Qt::Edges resizeEdges;
    std::optional<ContainerControl> containerControl;

    [[nodiscard]] bool isInteractive() const { return kind != HitKind::None; }
    friend bool operator==(const ChromeHitTarget &, const ChromeHitTarget &) = default;
};

struct ChromeDragEvent final
{
    ChromeHitTarget target;
    DragPhase phase = DragPhase::Update;
    QPointF globalPosition;
    // Total logical displacement from the original press. A consumer can
    // derive incremental motion without inheriting QWidget pointer state.
    QPointF delta;

    friend bool operator==(const ChromeDragEvent &, const ChromeDragEvent &) = default;
};

struct ChromePaintState final
{
    bool controlsHovered = false;
    ChromeHitTarget hoveredTarget;
    ChromeHitTarget pressedTarget;
};

} // namespace QindaQt::HybridChrome

Q_DECLARE_METATYPE(QindaQt::HybridChrome::ChromeHitTarget)
Q_DECLARE_METATYPE(QindaQt::HybridChrome::ChromeDragEvent)
Q_DECLARE_METATYPE(QindaQt::HybridChrome::WindowAction)
Q_DECLARE_METATYPE(QindaQt::HybridChrome::ContainerControl)
