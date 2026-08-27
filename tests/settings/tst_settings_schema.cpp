// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/settings/settings_schema.h"

#include <QSet>
#include <QtTest>

using namespace QindaQt::Settings;

class SettingsSchemaTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsBuiltInSchemaWithEveryDomain();
    void rejectsMismatchedDomainAndDuplicateKey();
    void validatesTypesBoundsAndEnums();
    void normalizesJsonNumericDefaults();
    void doNotDisturbDefaultsToDisabled();
    void rejectsAnUnsupportedSchemaVersion();
};

void SettingsSchemaTests::loadsBuiltInSchemaWithEveryDomain()
{
    ValidationResult validation;
    QString error;
    const auto schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), &validation, &error);
    QVERIFY2(schema.has_value(), qPrintable(error));
    QVERIFY(validation.isValid());
    QCOMPARE(schema->systemDefaults().size(), schema->definitions().size());
    QVERIFY(schema->definitions().size() >= 35);

    QSet<QString> domains;
    for (const auto &definition : schema->definitions()) {
        domains.insert(toString(definition.domain));
    }
    const QSet<QString> expected{QStringLiteral("appearance"),
                                 QStringLiteral("fonts"),
                                 QStringLiteral("displays"),
                                 QStringLiteral("input"),
                                 QStringLiteral("panels"),
                                 QStringLiteral("window-management"),
                                 QStringLiteral("accessibility"),
                                 QStringLiteral("services")};
    QCOMPARE(domains, expected);
}

void SettingsSchemaTests::rejectsMismatchedDomainAndDuplicateKey()
{
    constexpr auto invalid = R"json({
      "schemaVersion": 2,
      "settings": [
        {"key":"appearance.theme","domain":"appearance","type":"string","default":"dark"},
        {"key":"appearance.theme","domain":"appearance","type":"string","default":"light"},
        {"key":"fonts.family","domain":"appearance","type":"string","default":"Sans"}
      ]
    })json";

    ValidationResult validation;
    QString error;
    const auto schema = SettingsSchema::fromJson(invalid, QStringLiteral("fixture"), &validation, &error);
    QVERIFY(!schema.has_value());
    QVERIFY(!validation.isValid());
    QVERIFY(error.contains(QStringLiteral("key prefix")));
    QVERIFY(error.contains(QStringLiteral("repeated")));
}

void SettingsSchemaTests::validatesTypesBoundsAndEnums()
{
    QString error;
    const auto schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(schema.has_value(), qPrintable(error));

    QVERIFY(schema->validateValue(QStringLiteral("appearance.blurEnabled"), false).isValid());
    QVERIFY(!schema->validateValue(QStringLiteral("appearance.blurEnabled"), 0).isValid());
    QVERIFY(!schema->validateValue(QStringLiteral("fonts.pointSize"), 72.0).isValid());
    QVERIFY(!schema->validateValue(QStringLiteral("fonts.hinting"), QStringLiteral("extreme")).isValid());
    QVERIFY(!schema->validateValue(QStringLiteral("unknown.key"), true).isValid());
}

void SettingsSchemaTests::normalizesJsonNumericDefaults()
{
    QString error;
    const auto schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(schema.has_value(), qPrintable(error));

    const auto integer = schema->systemDefaults().value(QStringLiteral("panels.autoHideDelayMs"));
    const auto number = schema->systemDefaults().value(QStringLiteral("fonts.pointSize"));
    QCOMPARE(integer.metaType().id(), QMetaType::LongLong);
    QCOMPARE(integer.toLongLong(), 250);
    QCOMPARE(number.metaType().id(), QMetaType::Double);
    QCOMPARE(number.toDouble(), 10.0);
}

void SettingsSchemaTests::doNotDisturbDefaultsToDisabled()
{
    QString error;
    const auto schema = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"), nullptr, &error);
    QVERIFY2(schema.has_value(), qPrintable(error));
    const auto *definition = schema->definition(QStringLiteral("services.doNotDisturb"));
    QVERIFY(definition != nullptr);
    QVERIFY(definition->domain == SettingDomain::Services);
    QVERIFY(definition->type == SettingValueType::Boolean);
    QCOMPARE(schema->systemDefaults().value(QStringLiteral("services.doNotDisturb")).toBool(), false);
    QVERIFY(schema->validateValue(QStringLiteral("services.doNotDisturb"), true).isValid());
    QVERIFY(!schema->validateValue(QStringLiteral("services.doNotDisturb"), QStringLiteral("on")).isValid());
}

void SettingsSchemaTests::rejectsAnUnsupportedSchemaVersion()
{
    QString error;
    const auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error);
    QVERIFY(!active.has_value());
    QVERIFY(error.contains(QStringLiteral("unsupported schemaVersion 1")));
    const auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"), nullptr, &error, 1);
    QVERIFY2(legacy.has_value(), qPrintable(error));
    QVERIFY(legacy->definition(QStringLiteral("services.doNotDisturb")) == nullptr);
}

QTEST_GUILESS_MAIN(SettingsSchemaTests)
#include "tst_settings_schema.moc"
