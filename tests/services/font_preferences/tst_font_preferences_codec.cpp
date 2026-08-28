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
    QJsonObject badJson;
    badJson.insert(QStringLiteral("pointSize"), QStringLiteral("not-a-number"));

    QString error;
    auto decoded = FontPreferencesCodec::fromJsonObject(badJson, &error);
    QVERIFY(!decoded.has_value());
    QVERIFY(error.contains(QStringLiteral("pointSize")));

    badJson.insert(QStringLiteral("pointSize"), 9999.0);
    decoded = FontPreferencesCodec::fromJsonObject(badJson, &error);
    QVERIFY(!decoded.has_value());
    QVERIFY(error.contains(QStringLiteral("bounds")));
}

void tst_FontPreferencesCodec::testMalformedSettingsMapRejection()
{
    QVariantMap badMap;
    badMap.insert(QStringLiteral("fonts.hinting"), QStringLiteral("invalid-hinting-style"));

    QString error;
    auto decoded = FontPreferencesCodec::fromSettingsMap(badMap, &error);
    QVERIFY(!decoded.has_value());
    QVERIFY(error.contains(QStringLiteral("hinting")));
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
