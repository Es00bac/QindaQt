// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

#include <array>
#include <cmath>
#include <limits>

using namespace QindaQt::DesignTokens;
using namespace QindaQt::Themes;

namespace {

ThemeSpec builtIn(const QString &fileName = QStringLiteral("qinda-dark.json"))
{
    const auto result = ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + fileName);
    if (!result.ok) {
        qFatal("built-in theme fixture failed: %s", qPrintable(result.error));
    }
    return result.theme;
}

ThemeSpec translucentTheme()
{
    static constexpr auto json = R"JSON({
        "schemaVersion": 1,
        "id": "translucent-contract",
        "name": "Translucent contract fixture",
        "variant": "custom",
        "cornerRadius": 9,
        "motionDuration": 140,
        "blurEnabled": true,
        "colors": {
            "canvas": "#80406080",
            "surface": "#80c08040",
            "surfaceRaised": "#80ffffff",
            "border": "#40404040",
            "text": "#c0ffffff",
            "textMuted": "#8080a0c0",
            "accent": "#809040d0",
            "accentText": "#c0000000",
            "danger": "#80ff2040"
        }
    })JSON";
    const auto result = ThemeLoader::fromJson(QByteArray(json), QStringLiteral("translucent fixture"));
    if (!result.ok) {
        qFatal("translucent theme fixture failed: %s", qPrintable(result.error));
    }
    return result.theme;
}

const DesignTokens &requireTokens(const DerivationResult &result)
{
    if (!result.ok()) {
        qFatal("token derivation failed: %s", qPrintable(result.diagnostic));
    }
    return *result.tokens;
}

std::array<QColor, 22> publishedColors(const DesignTokens &tokens)
{
    return {
        tokens.background().base,
        tokens.background().raised,
        tokens.background().highest,
        tokens.foreground().defaultColor,
        tokens.foreground().muted,
        tokens.foreground().disabled,
        tokens.accent().defaultColor,
        tokens.accent().foreground,
        tokens.accent().subtle,
        tokens.state().hover,
        tokens.state().pressed,
        tokens.focusRing(),
        tokens.divider(),
        tokens.strongOutline(),
        tokens.status().success.background,
        tokens.status().success.foreground,
        tokens.status().warning.background,
        tokens.status().warning.foreground,
        tokens.status().info.background,
        tokens.status().info.foreground,
        tokens.danger().defaultColor,
        tokens.danger().foreground,
    };
}

} // namespace

class DerivationTests final : public QObject {
    Q_OBJECT

private slots:
    void mapsEveryQstRoleFromSchemaV1();
    void normalizesCallerInputsDeterministically();
    void appliesAccessibilityTransforms();
    void flattensEveryTranslucentSemanticRoleDeterministically();
    void flattensTheCompleteSchemaAlphaRange();
    void coversSchemaMetricBoundaries();
    void rejectsValuesOutsideThePublicThemeContract();
};

void DerivationTests::mapsEveryQstRoleFromSchemaV1()
{
    const ThemeSpec theme = builtIn();
    const auto result = DesignTokenDeriver::derive(
        theme, {.basePointSize = 12.0, .textScale = 1.25});
    const DesignTokens &tokens = requireTokens(result);

    QCOMPARE(tokens.sourceThemeId(), QStringLiteral("qinda-dark"));
    QCOMPARE(tokens.background().base, theme.colors.value(QStringLiteral("canvas")));
    QCOMPARE(tokens.background().raised, theme.colors.value(QStringLiteral("surface")));
    QCOMPARE(tokens.background().highest,
             theme.colors.value(QStringLiteral("surfaceRaised")));
    QCOMPARE(tokens.foreground().defaultColor, theme.colors.value(QStringLiteral("text")));
    QCOMPARE(tokens.foreground().muted, theme.colors.value(QStringLiteral("textMuted")));
    QCOMPARE(tokens.accent().defaultColor, theme.colors.value(QStringLiteral("accent")));
    QCOMPARE(tokens.accent().foreground, theme.colors.value(QStringLiteral("accentText")));
    QCOMPARE(tokens.divider(), theme.colors.value(QStringLiteral("border")));
    QCOMPARE(tokens.danger().defaultColor, theme.colors.value(QStringLiteral("danger")));
    QCOMPARE(tokens.radius().small, 5.0);
    QCOMPARE(tokens.radius().medium, 10.0);
    QCOMPARE(tokens.radius().large, 15.0);
    QCOMPARE(tokens.spacing().one, 2.0);
    QCOMPARE(tokens.spacing().six, 24.0);
    QCOMPARE(tokens.typeScale().body, 15.0);
    QCOMPARE(tokens.typeScale().caption, 12.75);
    QCOMPARE(tokens.typeScale().display, 30.0);
    QCOMPARE(tokens.motion().instant, 0);
    QCOMPARE(tokens.motion().shortDuration, 96);
    QCOMPARE(tokens.motion().base, 160);
    QCOMPARE(tokens.motion().longDuration, 280);

    const QVariantMap map = tokens.toVariantMap();
    QCOMPARE(map.value(QStringLiteral("qstRevision")).toInt(), 1);
    QCOMPARE(map.value(QStringLiteral("sourceThemeId")).toString(), theme.id);
    const QStringList requiredGroups = {QStringLiteral("bg"),
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
                                        QStringLiteral("elevation")};
    for (const auto &group : requiredGroups) {
        QVERIFY2(map.value(group).toMap().size() > 0, qPrintable(group));
    }
}

void DerivationTests::normalizesCallerInputsDeterministically()
{
    const ThemeSpec theme = builtIn();
    const std::array<AccessibilityInputs, 6> inputs = {
        AccessibilityInputs{},
        AccessibilityInputs{.basePointSize = -10.0, .textScale = -1.0},
        AccessibilityInputs{.basePointSize = 500.0, .textScale = 10.0},
        AccessibilityInputs{.basePointSize = std::numeric_limits<double>::quiet_NaN(),
                            .textScale = std::numeric_limits<double>::infinity()},
        AccessibilityInputs{.basePointSize = 6.0, .textScale = 0.5},
        AccessibilityInputs{.basePointSize = 72.0, .textScale = 3.0},
    };

    for (const AccessibilityInputs &input : inputs) {
        const auto first = DesignTokenDeriver::derive(theme, input);
        const auto second = DesignTokenDeriver::derive(theme, input);
        QVERIFY(first.ok());
        QVERIFY(second.ok());
        QCOMPARE(*first.tokens, *second.tokens);
        QVERIFY(std::isfinite(first.tokens->typeScale().body));
        QVERIFY(first.tokens->inputs().basePointSize >= AccessibilityInputs::minimumBasePointSize);
        QVERIFY(first.tokens->inputs().basePointSize <= AccessibilityInputs::maximumBasePointSize);
        QVERIFY(first.tokens->inputs().textScale >= AccessibilityInputs::minimumTextScale);
        QVERIFY(first.tokens->inputs().textScale <= AccessibilityInputs::maximumTextScale);
    }
}

void DerivationTests::appliesAccessibilityTransforms()
{
    const ThemeSpec theme = builtIn(QStringLiteral("qinda-dusk.json"));
    const auto normalResult = DesignTokenDeriver::derive(theme);
    const DesignTokens &normal = requireTokens(normalResult);
    const auto transformedResult = DesignTokenDeriver::derive(
        theme,
        {.basePointSize = 10.0,
         .textScale = 1.5,
         .reducedMotion = true,
         .reducedTransparency = true,
         .highContrast = true});
    const DesignTokens &transformed = requireTokens(transformedResult);

    QCOMPARE(transformed.typeScale().body, normal.typeScale().body * 1.5);
    QCOMPARE(transformed.motion().instant, 0);
    QVERIFY(transformed.motion().shortDuration <= 80);
    QVERIFY(transformed.motion().base <= 80);
    QVERIFY(transformed.motion().longDuration <= 80);
    QCOMPARE(transformed.foreground().disabled.alpha(), 255);
    QCOMPARE(transformed.accent().subtle.alpha(), 255);
    QCOMPARE(transformed.state().hover.alpha(), 255);
    QCOMPARE(transformed.state().pressed.alpha(), 255);
    QVERIFY(normal.state().hover.alpha() < 255);
    QVERIFY(normal.state().pressed.alpha() < 255);
    QCOMPARE(transformed.focusRing(), theme.colors.value(QStringLiteral("text")));
    QCOMPARE(transformed.strongOutline(), theme.colors.value(QStringLiteral("text")));
    QVERIFY(!transformed.elevation().one.backgroundBlur);
    QCOMPARE(transformed.elevation().one.shadowOpacity, 0.0);
    QVERIFY(normal.elevation().one.backgroundBlur);
}

void DerivationTests::flattensEveryTranslucentSemanticRoleDeterministically()
{
    const ThemeSpec theme = translucentTheme();
    const auto normalResult = DesignTokenDeriver::derive(theme);
    const DesignTokens &normal = requireTokens(normalResult);
    QCOMPARE(normal.background().base.alpha(), 128);
    QCOMPARE(normal.background().raised.alpha(), 128);
    QCOMPARE(normal.background().highest.alpha(), 128);

    const auto firstResult = DesignTokenDeriver::derive(
        theme, {.reducedTransparency = true});
    const auto secondResult = DesignTokenDeriver::derive(
        theme, {.reducedTransparency = true});
    const DesignTokens &tokens = requireTokens(firstResult);
    QCOMPARE(tokens, requireTokens(secondResult));

    // AGENT-GUARD: Keep exact colors here, not only alpha checks. They pin the
    // normative canvas -> surface -> semantic-role flattening order against a
    // ThemeLoader-accepted schema-v1 theme with alpha in every source role.
    QCOMPARE(tokens.background().base.name(QColor::HexArgb), QStringLiteral("#ff203040"));
    QCOMPARE(tokens.background().raised.name(QColor::HexArgb), QStringLiteral("#ff705840"));
    QCOMPARE(tokens.background().highest.name(QColor::HexArgb), QStringLiteral("#ffb8aca0"));
    QCOMPARE(tokens.foreground().defaultColor.name(QColor::HexArgb),
             QStringLiteral("#ffdcd6d0"));
    QCOMPARE(tokens.foreground().muted.name(QColor::HexArgb), QStringLiteral("#ff787c80"));
    QCOMPARE(tokens.foreground().disabled.name(QColor::HexArgb),
             QStringLiteral("#ff746a60"));
    QCOMPARE(tokens.accent().defaultColor.name(QColor::HexArgb), QStringLiteral("#ff804c88"));
    QCOMPARE(tokens.accent().foreground.name(QColor::HexArgb), QStringLiteral("#ff201322"));
    QCOMPARE(tokens.accent().subtle.name(QColor::HexArgb), QStringLiteral("#ff725749"));
    QCOMPARE(tokens.state().hover.name(QColor::HexArgb), QStringLiteral("#ff79624c"));
    QCOMPARE(tokens.state().pressed.name(QColor::HexArgb), QStringLiteral("#ff826c57"));
    QCOMPARE(tokens.divider().name(QColor::HexArgb), QStringLiteral("#ff645240"));
    QCOMPARE(tokens.danger().defaultColor.name(QColor::HexArgb), QStringLiteral("#ffb83c40"));

    for (const QColor &color : publishedColors(tokens)) {
        QCOMPARE(color.alpha(), 255);
    }
}

void DerivationTests::flattensTheCompleteSchemaAlphaRange()
{
    ThemeSpec theme = translucentTheme();
    for (int alpha = 0; alpha <= 255; ++alpha) {
        for (auto color = theme.colors.begin(); color != theme.colors.end(); ++color) {
            color->setAlpha(alpha);
        }
        const auto first = DesignTokenDeriver::derive(
            theme, {.reducedTransparency = true});
        const auto second = DesignTokenDeriver::derive(
            theme, {.reducedTransparency = true});
        QVERIFY2(first.ok(), qPrintable(first.diagnostic));
        QVERIFY2(second.ok(), qPrintable(second.diagnostic));
        QCOMPARE(*first.tokens, *second.tokens);
        for (const QColor &color : publishedColors(*first.tokens)) {
            QCOMPARE(color.alpha(), 255);
        }
    }
}

void DerivationTests::coversSchemaMetricBoundaries()
{
    ThemeSpec theme = builtIn();
    for (int radius = 0; radius <= 32; ++radius) {
        theme.cornerRadius = radius;
        for (const int duration : {0, 1, 79, 80, 81, 999, 1000}) {
            theme.motionDuration = duration;
            const auto result = DesignTokenDeriver::derive(theme);
            QVERIFY2(result.ok(), qPrintable(result.diagnostic));
            QCOMPARE(result.tokens->radius().medium, static_cast<double>(radius));
            QVERIFY(result.tokens->radius().large <= 32.0);
            QVERIFY(result.tokens->motion().shortDuration >= 80);
            QVERIFY(result.tokens->motion().base >= 0);
            QVERIFY(result.tokens->motion().longDuration >= 0);
        }
    }
}

void DerivationTests::rejectsValuesOutsideThePublicThemeContract()
{
    ThemeSpec theme = builtIn();
    theme.schemaVersion = 2;
    QCOMPARE(DesignTokenDeriver::derive(theme).error, DerivationError::InvalidSchemaVersion);

    theme = builtIn();
    theme.id.clear();
    QCOMPARE(DesignTokenDeriver::derive(theme).error, DerivationError::MissingIdentity);

    theme = builtIn();
    theme.colors.remove(QStringLiteral("accent"));
    QCOMPARE(DesignTokenDeriver::derive(theme).error, DerivationError::InvalidColor);

    theme = builtIn();
    theme.motionDuration = 1001;
    QCOMPARE(DesignTokenDeriver::derive(theme).error, DerivationError::InvalidMetric);
}

QTEST_GUILESS_MAIN(DerivationTests)
#include "tst_derivation.moc"
