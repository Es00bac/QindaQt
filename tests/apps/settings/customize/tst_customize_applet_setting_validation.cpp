// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_applet_setting_validation.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <limits>

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

// One "value" property of type "integer", with `minimum`/`maximum` set to
// exactly the given JSON values (a default-constructed QJsonValue() is Null,
// distinct from omitting the key entirely).
QJsonObject integerSchema(const QJsonValue &minimum, const QJsonValue &maximum)
{
    QJsonObject property{{QStringLiteral("type"), QStringLiteral("integer")}};
    property.insert(QStringLiteral("minimum"), minimum);
    property.insert(QStringLiteral("maximum"), maximum);
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("value"), property}}}};
}

QJsonObject enumSchema(const QJsonArray &choices)
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("value"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                      {QStringLiteral("enum"), choices}}}}}};
}

QJsonObject propertyOf(const QJsonObject &schema, const QString &key)
{
    return schema.value(QStringLiteral("properties")).toObject().value(key).toObject();
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
    void classifiesOutOfIntWidthOrMalformedIntegerBoundsAsUnsupported();
    void neverNarrowsAnInBoundsValueAtTheIntStorageWidthExtremes();
    void rejectsSignedUnsignedAndDoubleValuesAboveIntMaxAgainstNormalBounds();
    void classifiesNumericOrMixedEnumMembersAsUnsupported();
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

// Review finding 2: a bound must be a finite, integral, int-representable
// number with minimum <= maximum, or the whole field is Unsupported --
// including the exact [0, 2147483648] shape the review's probe used to
// reach the finding-1 narrowing corruption in the first place.
void CustomizeAppletSettingValidationTests::
    classifiesOutOfIntWidthOrMalformedIntegerBoundsAsUnsupported()
{
    const auto maxInt = static_cast<double>(std::numeric_limits<int>::max());
    const auto minInt = static_cast<double>(std::numeric_limits<int>::min());
    const struct {
        const char *label;
        QJsonValue minimum;
        QJsonValue maximum;
    } cases[] = {
        {"maximum one above INT_MAX", 0, maxInt + 1.0},
        {"minimum one below INT_MIN", minInt - 1.0, 0},
        {"reversed bounds", 10, 1},
        {"fractional minimum", 0.5, 60},
        {"fractional maximum", 0, 60.25},
        {"null minimum", QJsonValue(), 60},
        {"null maximum", 0, QJsonValue()},
        {"string minimum", QStringLiteral("0"), 60},
        {"string maximum", 0, QStringLiteral("60")},
        {"boolean minimum", false, 60},
    };
    for (const auto &testCase : cases) {
        const QJsonObject schema = integerSchema(testCase.minimum, testCase.maximum);
        QCOMPARE(appletSettingFieldKind(propertyOf(schema, QStringLiteral("value"))),
                 AppletSettingFieldKind::Unsupported);
        const auto result =
            validateAppletSettingValue(schema, QStringLiteral("value"), 5);
        QVERIFY2(!result.ok(), testCase.label);
    }
    // The exact shape the review's probe used to reach the finding-1
    // corruption (in-range-per-schema but not int-representable).
    const QJsonObject huge = integerSchema(0, 2147483648.0);
    QCOMPARE(appletSettingFieldKind(propertyOf(huge, QStringLiteral("value"))),
             AppletSettingFieldKind::Unsupported);
    QVERIFY(!validateAppletSettingValue(huge, QStringLiteral("value"),
                                        qint64(2147483648))
                 .ok());
}

// Review finding 1: a value at the exact int storage-width extreme, declared
// in-bounds by a schema whose own bounds are themselves int-representable,
// must round-trip exactly -- never narrow, wrap, or silently corrupt.
void CustomizeAppletSettingValidationTests::
    neverNarrowsAnInBoundsValueAtTheIntStorageWidthExtremes()
{
    const auto maxInt = std::numeric_limits<int>::max();
    const auto minInt = std::numeric_limits<int>::min();
    const QJsonObject fullRange = integerSchema(static_cast<double>(minInt),
                                                static_cast<double>(maxInt));

    const auto atMax = validateAppletSettingValue(fullRange, QStringLiteral("value"),
                                                  QVariant(maxInt));
    QVERIFY(atMax.ok());
    QCOMPARE(atMax.value.toInt(), maxInt);

    const auto atMin = validateAppletSettingValue(fullRange, QStringLiteral("value"),
                                                  QVariant(minInt));
    QVERIFY(atMin.ok());
    QCOMPARE(atMin.value.toInt(), minInt);

    // The same extremes offered as an integral double, the shape a QML
    // Slider's Math.round(value) call actually produces.
    const auto atMaxDouble = validateAppletSettingValue(
        fullRange, QStringLiteral("value"), QVariant(static_cast<double>(maxInt)));
    QVERIFY(atMaxDouble.ok());
    QCOMPARE(atMaxDouble.value.toInt(), maxInt);
}

void CustomizeAppletSettingValidationTests::
    rejectsSignedUnsignedAndDoubleValuesAboveIntMaxAgainstNormalBounds()
{
    // Ordinary small bounds: every hostile value below must be rejected as
    // out of range, not accepted-and-narrowed.
    const QJsonObject schema = boundedIntegerSchema();
    const auto rejects = [&](const QVariant &hostile) {
        const auto result = validateAppletSettingValue(
            schema, QStringLiteral("refreshSeconds"), hostile);
        QVERIFY(!result.ok());
    };
    rejects(QVariant(qint64(2147483648))); // one above INT_MAX, as qint64
    rejects(QVariant(quint32(4000000000u))); // fits UInt, exceeds INT_MAX
    rejects(QVariant(std::numeric_limits<qint64>::max()));
    rejects(QVariant(std::numeric_limits<quint64>::max()));
    rejects(QVariant(2147483648.0)); // one above INT_MAX, as an integral double
    rejects(QVariant(std::numeric_limits<double>::infinity()));
    rejects(QVariant(-std::numeric_limits<double>::infinity()));
    rejects(QVariant(std::numeric_limits<double>::quiet_NaN()));
}

// Review finding 2: an enum choice list is a closed *string* choice only.
void CustomizeAppletSettingValidationTests::
    classifiesNumericOrMixedEnumMembersAsUnsupported()
{
    const QJsonObject numeric = enumSchema(QJsonArray{7});
    QCOMPARE(appletSettingFieldKind(propertyOf(numeric, QStringLiteral("value"))),
             AppletSettingFieldKind::Unsupported);
    QVERIFY(!validateAppletSettingValue(numeric, QStringLiteral("value"), QString())
                 .ok());

    const QJsonObject mixed =
        enumSchema(QJsonArray{QStringLiteral("leading"), 7});
    QCOMPARE(appletSettingFieldKind(propertyOf(mixed, QStringLiteral("value"))),
             AppletSettingFieldKind::Unsupported);
    QVERIFY(!validateAppletSettingValue(mixed, QStringLiteral("value"),
                                        QStringLiteral("leading"))
                 .ok());

    const QJsonObject empty = enumSchema(QJsonArray{});
    QCOMPARE(appletSettingFieldKind(propertyOf(empty, QStringLiteral("value"))),
             AppletSettingFieldKind::Unsupported);
}

QTEST_MAIN(CustomizeAppletSettingValidationTests)
#include "tst_customize_applet_setting_validation.moc"
