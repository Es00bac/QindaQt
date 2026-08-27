// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QVariantMap>

namespace {

bool hasExactKeys(const QVariantMap &map,
                  std::initializer_list<QString> expected,
                  const char *label)
{
    QStringList actual = map.keys();
    QStringList required(expected);
    actual.sort();
    required.sort();
    if (actual == required) {
        return true;
    }
    qCritical().noquote() << label << "keys differ; actual:" << actual.join(',')
                          << "expected:" << required.join(',');
    return false;
}

bool validatesPublicShape(const QVariantMap &tokens)
{
    if (!hasExactKeys(tokens,
                      {QStringLiteral("qstRevision"),
                       QStringLiteral("sourceThemeId"),
                       QStringLiteral("bg"),
                       QStringLiteral("fg"),
                       QStringLiteral("accent"),
                       QStringLiteral("state"),
                       QStringLiteral("focus"),
                       QStringLiteral("outline"),
                       QStringLiteral("status"),
                       QStringLiteral("danger"),
                       QStringLiteral("radius"),
                       QStringLiteral("space"),
                       QStringLiteral("type"),
                       QStringLiteral("motion"),
                       QStringLiteral("elevation")},
                      "QST-1 top-level")) {
        return false;
    }

    const struct {
        const char *group;
        std::initializer_list<QString> keys;
    } groups[] = {
        {"bg", {QStringLiteral("base"), QStringLiteral("raised"), QStringLiteral("highest")}},
        {"fg", {QStringLiteral("default"), QStringLiteral("muted"), QStringLiteral("disabled")}},
        {"accent", {QStringLiteral("default"), QStringLiteral("fg"), QStringLiteral("subtle")}},
        {"state", {QStringLiteral("hover"), QStringLiteral("pressed")}},
        {"focus", {QStringLiteral("ring")}},
        {"outline", {QStringLiteral("divider"), QStringLiteral("strong")}},
        {"status", {QStringLiteral("success"), QStringLiteral("warning"), QStringLiteral("info")}},
        {"danger", {QStringLiteral("default"), QStringLiteral("fg")}},
        {"radius", {QStringLiteral("s"), QStringLiteral("m"), QStringLiteral("l")}},
        {"space", {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                   QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6")}},
        {"type", {QStringLiteral("fontFamily"), QStringLiteral("monoFontFamily"),
                  QStringLiteral("caption"), QStringLiteral("body"),
                  QStringLiteral("subtitle"), QStringLiteral("title"),
                  QStringLiteral("display")}},
        {"motion", {QStringLiteral("instant"), QStringLiteral("short"),
                    QStringLiteral("base"), QStringLiteral("long")}},
        {"elevation", {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")}},
    };
    for (const auto &group : groups) {
        if (!hasExactKeys(tokens.value(QLatin1String(group.group)).toMap(),
                          group.keys,
                          group.group)) {
            return false;
        }
    }

    for (const auto *status : {"success", "warning", "info"}) {
        const QVariantMap pair = tokens.value(QStringLiteral("status"))
                                     .toMap()
                                     .value(QLatin1String(status))
                                     .toMap();
        if (!hasExactKeys(pair,
                          {QStringLiteral("background"), QStringLiteral("foreground")},
                          status)) {
            return false;
        }
    }
    for (const auto *level : {"1", "2", "3"}) {
        const QVariantMap elevation = tokens.value(QStringLiteral("elevation"))
                                          .toMap()
                                          .value(QLatin1String(level))
                                          .toMap();
        if (!hasExactKeys(elevation,
                          {QStringLiteral("backgroundBlur"), QStringLiteral("blurRadius"),
                           QStringLiteral("verticalOffset"), QStringLiteral("shadowOpacity")},
                          level)) {
            return false;
        }
    }
    return true;
}

bool validatesQindaMacAccessibility(const QindaQt::DesignTokens::DesignTokens &tokens)
{
    const QVariantMap map = tokens.toVariantMap();
    const QVariantMap background = map.value(QStringLiteral("bg")).toMap();
    const QVariantMap foreground = map.value(QStringLiteral("fg")).toMap();
    const QVariantMap motion = map.value(QStringLiteral("motion")).toMap();
    const QVariantMap elevationOne = map.value(QStringLiteral("elevation"))
                                         .toMap()
                                         .value(QStringLiteral("1"))
                                         .toMap();
    const bool valid = map.value(QStringLiteral("qstRevision")).toInt() == 1
        && map.value(QStringLiteral("sourceThemeId")).toString() == QStringLiteral("qinda-macos")
        && background.value(QStringLiteral("base")).value<QColor>() == QColor(QStringLiteral("#9fb8b2"))
        && background.value(QStringLiteral("raised")).value<QColor>() == QColor(QStringLiteral("#e7efec"))
        && foreground.value(QStringLiteral("default")).value<QColor>() == QColor(QStringLiteral("#17231f"))
        && tokens.focusRing() == QColor(QStringLiteral("#17231f"))
        && tokens.radius().medium == 12.0 && tokens.radius().large == 18.0
        && tokens.typeScale().body == 13.75 && tokens.motion().instant == 0
        && motion.value(QStringLiteral("short")).toInt() == 80
        && motion.value(QStringLiteral("base")).toInt() == 80
        && motion.value(QStringLiteral("long")).toInt() == 80
        && !elevationOne.value(QStringLiteral("backgroundBlur")).toBool()
        && elevationOne.value(QStringLiteral("blurRadius")).toInt() == 0
        && elevationOne.value(QStringLiteral("shadowOpacity")).toDouble() == 0.0;
    if (!valid) {
        qCritical() << "Qinda macOS accessibility projection differs from QST-1";
    }
    return valid;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        return 2;
    }

    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(application.arguments().at(1));
    if (!theme.ok) {
        return 3;
    }
    const auto result = QindaQt::DesignTokens::DesignTokenDeriver::derive(
        theme.theme,
        {.basePointSize = 11.0,
         .textScale = 1.25,
         .reducedMotion = true,
         .reducedTransparency = true,
         .highContrast = false});
    if (!result.ok()) {
        return 4;
    }
    const QVariantMap tokens = result.tokens->toVariantMap();
    if (!validatesPublicShape(tokens)) {
        return 5;
    }
    return validatesQindaMacAccessibility(*result.tokens) ? 0 : 6;
}
