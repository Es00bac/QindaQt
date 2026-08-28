// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_bootstrap.h"

#include <QTest>

using namespace QindaQt::Services::FontPreferences;

class tst_FontBootstrap : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testCreateApplicationFont();
    void testCreateMonospaceFont();
    void testDeriveRenderingAttributes();
    void testApplyPreferences();
};

void tst_FontBootstrap::testCreateApplicationFont()
{
    FontPreferences prefs;
    prefs.setFamily(QStringLiteral("Cantarell"));
    prefs.setPointSize(11.5);
    prefs.setHinting(FontHinting::Slight);
    prefs.setAntialiasing(FontAntialiasing::Subpixel);

    const QFont font = FontBootstrap::createApplicationFont(prefs);
    QCOMPARE(font.family(), QStringLiteral("Cantarell"));
    QCOMPARE(font.pointSizeF(), 11.5);
    QCOMPARE(font.hintingPreference(), QFont::PreferVerticalHinting);
    QVERIFY(font.styleStrategy() & QFont::PreferAntialias);
}

void tst_FontBootstrap::testCreateMonospaceFont()
{
    FontPreferences prefs;
    prefs.setMonospaceFamily(QStringLiteral("Fira Code"));
    prefs.setPointSize(10.5);
    prefs.setHinting(FontHinting::Full);
    prefs.setAntialiasing(FontAntialiasing::None);

    const QFont font = FontBootstrap::createMonospaceFont(prefs);
    QCOMPARE(font.family(), QStringLiteral("Fira Code"));
    QCOMPARE(font.pointSizeF(), 10.5);
    QVERIFY(font.fixedPitch());
    QCOMPARE(font.hintingPreference(), QFont::PreferFullHinting);
    QCOMPARE(font.styleStrategy(), QFont::NoAntialias);
}

void tst_FontBootstrap::testDeriveRenderingAttributes()
{
    FontPreferences prefs;
    prefs.setAntialiasing(FontAntialiasing::Grayscale);
    prefs.setHinting(FontHinting::Medium);
    prefs.setLogicalDpi(192.0);

    const auto attrs = FontBootstrap::deriveRenderingAttributes(prefs);
    QVERIFY(attrs.antialiasingEnabled);
    QCOMPARE(attrs.hintingPreference, QFont::PreferFullHinting);
    QCOMPARE(attrs.logicalDpi, 192.0);
}

void tst_FontBootstrap::testApplyPreferences()
{
    QFont baseFont(QStringLiteral("Helvetica"), 9);
    FontPreferences prefs;
    prefs.setFamily(QStringLiteral("Inter"));
    prefs.setPointSize(12.0);

    const QFont updated = FontBootstrap::applyPreferences(baseFont, prefs);
    QCOMPARE(updated.family(), QStringLiteral("Inter"));
    QCOMPARE(updated.pointSizeF(), 12.0);
}

QTEST_MAIN(tst_FontBootstrap)
#include "tst_font_bootstrap.moc"
