// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"

#include <utility>

namespace QindaQt::DesignTokens {
namespace {

QVariantMap statusPairMap(const StatusPair &pair)
{
    return {{QStringLiteral("background"), pair.background},
            {QStringLiteral("foreground"), pair.foreground}};
}

QVariantMap elevationLevelMap(const ElevationLevel &level)
{
    return {{QStringLiteral("backgroundBlur"), level.backgroundBlur},
            {QStringLiteral("blurRadius"), level.blurRadius},
            {QStringLiteral("verticalOffset"), level.verticalOffset},
            {QStringLiteral("shadowOpacity"), level.shadowOpacity}};
}

} // namespace

DesignTokens::DesignTokens(QString sourceThemeId,
                           AccessibilityInputs inputs,
                           BackgroundTokens background,
                           ForegroundTokens foreground,
                           AccentTokens accent,
                           StateTokens state,
                           QColor focusRing,
                           QColor divider,
                           QColor strongOutline,
                           StatusTokens status,
                           DangerTokens danger,
                           RadiusTokens radius,
                           SpacingTokens spacing,
                           TypeScaleTokens typeScale,
                           MotionTokens motion,
                           ElevationTokens elevation)
    : m_sourceThemeId(std::move(sourceThemeId))
    , m_inputs(inputs)
    , m_background(std::move(background))
    , m_foreground(std::move(foreground))
    , m_accent(std::move(accent))
    , m_state(std::move(state))
    , m_focusRing(std::move(focusRing))
    , m_divider(std::move(divider))
    , m_strongOutline(std::move(strongOutline))
    , m_status(std::move(status))
    , m_danger(std::move(danger))
    , m_radius(radius)
    , m_spacing(spacing)
    , m_typeScale(std::move(typeScale))
    , m_motion(motion)
    , m_elevation(elevation)
{
}

const QString &DesignTokens::sourceThemeId() const { return m_sourceThemeId; }
const AccessibilityInputs &DesignTokens::inputs() const { return m_inputs; }
const BackgroundTokens &DesignTokens::background() const { return m_background; }
const ForegroundTokens &DesignTokens::foreground() const { return m_foreground; }
const AccentTokens &DesignTokens::accent() const { return m_accent; }
const StateTokens &DesignTokens::state() const { return m_state; }
const QColor &DesignTokens::focusRing() const { return m_focusRing; }
const QColor &DesignTokens::divider() const { return m_divider; }
const QColor &DesignTokens::strongOutline() const { return m_strongOutline; }
const StatusTokens &DesignTokens::status() const { return m_status; }
const DangerTokens &DesignTokens::danger() const { return m_danger; }
const RadiusTokens &DesignTokens::radius() const { return m_radius; }
const SpacingTokens &DesignTokens::spacing() const { return m_spacing; }
const TypeScaleTokens &DesignTokens::typeScale() const { return m_typeScale; }
const MotionTokens &DesignTokens::motion() const { return m_motion; }
const ElevationTokens &DesignTokens::elevation() const { return m_elevation; }

QVariantMap DesignTokens::toVariantMap() const
{
    const QVariantMap background = {{QStringLiteral("base"), m_background.base},
                                    {QStringLiteral("raised"), m_background.raised},
                                    {QStringLiteral("highest"), m_background.highest}};
    const QVariantMap foreground = {{QStringLiteral("default"), m_foreground.defaultColor},
                                    {QStringLiteral("muted"), m_foreground.muted},
                                    {QStringLiteral("disabled"), m_foreground.disabled}};
    const QVariantMap accentValues = {{QStringLiteral("default"), m_accent.defaultColor},
                                      {QStringLiteral("fg"), m_accent.foreground},
                                      {QStringLiteral("subtle"), m_accent.subtle}};
    const QVariantMap stateValues = {{QStringLiteral("hover"), m_state.hover},
                                     {QStringLiteral("pressed"), m_state.pressed}};
    const QVariantMap focusValues = {{QStringLiteral("ring"), m_focusRing}};
    const QVariantMap outlineValues = {{QStringLiteral("divider"), m_divider},
                                       {QStringLiteral("strong"), m_strongOutline}};
    const QVariantMap statusValues = {{QStringLiteral("success"), statusPairMap(m_status.success)},
                                      {QStringLiteral("warning"), statusPairMap(m_status.warning)},
                                      {QStringLiteral("info"), statusPairMap(m_status.info)}};
    const QVariantMap dangerValues = {{QStringLiteral("default"), m_danger.defaultColor},
                                      {QStringLiteral("fg"), m_danger.foreground}};
    const QVariantMap radiusValues = {{QStringLiteral("s"), m_radius.small},
                                      {QStringLiteral("m"), m_radius.medium},
                                      {QStringLiteral("l"), m_radius.large}};
    const QVariantMap spacingValues = {{QStringLiteral("1"), m_spacing.one},
                                       {QStringLiteral("2"), m_spacing.two},
                                       {QStringLiteral("3"), m_spacing.three},
                                       {QStringLiteral("4"), m_spacing.four},
                                       {QStringLiteral("5"), m_spacing.five},
                                       {QStringLiteral("6"), m_spacing.six}};
    const QVariantMap typeValues = {{QStringLiteral("fontFamily"), m_typeScale.fontFamily},
                                    {QStringLiteral("monoFontFamily"), m_typeScale.monoFontFamily},
                                    {QStringLiteral("caption"), m_typeScale.caption},
                                    {QStringLiteral("body"), m_typeScale.body},
                                    {QStringLiteral("subtitle"), m_typeScale.subtitle},
                                    {QStringLiteral("title"), m_typeScale.title},
                                    {QStringLiteral("display"), m_typeScale.display}};
    const QVariantMap motionValues = {{QStringLiteral("instant"), m_motion.instant},
                                      {QStringLiteral("short"), m_motion.shortDuration},
                                      {QStringLiteral("base"), m_motion.base},
                                      {QStringLiteral("long"), m_motion.longDuration}};
    const QVariantMap elevationValues = {
        {QStringLiteral("1"), elevationLevelMap(m_elevation.one)},
        {QStringLiteral("2"), elevationLevelMap(m_elevation.two)},
        {QStringLiteral("3"), elevationLevelMap(m_elevation.three)}};

    return {{QStringLiteral("qstRevision"), qstRevision},
            {QStringLiteral("sourceThemeId"), m_sourceThemeId},
            {QStringLiteral("bg"), background},
            {QStringLiteral("fg"), foreground},
            {QStringLiteral("accent"), accentValues},
            {QStringLiteral("state"), stateValues},
            {QStringLiteral("focus"), focusValues},
            {QStringLiteral("outline"), outlineValues},
            {QStringLiteral("status"), statusValues},
            {QStringLiteral("danger"), dangerValues},
            {QStringLiteral("radius"), radiusValues},
            {QStringLiteral("space"), spacingValues},
            {QStringLiteral("type"), typeValues},
            {QStringLiteral("motion"), motionValues},
            {QStringLiteral("elevation"), elevationValues}};
}

} // namespace QindaQt::DesignTokens
