// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

using namespace QindaQt::DesignTokens;
using namespace QindaQt::Themes;

namespace {

void requireContrast(const QString &themeId,
                     const QString &pairName,
                     const QColor &foreground,
                     const QColor &background,
                     double minimum)
{
    const double ratio = DesignTokenDeriver::contrastRatio(foreground, background);
    QVERIFY2(ratio >= minimum,
             qPrintable(QStringLiteral("%1: %2 ratio %3 is below %4")
                            .arg(themeId, pairName)
                            .arg(ratio, 0, 'f', 3)
                            .arg(minimum, 0, 'f', 1)));
}

} // namespace

class BuiltInContrastTests final : public QObject {
    Q_OBJECT

private slots:
    void everyBuiltInMeetsDocumentedPairs();
    void everyBuiltInTranslucentSurfaceKeepsTextContrast();
};

void BuiltInContrastTests::everyBuiltInMeetsDocumentedPairs()
{
    const auto loaded = ThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(loaded.size(), 12);

    for (const auto &result : loaded) {
        QVERIFY2(result.ok, qPrintable(result.error));
        const auto derived = DesignTokenDeriver::derive(result.theme);
        QVERIFY2(derived.ok(), qPrintable(derived.diagnostic));
        const DesignTokens &tokens = *derived.tokens;
        const QString &id = result.theme.id;

        requireContrast(id, QStringLiteral("fg.default/bg.base"),
                        tokens.foreground().defaultColor, tokens.background().base, 4.5);
        requireContrast(id, QStringLiteral("fg.default/bg.raised"),
                        tokens.foreground().defaultColor, tokens.background().raised, 4.5);
        requireContrast(id, QStringLiteral("fg.default/bg.highest"),
                        tokens.foreground().defaultColor, tokens.background().highest, 4.5);
        requireContrast(id, QStringLiteral("fg.muted/bg.raised"),
                        tokens.foreground().muted, tokens.background().raised, 4.5);
        requireContrast(id, QStringLiteral("fg.muted/bg.base-ui"),
                        tokens.foreground().muted, tokens.background().base, 4.5);
        requireContrast(id, QStringLiteral("fg.muted/bg.highest-ui"),
                        tokens.foreground().muted, tokens.background().highest, 4.5);
        requireContrast(id, QStringLiteral("accent.fg/accent.default"),
                        tokens.accent().foreground, tokens.accent().defaultColor, 4.5);
        requireContrast(id, QStringLiteral("danger.fg/danger.default"),
                        tokens.danger().foreground, tokens.danger().defaultColor, 4.5);
        requireContrast(id, QStringLiteral("focus.ring/bg.raised"),
                        tokens.focusRing(), tokens.background().raised, 3.0);
        requireContrast(id, QStringLiteral("outline.strong/bg.raised"),
                        tokens.strongOutline(), tokens.background().raised, 3.0);
        requireContrast(id, QStringLiteral("status.success"),
                        tokens.status().success.foreground,
                        tokens.status().success.background,
                        4.5);
        requireContrast(id, QStringLiteral("status.warning"),
                        tokens.status().warning.foreground,
                        tokens.status().warning.background,
                        4.5);
        requireContrast(id, QStringLiteral("status.info"),
                        tokens.status().info.foreground,
                        tokens.status().info.background,
                        4.5);

        if (id == QStringLiteral("qinda-high-contrast")) {
            requireContrast(id, QStringLiteral("high-contrast-body"),
                            tokens.foreground().defaultColor, tokens.background().raised, 7.0);
        }
    }
}

void BuiltInContrastTests::everyBuiltInTranslucentSurfaceKeepsTextContrast()
{
    // ADR-0206 guardrail, checked on the shipped catalog: every published
    // surface material keeps fg.default and fg.muted at 4.5:1 over the
    // surface composited on black and on white at its published opacity.
    const auto loaded = ThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(loaded.size(), 12);
    int translucentSurfaces = 0;
    for (const auto &result : loaded) {
        QVERIFY2(result.ok, qPrintable(result.error));
        const auto derived = DesignTokenDeriver::derive(result.theme);
        QVERIFY2(derived.ok(), qPrintable(derived.diagnostic));
        const DesignTokens &tokens = *derived.tokens;
        const auto &material = tokens.material();
        const struct {
            const char *name;
            const SurfaceMaterialTokens *surface;
            QColor paint;
        } surfaces[] = {
            {"panel", &material.panel, tokens.background().raised},
            {"popup", &material.popup, tokens.background().raised},
            {"menu", &material.menu, tokens.background().raised},
            {"containerChrome", &material.containerChrome, tokens.background().raised},
            {"decoration", &material.decoration, tokens.background().highest},
            {"desktopIcons", &material.desktopIcons, tokens.background().base},
        };
        for (const auto &entry : surfaces) {
            if (entry.surface->opacity < 1.0) {
                ++translucentSurfaces;
            }
            for (const QColor &backdrop : {QColor(Qt::black), QColor(Qt::white)}) {
                const QColor composite = DesignTokenDeriver::compositeOver(
                    entry.paint, entry.surface->opacity, backdrop);
                const QString label = QString::fromLatin1(entry.name)
                    + (backdrop == QColor(Qt::black) ? QStringLiteral(" over black")
                                                     : QStringLiteral(" over white"));
                requireContrast(result.theme.id, QStringLiteral("fg.default/") + label,
                                tokens.foreground().defaultColor, composite, 4.5);
                requireContrast(result.theme.id, QStringLiteral("fg.muted/") + label,
                                tokens.foreground().muted, composite, 4.5);
            }
        }
    }
    // The v2 catalog ships translucent surfaces; the guardrail must have had
    // something to guard.
    QVERIFY(translucentSurfaces > 0);
}

QTEST_GUILESS_MAIN(BuiltInContrastTests)
#include "tst_builtin_contrast.moc"
