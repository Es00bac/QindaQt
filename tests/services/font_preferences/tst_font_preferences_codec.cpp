// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/font_preferences/font_preferences_codec.h"

#include <QTest>

using namespace QindaQt::Services::FontPreferences;

class tst_FontPreferencesCodec : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testJsonRoundTrip();
    void testVariantMapRoundTrip();
    void testSettingsMapRoundTrip();
    void testMalformedJsonRejection();
    void testMalformedSettingsMapRejection();
    void testAntialiasingTypeFlexibility();
};

void tst_FontPreferencesCodec::testJsonRoundTrip()
{
    FontPreferences original;
    original.setFamily(QStringLiteral("Ubuntu"));
    original.setMonospaceFamily(QStringLiteral("Ubuntu Mono"));
    original.setPointSize(11.25);
    original.setAntialiasing(FontAntialiasing::Grayscale);
    original.setHinting(FontHinting::Medium);
    original.setSubpixelOrder(FontSubpixelOrder::Vrgb);
    original.setLogicalDpi(144.0);

    const QJsonObject json = FontPreferencesCodec::toJsonObject(original);
    QString error;
    const auto decoded = FontPreferencesCodec::fromJsonObject(json, &error);

    QVERIFY(decoded.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(*decoded, original);
}

void tst_FontPreferencesCodec::testVariantMapRoundTrip()
{
    FontPreferences original;
    original.setFamily(QStringLiteral("Liberation Sans"));
    original.setMonospaceFamily(QStringLiteral("Liberation Mono"));
    original.setPointSize(9.5);
    original.setAntialiasing(FontAntialiasing::None);
    original.setHinting(FontHinting::None);
    original.setSubpixelOrder(FontSubpixelOrder::None);
    original.setLogicalDpi(std::nullopt);

    const QVariantMap map = FontPreferencesCodec::toVariantMap(original);
    QString error;
    const auto decoded = FontPreferencesCodec::fromVariantMap(map, &error);

    QVERIFY(decoded.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(*decoded, original);
}

void tst_FontPreferencesCodec::testSettingsMapRoundTrip()
{
    FontPreferences original;
    original.setFamily(QStringLiteral("Noto Sans"));
    original.setMonospaceFamily(QStringLiteral("Noto Sans Mono"));
    original.setPointSize(10.0);
    original.setAntialiasing(FontAntialiasing::Subpixel);
    original.setHinting(FontHinting::Slight);
    original.setSubpixelOrder(FontSubpixelOrder::Rgb);

    const QVariantMap settings = FontPreferencesCodec::toSettingsMap(original);
    QCOMPARE(settings.value(QStringLiteral("fonts.family")).toString(), QStringLiteral("Noto Sans"));
    QCOMPARE(settings.value(QStringLiteral("fonts.monospaceFamily")).toString(), QStringLiteral("Noto Sans Mono"));
    QCOMPARE(settings.value(QStringLiteral("fonts.pointSize")).toDouble(), 10.0);
    QCOMPARE(settings.value(QStringLiteral("fonts.antialiasing")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("fonts.hinting")).toString(), QStringLiteral("slight"));
    QCOMPARE(settings.value(QStringLiteral("fonts.subpixelOrder")).toString(), QStringLiteral("rgb"));

    QString error;
    const auto decoded = FontPreferencesCodec::fromSettingsMap(settings, &error);
    QVERIFY(decoded.has_value());
    QVERIFY(error.isEmpty());
    QCOMPARE(*decoded, original);
}

void tst_FontPreferencesCodec::testMalformedJsonRejection()
{
    QString error;

    // String instead of double
    QJsonObject badJson;
    badJson.insert(QStringLiteral("pointSize"), QStringLiteral("not-a-number"));
    auto decoded = FontPreferencesCodec::fromJsonObject(badJson, &error);
    QVERIFY(!decoded.has_value());
    QVERIFY(error.contains(QStringLiteral("pointSize")));

    // Point size outside [6.0, 36.0]
    const QList<double> badPointSizes = {
        0.0, 4.0, 5.9, 36.1, 50.0, 100.0, 144.0, 9999.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double badSize : badPointSizes) {
        badJson.insert(QStringLiteral("pointSize"), badSize);
        decoded = FontPreferencesCodec::fromJsonObject(badJson, &error);
        QVERIFY(!decoded.has_value());
        QVERIFY(error.contains(QStringLiteral("bounds")) || error.contains(QStringLiteral("pointSize")));
    }

    // Logical DPI outside [48.0, 576.0] or non-finite
    const QList<double> badDpiValues = {
        10.0, 47.9, 576.1, 1000.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double badDpi : badDpiValues) {
        QJsonObject badDpiJson;
        badDpiJson.insert(QStringLiteral("logicalDpi"), badDpi);
        decoded = FontPreferencesCodec::fromJsonObject(badDpiJson, &error);
        QVERIFY(!decoded.has_value());
        QVERIFY(error.contains(QStringLiteral("logicalDpi")));
    }
}

void tst_FontPreferencesCodec::testMalformedSettingsMapRejection()
{
    QString error;

    QVariantMap badMap;
    badMap.insert(QStringLiteral("fonts.hinting"), QStringLiteral("invalid-hinting-style"));
    auto decoded = FontPreferencesCodec::fromSettingsMap(badMap, &error);
    QVERIFY(!decoded.has_value());
    QVERIFY(error.contains(QStringLiteral("hinting")));

    // Settings1 schema-key rejection for point sizes outside [6.0, 36.0]
    const QList<double> badPointSizes = {
        0.0, 4.0, 5.9, 36.1, 50.0, 100.0, 144.0, 9999.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double badSize : badPointSizes) {
        QVariantMap badSettings;
        badSettings.insert(QStringLiteral("fonts.pointSize"), badSize);
        decoded = FontPreferencesCodec::fromSettingsMap(badSettings, &error);
        QVERIFY(!decoded.has_value());
        QVERIFY(error.contains(QStringLiteral("fonts.pointSize")));
    }
}

void tst_FontPreferencesCodec::testAntialiasingTypeFlexibility()
{
    QVariantMap boolMap;
    boolMap.insert(QStringLiteral("fonts.antialiasing"), false);
    auto decoded = FontPreferencesCodec::fromSettingsMap(boolMap);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->antialiasing(), FontAntialiasing::None);

    QVariantMap strMap;
    strMap.insert(QStringLiteral("fonts.antialiasing"), QStringLiteral("grayscale"));
    decoded = FontPreferencesCodec::fromSettingsMap(strMap);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->antialiasing(), FontAntialiasing::Grayscale);
}

QTEST_MAIN(tst_FontPreferencesCodec)
#include "tst_font_preferences_codec.moc"
