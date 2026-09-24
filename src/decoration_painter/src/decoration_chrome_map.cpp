// SPDX-License-Identifier: GPL-3.0-or-later
// The DecorationChrome <-> QVariantMap codec: the map the compositor publishes
// to the decoration plugin (`qindaqtChromePalette`) and the Appearance
// previews paint from. Tolerant on read (a missing or malformed key decodes
// to its default), minimal on write (defaults are omitted) so a schema v1
// theme's map stays byte-identical to what shipped before theming v2.
#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QMetaType>
#include <QStringList>
#include <QVariantMap>

#include <cmath>

namespace QindaQt::Decoration {
namespace {

QColor mapColor(const QVariantMap &map, const char *key)
{
    const auto value = map.value(QString::fromLatin1(key));
    if (value.canConvert<QColor>()) {
        const auto color = value.value<QColor>();
        return color.isValid() ? color : QColor();
    }
    return {};
}

} // namespace

DecorationChrome DecorationChrome::fromVariantMap(const QVariantMap &map)
{
    DecorationChrome chrome;
    chrome.surface = mapColor(map, "surface");
    chrome.surfaceRaised = mapColor(map, "surfaceRaised");
    chrome.border = mapColor(map, "border");
    chrome.text = mapColor(map, "text");
    chrome.textMuted = mapColor(map, "textMuted");
    chrome.close = mapColor(map, "close");
    chrome.minimize = mapColor(map, "minimize");
    chrome.maximize = mapColor(map, "maximize");
    const auto style = map.value(QStringLiteral("buttonStyle"));
    if (style.metaType().id() == QMetaType::QString && !style.toString().isEmpty()) {
        chrome.buttonStyle = style.toString();
    }
    const auto token = [&map](const char *name, const QStringList &allowed,
                              const QString &fallback) {
        const auto value = map.value(QString::fromLatin1(name));
        return value.metaType().id() == QMetaType::QString && allowed.contains(value.toString())
            ? value.toString() : fallback;
    };
    chrome.buttonSide = token("buttonSide", {QStringLiteral("left"), QStringLiteral("right")}, {});
    chrome.buttons = token("buttons", {QStringLiteral("all"), QStringLiteral("minimize-close"),
                                       QStringLiteral("close")},
                           QStringLiteral("all"));
    chrome.titleAlignment = token("titleAlignment",
                                  {QStringLiteral("center"), QStringLiteral("left")},
                                  QStringLiteral("center"));
    chrome.titleBar = mapColor(map, "titleBar");
    chrome.titleBarInactive = mapColor(map, "titleBarInactive");
    chrome.restore = mapColor(map, "restore");
    chrome.identityColor = mapColor(map, "identityColor");
    chrome.memberFocused = map.value(QStringLiteral("memberFocused")).toBool();
    // Theming v2 material (ADR-0207): tolerant, bounded, default when absent.
    const auto number = [&map](const char *name, double minimum, double maximum, double fallback) {
        const auto value = map.value(QString::fromLatin1(name));
        if (!value.canConvert<double>() || value.metaType().id() == QMetaType::QString
            || value.metaType().id() == QMetaType::Bool) {
            return fallback;
        }
        const double decoded = value.toDouble();
        return std::isfinite(decoded) && decoded >= minimum && decoded <= maximum ? decoded
                                                                                  : fallback;
    };
    chrome.titleOpacity = number("titleOpacity", 0.0, 1.0, 1.0);
    chrome.titleBlur = map.value(QStringLiteral("titleBlur")).toBool();
    chrome.titleHighlight = map.value(QStringLiteral("titleHighlight")).toBool();
    chrome.titleTint = mapColor(map, "titleTint");
    chrome.cornerRadius = number("cornerRadius", 0.0, 32.0, DecorationCornerRadius);
    chrome.shadowExtent = number("shadowExtent", 0.0, 48.0, 12.0);
    chrome.shadowOpacity = number("shadowOpacity", 0.0, 1.0, 0.30);
    chrome.handleStyle = token("handleStyle",
                               {QStringLiteral("grip"), QStringLiteral("dots"),
                                QStringLiteral("plain")},
                               QStringLiteral("grip"));
    // Title-bar options (ADR-0264): tolerant, bounded, default when absent.
    chrome.buttonScale = number("buttonScale", 0.5, 2.0, 1.0);
    chrome.spacingScale = number("spacingScale", 0.0, 3.0, 1.0);
    chrome.titleHeight = number("titleHeight", 0.0, 64.0, 0.0);
    chrome.titleWeight = static_cast<int>(number("titleWeight", 0.0, 900.0, 0.0));
    chrome.appIcon = map.value(QStringLiteral("appIcon")).toBool();
    chrome.rollUpButton = map.value(QStringLiteral("rollUpButton")).toBool();
    chrome.titleDoubleClick = token("titleDoubleClick",
                                    {QStringLiteral("maximize"), QStringLiteral("roll-up"),
                                     QStringLiteral("minimize")},
                                    {});
    // Theme-authored behaviour and finish (ADR-0268).
    chrome.minimizeRollsUp = map.value(QStringLiteral("minimizeRollsUp")).toBool();
    chrome.titleWorn = map.value(QStringLiteral("titleWorn"), true).toBool();
    return chrome;
}

QVariantMap DecorationChrome::toVariantMap() const
{
    QVariantMap map{{QStringLiteral("surface"), surface},
                    {QStringLiteral("surfaceRaised"), surfaceRaised},
                    {QStringLiteral("border"), border},
                    {QStringLiteral("text"), text},
                    {QStringLiteral("textMuted"), textMuted},
                    {QStringLiteral("close"), close},
                    {QStringLiteral("minimize"), minimize},
                    {QStringLiteral("maximize"), maximize},
                    {QStringLiteral("buttonStyle"), buttonStyle}};
    if (!buttonSide.isEmpty()) {
        map.insert(QStringLiteral("buttonSide"), buttonSide);
    }
    if (buttons != QLatin1String("all")) {
        map.insert(QStringLiteral("buttons"), buttons);
    }
    if (titleAlignment != QLatin1String("center")) {
        map.insert(QStringLiteral("titleAlignment"), titleAlignment);
    }
    if (titleBar.isValid()) {
        map.insert(QStringLiteral("titleBar"), titleBar);
    }
    if (titleBarInactive.isValid()) {
        map.insert(QStringLiteral("titleBarInactive"), titleBarInactive);
    }
    if (restore.isValid()) {
        map.insert(QStringLiteral("restore"), restore);
    }
    // Identity emphasis keys are additive and optional (ADR-0139): absent
    // keys keep neutral members byte-identical.
    if (identityColor.isValid()) {
        map.insert(QStringLiteral("identityColor"), identityColor);
    }
    if (memberFocused) {
        map.insert(QStringLiteral("memberFocused"), true);
    }
    // Material keys (ADR-0207) are additive and omitted at their defaults, so
    // a v1 theme's published map stays byte-identical.
    if (!qFuzzyCompare(titleOpacity, 1.0)) {
        map.insert(QStringLiteral("titleOpacity"), titleOpacity);
    }
    if (titleBlur) {
        map.insert(QStringLiteral("titleBlur"), true);
    }
    if (titleHighlight) {
        map.insert(QStringLiteral("titleHighlight"), true);
    }
    if (titleTint.isValid()) {
        map.insert(QStringLiteral("titleTint"), titleTint);
    }
    if (!qFuzzyCompare(cornerRadius, DecorationCornerRadius)) {
        map.insert(QStringLiteral("cornerRadius"), cornerRadius);
    }
    if (!qFuzzyCompare(shadowExtent, 12.0)) {
        map.insert(QStringLiteral("shadowExtent"), shadowExtent);
    }
    if (!qFuzzyCompare(shadowOpacity, 0.30)) {
        map.insert(QStringLiteral("shadowOpacity"), shadowOpacity);
    }
    if (handleStyle != QLatin1String("grip")) {
        map.insert(QStringLiteral("handleStyle"), handleStyle);
    }
    // Title-bar options (ADR-0264), omitted at their defaults.
    if (!qFuzzyCompare(buttonScale, 1.0)) {
        map.insert(QStringLiteral("buttonScale"), buttonScale);
    }
    if (!qFuzzyCompare(spacingScale, 1.0)) {
        map.insert(QStringLiteral("spacingScale"), spacingScale);
    }
    if (titleHeight > 0.0) {
        map.insert(QStringLiteral("titleHeight"), titleHeight);
    }
    if (titleWeight > 0) {
        map.insert(QStringLiteral("titleWeight"), titleWeight);
    }
    if (appIcon) {
        map.insert(QStringLiteral("appIcon"), true);
    }
    if (rollUpButton) {
        map.insert(QStringLiteral("rollUpButton"), true);
    }
    if (!titleDoubleClick.isEmpty()) {
        map.insert(QStringLiteral("titleDoubleClick"), titleDoubleClick);
    }
    // Theme-authored behaviour and finish (ADR-0268), omitted at defaults.
    if (minimizeRollsUp) {
        map.insert(QStringLiteral("minimizeRollsUp"), true);
    }
    if (!titleWorn) {
        map.insert(QStringLiteral("titleWorn"), false);
    }
    return map;
}

} // namespace QindaQt::Decoration
