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
                                           firstPreferences));
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
        QStringLiteral("qinda-dark"),
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
