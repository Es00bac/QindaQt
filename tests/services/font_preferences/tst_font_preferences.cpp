// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences.h"

#include <QTest>

using namespace QindaQt::Services::FontPreferences;

class tst_FontPreferences : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testSystemDefaults();
    void testCustomProperties();
    void testPointSizeClamping();
    void testLogicalDpiClamping();
    void testFamilyNormalization();
    void testValidity();
    void testEquality();
};

void tst_FontPreferences::testSystemDefaults()
{
    const auto defaults = FontPreferences::systemDefaults();
    QCOMPARE(defaults.family(), QStringLiteral("Noto Sans"));
    QCOMPARE(defaults.monospaceFamily(), QStringLiteral("Noto Sans Mono"));
    QCOMPARE(defaults.pointSize(), 10.0);
    QCOMPARE(defaults.antialiasing(), FontAntialiasing::Subpixel);
    QCOMPARE(defaults.hinting(), FontHinting::Slight);
    QCOMPARE(defaults.subpixelOrder(), FontSubpixelOrder::Rgb);
    QVERIFY(!defaults.logicalDpi().has_value());
    QVERIFY(defaults.isValid());
}

void tst_FontPreferences::testCustomProperties()
{
    FontPreferences prefs;
    prefs.setFamily(QStringLiteral("Cantarell"));
    prefs.setMonospaceFamily(QStringLiteral("Source Code Pro"));
    prefs.setPointSize(12.5);
    prefs.setAntialiasing(FontAntialiasing::Grayscale);
    prefs.setHinting(FontHinting::Full);
    prefs.setSubpixelOrder(FontSubpixelOrder::Bgr);
    prefs.setLogicalDpi(120.0);

    QCOMPARE(prefs.family(), QStringLiteral("Cantarell"));
    QCOMPARE(prefs.monospaceFamily(), QStringLiteral("Source Code Pro"));
    QCOMPARE(prefs.pointSize(), 12.5);
    QCOMPARE(prefs.antialiasing(), FontAntialiasing::Grayscale);
    QCOMPARE(prefs.hinting(), FontHinting::Full);
    QCOMPARE(prefs.subpixelOrder(), FontSubpixelOrder::Bgr);
    QVERIFY(prefs.logicalDpi().has_value());
    QCOMPARE(*prefs.logicalDpi(), 120.0);
    QVERIFY(prefs.isValid());
}

void tst_FontPreferences::testPointSizeClamping()
{
    FontPreferences prefs;
    prefs.setPointSize(1.0);
    QCOMPARE(prefs.pointSize(), MinPointSize);

    prefs.setPointSize(500.0);
    QCOMPARE(prefs.pointSize(), MaxPointSize);

    prefs.setPointSize(11.0);
    QCOMPARE(prefs.pointSize(), 11.0);
}

void tst_FontPreferences::testLogicalDpiClamping()
{
    FontPreferences prefs;
    prefs.setLogicalDpi(10.0);
    QVERIFY(prefs.logicalDpi().has_value());
    QCOMPARE(*prefs.logicalDpi(), MinLogicalDpi);

    prefs.setLogicalDpi(1000.0);
    QVERIFY(prefs.logicalDpi().has_value());
    QCOMPARE(*prefs.logicalDpi(), MaxLogicalDpi);

    prefs.setLogicalDpi(std::nullopt);
    QVERIFY(!prefs.logicalDpi().has_value());
}

void tst_FontPreferences::testFamilyNormalization()
{
    FontPreferences prefs;
    prefs.setFamily(QStringLiteral("  DejaVu   Sans  "));
    QCOMPARE(prefs.family(), QStringLiteral("DejaVu Sans"));

    prefs.setMonospaceFamily(QStringLiteral("  Fira   Mono  "));
    QCOMPARE(prefs.monospaceFamily(), QStringLiteral("Fira Mono"));
}

void tst_FontPreferences::testValidity()
{
    FontPreferences validPrefs;
    QVERIFY(validPrefs.isValid());

    FontPreferences invalidFamily;
    invalidFamily.setFamily(QStringLiteral(""));
    QVERIFY(!invalidFamily.isValid());

    FontPreferences normalized = invalidFamily.normalized();
    QVERIFY(normalized.isValid());
    QCOMPARE(normalized.family(), QStringLiteral("Noto Sans"));
}

void tst_FontPreferences::testEquality()
{
    FontPreferences a;
    FontPreferences b;
    QCOMPARE(a, b);

    b.setPointSize(14.0);
    QVERIFY(!(a == b));

    a.setPointSize(14.0);
    QCOMPARE(a, b);

    a.setLogicalDpi(96.0);
    QVERIFY(!(a == b));

    b.setLogicalDpi(96.0);
    QCOMPARE(a, b);
}

QTEST_MAIN(tst_FontPreferences)
#include "tst_font_preferences.moc"
