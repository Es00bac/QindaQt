// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/settings/layered_settings.h"
#include "qindaqt/settings/settings_document.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>
#include <bit>
#include <cmath>
#include <limits>

using namespace QindaQt::Settings;

class SettingsPersistenceTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsBuiltInProfileDefaults();
    void roundTripsAUserDocument();
    void roundTripsCanonicalObjectValuesWithoutTypeDrift();
    void rejectsNonRoundTrippableJsonText();
    void rejectsVolatileAndInvalidDocuments();
    void failedSavePreservesExistingFile();

private:
    std::optional<SettingsSchema> m_schema;
};

void SettingsPersistenceTests::initTestCase()
{
    QString error;
    m_schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(m_schema.has_value(), qPrintable(error));
}

void SettingsPersistenceTests::loadsBuiltInProfileDefaults()
{
    const auto loaded = SettingsFileStore::load(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"), *m_schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY(loaded.document.layer == SettingLayer::ProfileDefaults);

    LayeredSettings settings(*m_schema);
    const auto applied = settings.replaceLayer(loaded.document.layer, loaded.document.values);
    QVERIFY2(applied.ok(), qPrintable(applied.message));
    QCOMPARE(settings.value(QStringLiteral("appearance.animationDurationMs")).toLongLong(), 160);
}

void SettingsPersistenceTests::roundTripsAUserDocument()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument document{.schemaVersion = 2,
                                    .layer = SettingLayer::UserOverrides,
                                    .values = {{QStringLiteral("appearance.theme"),
                                                QStringLiteral("qinda-light")},
                                               {QStringLiteral("fonts.pointSize"), 11.5}}};

    QString error;
    QVERIFY2(SettingsFileStore::save(path, document, *m_schema, nullptr, &error), qPrintable(error));
    const auto loaded = SettingsFileStore::load(path, *m_schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    QVERIFY(loaded.document.layer == SettingLayer::UserOverrides);
    QCOMPARE(loaded.document.values.value(QStringLiteral("appearance.theme")).toString(),
             QStringLiteral("qinda-light"));
    QCOMPARE(loaded.document.values.value(QStringLiteral("fonts.pointSize")).toDouble(), 11.5);
}

void SettingsPersistenceTests::roundTripsCanonicalObjectValuesWithoutTypeDrift()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const QVariantMap object{
        {QStringLiteral("null"), QVariant::fromValue(nullptr)},
        {QStringLiteral("minimum"), QVariant::fromValue(std::numeric_limits<qint64>::min())},
        {QStringLiteral("maximum"), QVariant::fromValue(std::numeric_limits<qint64>::max())},
        {QStringLiteral("uint32"), QVariant::fromValue(std::numeric_limits<quint32>::max())},
        {QStringLiteral("integralDouble"), 8.0},
        {QStringLiteral("fraction"), 1.25},
        {QStringLiteral("negativeZero"), -0.0},
        {QStringLiteral("denormal"), std::numeric_limits<double>::denorm_min()},
        {QStringLiteral("maximumDouble"), std::numeric_limits<double>::max()},
        {QStringLiteral("decimal"), 0.1},
        {QStringLiteral("positiveInsideIntBoundary"),
         std::nextafter(std::ldexp(1.0, 63), 0.0)},
        {QStringLiteral("positiveIntBoundary"), std::ldexp(1.0, 63)},
        {QStringLiteral("positiveOutsideIntBoundary"),
         std::nextafter(std::ldexp(1.0, 63),
                        std::numeric_limits<double>::infinity())},
        {QStringLiteral("negativeIntBoundary"), -std::ldexp(1.0, 63)},
        {QStringLiteral("negativeOutsideIntBoundary"),
         std::nextafter(-std::ldexp(1.0, 63),
                        -std::numeric_limits<double>::infinity())},
        {QStringLiteral("array"),
         QVariantList{QVariant::fromValue(nullptr),
                      QVariantMap{{QStringLiteral("nested"), quint16(65'535)}}}}};
    const SettingsDocument document{
        .schemaVersion = 2,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("displays.configuration"), object}}};

    QString error;
    QVERIFY2(SettingsFileStore::save(path, document, *m_schema, nullptr, &error), qPrintable(error));
    QFile encoded(path);
    QVERIFY(encoded.open(QIODevice::ReadOnly));
    const QByteArray json = encoded.readAll();
    QVERIFY(json.contains("\"null\": null"));
    QVERIFY(json.contains("-9223372036854775808"));
    QVERIFY(json.contains("9223372036854775807"));

    const auto loaded = SettingsFileStore::load(path, *m_schema);
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    const QVariantMap result = loaded.document.values
                                   .value(QStringLiteral("displays.configuration")).toMap();
    QCOMPARE(result.value(QStringLiteral("null")).metaType().id(), QMetaType::Nullptr);
    QCOMPARE(result.value(QStringLiteral("minimum")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("minimum")).toLongLong(),
             std::numeric_limits<qint64>::min());
    QCOMPARE(result.value(QStringLiteral("maximum")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("maximum")).toLongLong(),
             std::numeric_limits<qint64>::max());
    QCOMPARE(result.value(QStringLiteral("uint32")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("uint32")).toLongLong(),
             qint64(std::numeric_limits<quint32>::max()));
    QCOMPARE(result.value(QStringLiteral("integralDouble")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("fraction")).metaType().id(), QMetaType::Double);
    QCOMPARE(result.value(QStringLiteral("negativeZero")).metaType().id(), QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("negativeZero")).toLongLong(), qint64(0));
    const auto exactDouble = [&result](QLatin1StringView key, double expected) {
        const QVariant actual = result.value(key);
        QCOMPARE(actual.metaType().id(), QMetaType::Double);
        QCOMPARE(std::bit_cast<quint64>(actual.toDouble()), std::bit_cast<quint64>(expected));
    };
    exactDouble(QLatin1StringView("denormal"), std::numeric_limits<double>::denorm_min());
    exactDouble(QLatin1StringView("maximumDouble"), std::numeric_limits<double>::max());
    exactDouble(QLatin1StringView("decimal"), 0.1);
    QCOMPARE(result.value(QStringLiteral("positiveInsideIntBoundary")).metaType().id(),
             QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("positiveInsideIntBoundary")).toLongLong(),
             static_cast<qint64>(std::nextafter(std::ldexp(1.0, 63), 0.0)));
    exactDouble(QLatin1StringView("positiveIntBoundary"), std::ldexp(1.0, 63));
    exactDouble(QLatin1StringView("positiveOutsideIntBoundary"),
                std::nextafter(std::ldexp(1.0, 63),
                               std::numeric_limits<double>::infinity()));
    QCOMPARE(result.value(QStringLiteral("negativeIntBoundary")).metaType().id(),
             QMetaType::LongLong);
    QCOMPARE(result.value(QStringLiteral("negativeIntBoundary")).toLongLong(),
             std::numeric_limits<qint64>::min());
    exactDouble(QLatin1StringView("negativeOutsideIntBoundary"),
                std::nextafter(-std::ldexp(1.0, 63),
                               -std::numeric_limits<double>::infinity()));
    const QVariantList array = result.value(QStringLiteral("array")).toList();
    QCOMPARE(array.at(0).metaType().id(), QMetaType::Nullptr);
    QCOMPARE(array.at(1).toMap().value(QStringLiteral("nested")).metaType().id(),
             QMetaType::LongLong);

    const SettingsDocument tooWide{
        .schemaVersion = 2,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("displays.configuration"),
                    QVariantMap{{QStringLiteral("tooWide"),
                                 QVariant::fromValue(std::numeric_limits<quint64>::max())}}}}};
    QVERIFY(!SettingsFileStore::save(path, tooWide, *m_schema, nullptr, &error));
    QFile preserved(path);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), json);
}

void SettingsPersistenceTests::rejectsNonRoundTrippableJsonText()
{
    constexpr auto nulValue = R"json({
      "schemaVersion": 2,
      "layer": "user-overrides",
      "values": {"displays.configuration": {"bad": "\u0000"}}
    })json";
    const auto nulLoaded = SettingsDocumentCodec::fromJson(
        nulValue, QStringLiteral("nul-fixture"), *m_schema);
    QVERIFY(!nulLoaded.ok);

    constexpr auto surrogateCollision = R"json({
      "schemaVersion": 2,
      "layer": "user-overrides",
      "values": {"displays.configuration": {"\ud800": true, "\ufffd": false}}
    })json";
    const auto surrogateLoaded = SettingsDocumentCodec::fromJson(
        surrogateCollision, QStringLiteral("surrogate-fixture"), *m_schema);
    QVERIFY(!surrogateLoaded.ok);
}

void SettingsPersistenceTests::rejectsVolatileAndInvalidDocuments()
{
    const SettingsDocument session{.schemaVersion = 2,
                                   .layer = SettingLayer::SessionOverrides,
                                   .values = {}};
    QString error;
    QVERIFY(!SettingsDocumentCodec::toJson(session, *m_schema, nullptr, &error).has_value());
    QVERIFY(error.contains(QStringLiteral("not persistable")));

    constexpr auto invalid = R"json({
      "schemaVersion": 2,
      "layer": "user-overrides",
      "values": {"windowManagement.snapDistance": 999, "unknown.key": true}
    })json";
    const auto loaded = SettingsDocumentCodec::fromJson(invalid, QStringLiteral("fixture"), *m_schema);
    QVERIFY(!loaded.ok);
    QVERIFY(loaded.validation.issues().size() == 2);
}

void SettingsPersistenceTests::failedSavePreservesExistingFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("settings.json"));
    const SettingsDocument valid{.schemaVersion = 2,
                                 .layer = SettingLayer::UserOverrides,
                                 .values = {{QStringLiteral("appearance.blurEnabled"), false}}};
    QString error;
    QVERIFY2(SettingsFileStore::save(path, valid, *m_schema, nullptr, &error), qPrintable(error));
    QFile beforeFile(path);
    QVERIFY(beforeFile.open(QIODevice::ReadOnly));
    const auto before = beforeFile.readAll();
    beforeFile.close();

    const SettingsDocument invalid{
        .schemaVersion = 2,
        .layer = SettingLayer::UserOverrides,
        .values = {{QStringLiteral("windowManagement.snapDistance"), 999}}};
    QVERIFY(!SettingsFileStore::save(path, invalid, *m_schema, nullptr, &error));
    QFile afterFile(path);
    QVERIFY(afterFile.open(QIODevice::ReadOnly));
    QCOMPARE(afterFile.readAll(), before);
}

QTEST_GUILESS_MAIN(SettingsPersistenceTests)
#include "tst_settings_persistence.moc"
