// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_applet_setting_validation.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace QindaQt::Apps::SettingsCustomize;

namespace {

QJsonObject booleanSchema()
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("showIcon"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                                      {QStringLiteral("default"), true}}}}}};
}

QJsonObject boundedIntegerSchema()
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{
                 {QStringLiteral("refreshSeconds"),
                  QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                              {QStringLiteral("minimum"), 1},
                              {QStringLiteral("maximum"), 60},
                              {QStringLiteral("default"), 5}}}}}};
}

QJsonObject enumChoiceSchema()
{
    const QJsonObject alignment{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("enum"),
         QJsonArray{QStringLiteral("leading"), QStringLiteral("center"),
                   QStringLiteral("trailing")}},
        {QStringLiteral("default"), QStringLiteral("leading")},
    };
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("alignment"), alignment}}}};
}

QJsonObject freeformStringSchema()
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("labelFormat"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                      {QStringLiteral("default"), QStringLiteral("short")}}}}}};
}

QJsonObject unboundedIntegerSchema()
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("count"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}}}}}};
}

} // namespace

class CustomizeAppletSettingValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void classifiesEveryDeclaredKindCorrectly();
    void acceptsATrueBoolean();
    void rejectsANonBooleanForABooleanField();
    void acceptsAnInBoundsInteger();
    void rejectsAnOutOfRangeInteger();
    void rejectsAFractionalDoubleForAnIntegerField();
    void rejectsANonNumericTypeForAnIntegerField();
    void acceptsAListedEnumChoice();
    void rejectsAnUnlistedEnumChoice();
    void rejectsANonStringForAnEnumField();
    void rejectsAnUnknownKey();
    void rejectsAFreeformStringWithoutAnEnum();
    void rejectsAnIntegerWithoutDeclaredBounds();
    void schemaDefaultReadsTheDeclaredDefault();
    void schemaDefaultIsInvalidForAnUndeclaredKey();
};

void CustomizeAppletSettingValidationTests::classifiesEveryDeclaredKindCorrectly()
{
    QCOMPARE(appletSettingFieldKind(
                 booleanSchema().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("showIcon")).toObject()),
             AppletSettingFieldKind::Boolean);
    QCOMPARE(appletSettingFieldKind(
                 boundedIntegerSchema().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("refreshSeconds")).toObject()),
             AppletSettingFieldKind::BoundedInteger);
    QCOMPARE(appletSettingFieldKind(
                 enumChoiceSchema().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("alignment")).toObject()),
             AppletSettingFieldKind::EnumChoice);
    QCOMPARE(appletSettingFieldKind(
                 freeformStringSchema().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("labelFormat")).toObject()),
             AppletSettingFieldKind::Unsupported);
    QCOMPARE(appletSettingFieldKind(
                 unboundedIntegerSchema().value(QStringLiteral("properties")).toObject()
                     .value(QStringLiteral("count")).toObject()),
             AppletSettingFieldKind::Unsupported);
    QCOMPARE(appletSettingFieldKind({}), AppletSettingFieldKind::Unsupported);
}

void CustomizeAppletSettingValidationTests::acceptsATrueBoolean()
{
    const auto result =
        validateAppletSettingValue(booleanSchema(), QStringLiteral("showIcon"), true);
    QVERIFY(result.ok());
    QCOMPARE(result.value.toBool(), true);
}

void CustomizeAppletSettingValidationTests::rejectsANonBooleanForABooleanField()
{
    for (const QVariant &hostile :
         {QVariant(QStringLiteral("true")), QVariant(1), QVariant(1.0)}) {
        const auto result =
            validateAppletSettingValue(booleanSchema(), QStringLiteral("showIcon"), hostile);
        QVERIFY(!result.ok());
    }
}

void CustomizeAppletSettingValidationTests::acceptsAnInBoundsInteger()
{
    const auto result = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 30);
    QVERIFY(result.ok());
    QCOMPARE(result.value.toInt(), 30);

    const auto atMinimum = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 1);
    QVERIFY(atMinimum.ok());
    const auto atMaximum = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 60);
    QVERIFY(atMaximum.ok());
}

void CustomizeAppletSettingValidationTests::rejectsAnOutOfRangeInteger()
{
    const auto tooLow = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 0);
    QVERIFY(!tooLow.ok());
    const auto tooHigh = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 61);
    QVERIFY(!tooHigh.ok());
    const auto negative = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), -100);
    QVERIFY(!negative.ok());
}

void CustomizeAppletSettingValidationTests::rejectsAFractionalDoubleForAnIntegerField()
{
    const auto result = validateAppletSettingValue(
        boundedIntegerSchema(), QStringLiteral("refreshSeconds"), 5.5);
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::rejectsANonNumericTypeForAnIntegerField()
{
    for (const QVariant &hostile : {QVariant(QStringLiteral("30")), QVariant(true)}) {
        const auto result = validateAppletSettingValue(
            boundedIntegerSchema(), QStringLiteral("refreshSeconds"), hostile);
        QVERIFY(!result.ok());
    }
}

void CustomizeAppletSettingValidationTests::acceptsAListedEnumChoice()
{
    const auto result = validateAppletSettingValue(
        enumChoiceSchema(), QStringLiteral("alignment"), QStringLiteral("center"));
    QVERIFY(result.ok());
    QCOMPARE(result.value.toString(), QStringLiteral("center"));
}

void CustomizeAppletSettingValidationTests::rejectsAnUnlistedEnumChoice()
{
    const auto result = validateAppletSettingValue(
        enumChoiceSchema(), QStringLiteral("alignment"), QStringLiteral("nonexistent"));
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::rejectsANonStringForAnEnumField()
{
    const auto result =
        validateAppletSettingValue(enumChoiceSchema(), QStringLiteral("alignment"), 1);
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::rejectsAnUnknownKey()
{
    const auto result = validateAppletSettingValue(
        booleanSchema(), QStringLiteral("doesNotExist"), true);
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::rejectsAFreeformStringWithoutAnEnum()
{
    const auto result = validateAppletSettingValue(
        freeformStringSchema(), QStringLiteral("labelFormat"), QStringLiteral("long"));
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::rejectsAnIntegerWithoutDeclaredBounds()
{
    const auto result =
        validateAppletSettingValue(unboundedIntegerSchema(), QStringLiteral("count"), 3);
    QVERIFY(!result.ok());
}

void CustomizeAppletSettingValidationTests::schemaDefaultReadsTheDeclaredDefault()
{
    QCOMPARE(appletSettingSchemaDefault(booleanSchema(), QStringLiteral("showIcon")).toBool(),
             true);
    QCOMPARE(appletSettingSchemaDefault(enumChoiceSchema(), QStringLiteral("alignment"))
                 .toString(),
             QStringLiteral("leading"));
}

void CustomizeAppletSettingValidationTests::schemaDefaultIsInvalidForAnUndeclaredKey()
{
    QVERIFY(!appletSettingSchemaDefault(booleanSchema(), QStringLiteral("doesNotExist"))
                 .isValid());
}

QTEST_MAIN(CustomizeAppletSettingValidationTests)
#include "tst_customize_applet_setting_validation.moc"
