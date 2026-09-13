// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_test_support.h"

#include "qindaqt/decoration_painter/decoration_painter.h"

#include <QtTest>

using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;

class CustomizeWindowPreviewTests final : public QObject {
    Q_OBJECT

private slots:
    void fallsBackToTheThemeDefaultBeforeConfirmation();
    void resolvesAConfirmedNonFallbackThemeNotJustTheFallback();
    void republishesWhenEitherRequiredThemeKeyChangesIndependently();
    void resolvesTwoDifferentConfirmedCombosDifferently();
    void tracksAuthoritativeChromePreferenceChanges();
};

// Before Settings1 confirms a chrome-preference snapshot, the preview must
// already show the resolved theme's own default chrome -- the same
// construction-time fallback AppAppearance::ApplicationAppearanceController
// itself uses -- never an invented placeholder.
void CustomizeWindowPreviewTests::fallsBackToTheThemeDefaultBeforeConfirmation()
{
    ModelHarness harness;
    const QVariantMap expected =
        QindaQt::Decoration::resolveWindowChrome(
            harness.appearance.theme(), QindaQt::Decoration::ChromePreferences{})
            .toVariantMap();
    QCOMPARE(harness.windowPreview.chrome(), expected);
}

// The theme client must be scoped to both required Settings1 keys
// (appearance.theme and appearance.colorScheme), matching
// ApplicationAppearanceController's own resolution contract: requesting a
// genuinely non-fallback, non-circular theme must actually be adopted, not
// silently stay on the "qinda-dark" construction-time fallback. Asserting
// the adopted theme id directly -- independent of the chrome comparison --
// proves adoption happened rather than merely that resolveWindowChrome is
// self-consistent with whatever theme never changed.
void CustomizeWindowPreviewTests::resolvesAConfirmedNonFallbackThemeNotJustTheFallback()
{
    ModelHarness harness;
    QVERIFY(harness.establishWindowPreview(QStringLiteral("qinda-light"),
                                           QStringLiteral("light"), {}));
    QCOMPARE(harness.appearance.theme().id, QStringLiteral("qinda-light"));
    const QVariantMap expected =
        QindaQt::Decoration::resolveWindowChrome(
            harness.appearance.theme(), QindaQt::Decoration::ChromePreferences{})
            .toVariantMap();
    QCOMPARE(harness.windowPreview.chrome(), expected);
}

// Proves the preview republishes when EITHER required theme key changes on
// its own, not just when both happen to change together: a theme-id change
// with the color scheme held fixed, then a color-scheme change with the
// requested theme id text held fixed (moving qinda-bliss, a light-only
// theme, out of its compatible scheme forces a different resolved theme).
void CustomizeWindowPreviewTests::republishesWhenEitherRequiredThemeKeyChangesIndependently()
{
    ModelHarness harness;
    QVERIFY(harness.establishWindowPreview(QStringLiteral("qinda-light"),
                                           QStringLiteral("light"), {}));
    QCOMPARE(harness.appearance.theme().id, QStringLiteral("qinda-light"));

    // Theme id changes, color scheme does not.
    QVERIFY(harness.updateTheme(QStringLiteral("qinda-bliss"), QStringLiteral("light"), 8));
    QTRY_COMPARE(harness.appearance.theme().id, QStringLiteral("qinda-bliss"));

    // Color scheme changes, the requested theme id text does not: qinda-bliss
    // is light-only, so demanding "dark" makes it incompatible and the
    // resolver falls through to the dark built-in -- a different theme is
    // adopted purely because the color-scheme key was received and changed.
    QVERIFY(harness.updateTheme(QStringLiteral("qinda-bliss"), QStringLiteral("dark"), 9));
    QTRY_COMPARE(harness.appearance.theme().id, QStringLiteral("qinda-dark"));
}

// The two-resolver-outputs acceptance criterion: a confirmed theme with
// distinct button-style/button-side preferences produces genuinely different,
// correctly-resolved chrome -- proving the preview reuses
// Decoration::resolveWindowChrome end to end (Settings1 wire ->
// ChromePreferences -> DecorationChrome), rather than any hard-coded shape.
void CustomizeWindowPreviewTests::resolvesTwoDifferentConfirmedCombosDifferently()
{
    ModelHarness harness;
    const QVariantMap firstPreferences{
        {QStringLiteral("appearance.windowButtonStyle"), QStringLiteral("flat")},
        {QStringLiteral("appearance.windowButtonSide"), QStringLiteral("left")},
    };
    QVERIFY(harness.establishWindowPreview(QStringLiteral("qinda-dark"),
                                           QStringLiteral("dark"), firstPreferences));
    const QVariantMap firstChrome = harness.windowPreview.chrome();
    const QVariantMap firstExpected =
        QindaQt::Decoration::resolveWindowChrome(
            harness.appearance.theme(),
            QindaQt::Decoration::ChromePreferences::fromSettingsValues(
                firstPreferences))
            .toVariantMap();
    QCOMPARE(firstChrome, firstExpected);
    QCOMPARE(firstChrome.value(QStringLiteral("buttonSide")).toString(),
             QStringLiteral("left"));
    QCOMPARE(firstChrome.value(QStringLiteral("buttonStyle")).toString(),
             QStringLiteral("flat"));

    const QVariantMap secondPreferences{
        {QStringLiteral("appearance.windowButtonStyle"), QStringLiteral("symbols")},
        {QStringLiteral("appearance.windowButtonSide"), QStringLiteral("right")},
    };
    QVERIFY(harness.updateChromePreferences(secondPreferences, 8));
    QTRY_VERIFY(harness.windowPreview.chrome() != firstChrome);
    const QVariantMap secondChrome = harness.windowPreview.chrome();
    const QVariantMap secondExpected =
        QindaQt::Decoration::resolveWindowChrome(
            harness.appearance.theme(),
            QindaQt::Decoration::ChromePreferences::fromSettingsValues(
                secondPreferences))
            .toVariantMap();
    QCOMPARE(secondChrome, secondExpected);
    QCOMPARE(secondChrome.value(QStringLiteral("buttonSide")).toString(),
             QStringLiteral("right"));
    QCOMPARE(secondChrome.value(QStringLiteral("buttonStyle")).toString(),
             QStringLiteral("symbols"));
}

// A changed Settings1 revision republishes the preview truth exactly like
// CustomizeWindowPreview::recompute() promises, mirroring the wallpaper
// preview's own authoritative-snapshot-tracking contract.
void CustomizeWindowPreviewTests::tracksAuthoritativeChromePreferenceChanges()
{
    ModelHarness harness;
    QVERIFY(harness.establishWindowPreview(
        QStringLiteral("qinda-dark"), QStringLiteral("dark"),
        {{QStringLiteral("appearance.windowButtons"), QStringLiteral("all")}}));
    QSignalSpy changed(&harness.windowPreview, &CustomizeWindowPreview::changed);

    QVERIFY(harness.updateChromePreferences(
        {{QStringLiteral("appearance.windowButtons"),
          QStringLiteral("minimize-close")}},
        9));
    QTRY_COMPARE(harness.windowPreview.chrome().value(QStringLiteral("buttons")).toString(),
                 QStringLiteral("minimize-close"));
    QVERIFY(changed.count() > 0);
}

QTEST_GUILESS_MAIN(CustomizeWindowPreviewTests)
#include "tst_customize_window_preview.moc"
