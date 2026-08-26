// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"

#include <QJsonArray>
#include <QtTest>

#include <utility>

using namespace QindaQt::Profiles;
using namespace QindaQt::Profiles::TestFixtures;

Q_DECLARE_METATYPE(ProfileErrorCode)

namespace {

void verifyRejected(const QByteArray &json,
                    ProfileErrorCode code,
                    const QString &path)
{
    const LoadResult result = ProfileLoader::fromJson(json, QStringLiteral("fixture.json"));
    QVERIFY(result.error.hasError());
    QCOMPARE(result.error.code, code);
    QCOMPARE(result.error.path, path);
    QCOMPARE(result.error.origin, QStringLiteral("fixture.json"));
    QVERIFY(!result.error.message.isEmpty());
    QVERIFY(result.error.diagnostic().contains(QStringLiteral("fixture.json")));
    if (code == ProfileErrorCode::InvalidJson
        || code == ProfileErrorCode::DuplicateJsonKey
        || code == ProfileErrorCode::ExcessiveNesting) {
        QVERIFY(result.error.byteOffset >= 0);
    }
    QVERIFY(!result.ok);
    QVERIFY(result.profile.panels.isEmpty());
    QVERIFY(result.profile.id.isEmpty());
}

void replaceRequired(QByteArray *json,
                     const QByteArray &before,
                     const QByteArray &after)
{
    QVERIFY(json->contains(before));
    json->replace(before, after);
}

} // namespace

class ProfileJsonValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void rejectsMalformedDocuments();
    void rejectsNonStandardJsonSyntax();
    void rejectsInvalidUtf8AndExcessiveNesting();
    void rejectsStrictFieldTypes_data();
    void rejectsStrictFieldTypes();
    void rejectsSemanticViolations();
    void acceptsExactIntegralSyntaxAndInclusiveBounds();
    void rejectsIntegerOverflow();
    void appliesDefaultsOnlyWhenFieldsAreAbsent();
    void preservesJsonSettingsAndDropsUnknownFields();
};

void ProfileJsonValidationTests::rejectsMalformedDocuments()
{
    verifyRejected(QByteArrayLiteral("{"), ProfileErrorCode::InvalidJson, {});
    verifyRejected(QByteArrayLiteral("[]"), ProfileErrorCode::InvalidRoot, {});
    verifyRejected(QByteArrayLiteral("null"), ProfileErrorCode::InvalidRoot, {});
    verifyRejected(QByteArrayLiteral("\"profile\""), ProfileErrorCode::InvalidRoot, {});
    verifyRejected(QByteArrayLiteral("1"), ProfileErrorCode::InvalidRoot, {});
}

void ProfileJsonValidationTests::rejectsNonStandardJsonSyntax()
{
    QByteArray json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"Fixture\""),
                    QByteArrayLiteral("\"bad\\q\""));
    verifyRejected(json, ProfileErrorCode::InvalidJson, QStringLiteral("/name"));

    json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"Fixture\""),
                    QByteArrayLiteral("\"bad\nname\""));
    verifyRejected(json, ProfileErrorCode::InvalidJson, QStringLiteral("/name"));

    json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"schemaVersion\":1"),
                    QByteArrayLiteral("\"schemaVersion\":.1"));
    verifyRejected(json, ProfileErrorCode::InvalidJson,
                   QStringLiteral("/schemaVersion"));

    json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"schemaVersion\":1"),
                    QByteArrayLiteral("\"schemaVersion\":1."));
    verifyRejected(json, ProfileErrorCode::InvalidJson,
                   QStringLiteral("/schemaVersion"));

    json = encode(validProfileObject());
    json.insert(1, QByteArrayLiteral("\"schemaVersion\":1,"));
    verifyRejected(json, ProfileErrorCode::DuplicateJsonKey,
                   QStringLiteral("/schemaVersion"));

    json = encode(validProfileObject());
    json.insert(1, QByteArrayLiteral("\"\\u0069d\":\"shadow\","));
    verifyRejected(json, ProfileErrorCode::DuplicateJsonKey, QStringLiteral("/id"));

    json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"Fixture\""),
                    QByteArrayLiteral("\"\\uD800\""));
    verifyRejected(json, ProfileErrorCode::InvalidJson, QStringLiteral("/name"));
}

void ProfileJsonValidationTests::rejectsInvalidUtf8AndExcessiveNesting()
{
    QByteArray invalidUtf8 = encode(validProfileObject());
    QByteArray invalidName(1, '"');
    invalidName.append(static_cast<char>(0xC3));
    invalidName.append('(');
    invalidName.append('"');
    replaceRequired(&invalidUtf8, QByteArrayLiteral("\"Fixture\""), invalidName);
    verifyRejected(invalidUtf8, ProfileErrorCode::InvalidJson, QStringLiteral("/name"));

    QByteArray nested = QByteArrayLiteral(
        "{\"schemaVersion\":1,\"id\":\"nested\",\"name\":\"Nested\",\"panels\":["
        "{\"id\":\"panel\"}],\"unknown\":");
    nested.append(66, '[');
    nested.append(QByteArrayLiteral("null"));
    nested.append(66, ']');
    nested.append('}');
    const LoadResult result = ProfileLoader::fromJson(nested, QStringLiteral("fixture.json"));
    QCOMPARE(result.error.code, ProfileErrorCode::ExcessiveNesting);
    QVERIFY(result.error.byteOffset >= 0);
    QVERIFY(!result.ok);
}

void ProfileJsonValidationTests::rejectsStrictFieldTypes_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<ProfileErrorCode>("code");
    QTest::addColumn<QString>("path");

    const auto add = [](const char *name,
                        QJsonObject root,
                        ProfileErrorCode code,
                        QString path) {
        QTest::newRow(name) << encode(root) << code << std::move(path);
    };

    QJsonObject root = validProfileObject();
    root.remove(QStringLiteral("schemaVersion"));
    add("missing schema", root, ProfileErrorCode::MissingRequiredField,
        QStringLiteral("/schemaVersion"));

    root = validProfileObject();
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("1"));
    add("string schema", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/schemaVersion"));

    root = validProfileObject();
    root.insert(QStringLiteral("schemaVersion"), 1.5);
    add("fractional schema", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/schemaVersion"));

    root = validProfileObject();
    root.insert(QStringLiteral("id"), 7);
    add("numeric id", root, ProfileErrorCode::InvalidFieldType, QStringLiteral("/id"));

    root = validProfileObject();
    root.insert(QStringLiteral("name"), QJsonValue::Null);
    add("null name", root, ProfileErrorCode::InvalidFieldType, QStringLiteral("/name"));

    root = validProfileObject();
    root.insert(QStringLiteral("description"), false);
    add("boolean description", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/description"));

    root = validProfileObject();
    root.insert(QStringLiteral("workflow"), QJsonArray{});
    add("array workflow", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/workflow"));

    root = validProfileObject();
    root.insert(QStringLiteral("workflow"),
                QJsonObject{{QStringLiteral("globalMenu"), QStringLiteral("true")}});
    add("string workflow boolean", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/workflow/globalMenu"));

    root = validProfileObject();
    root.remove(QStringLiteral("panels"));
    add("missing panels", root, ProfileErrorCode::MissingRequiredField,
        QStringLiteral("/panels"));

    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonObject{});
    add("object panels", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels"));

    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{QStringLiteral("panel")});
    add("scalar panel", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0"));

    auto panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.remove(QStringLiteral("id"));
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("missing panel id", root, ProfileErrorCode::MissingRequiredField,
        QStringLiteral("/panels/0/id"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("output"), QJsonValue::Null);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("null output", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/output"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("edge"), QStringLiteral("TOP"));
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("noncanonical edge", root, ProfileErrorCode::InvalidEnumValue,
        QStringLiteral("/panels/0/edge"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("layer"), false);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("boolean layer", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/layer"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("hideMode"), QJsonValue::Null);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("null hide mode", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/hideMode"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("alignment"), 7);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("numeric alignment", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/alignment"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("rows"), QStringLiteral("2"));
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("string rows", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/rows"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("thickness"), 32.5);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("fractional thickness", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/thickness"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("length"), QStringLiteral("1.0"));
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("string length", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/length"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("applets"), QJsonObject{});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("object applets", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/applets"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("applets"), QJsonArray{false});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("scalar applet", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/applets/0"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    auto applet = panel.value(QStringLiteral("applets")).toArray()[0].toObject();
    applet.insert(QStringLiteral("id"), 1);
    panel.insert(QStringLiteral("applets"), QJsonArray{applet});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("numeric applet id", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/applets/0/id"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    applet = panel.value(QStringLiteral("applets")).toArray()[0].toObject();
    applet.remove(QStringLiteral("plugin"));
    panel.insert(QStringLiteral("applets"), QJsonArray{applet});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("missing applet plugin", root, ProfileErrorCode::MissingRequiredField,
        QStringLiteral("/panels/0/applets/0/plugin"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    applet = panel.value(QStringLiteral("applets")).toArray()[0].toObject();
    applet.insert(QStringLiteral("settings"), QJsonArray{});
    panel.insert(QStringLiteral("applets"), QJsonArray{applet});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    add("array settings", root, ProfileErrorCode::InvalidFieldType,
        QStringLiteral("/panels/0/applets/0/settings"));
}

void ProfileJsonValidationTests::rejectsStrictFieldTypes()
{
    QFETCH(QByteArray, json);
    QFETCH(ProfileErrorCode, code);
    QFETCH(QString, path);
    verifyRejected(json, code, path);
}

void ProfileJsonValidationTests::rejectsSemanticViolations()
{
    QJsonObject root = validProfileObject();
    root.insert(QStringLiteral("schemaVersion"), 2);
    verifyRejected(encode(root), ProfileErrorCode::UnsupportedSchemaVersion,
                   QStringLiteral("/schemaVersion"));

    root = validProfileObject();
    root.insert(QStringLiteral("id"), QStringLiteral("   "));
    verifyRejected(encode(root), ProfileErrorCode::InvalidIdentifier, QStringLiteral("/id"));

    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{});
    verifyRejected(encode(root), ProfileErrorCode::EmptyPanelSet, QStringLiteral("/panels"));

    auto panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("rows"), 5);
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    verifyRejected(encode(root), ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/rows"));

    panel = validProfileObject().value(QStringLiteral("panels")).toArray()[0].toObject();
    auto secondPanel = panel;
    secondPanel.insert(QStringLiteral("applets"), QJsonArray{});
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel, secondPanel});
    verifyRejected(encode(root), ProfileErrorCode::DuplicatePanelId,
                   QStringLiteral("/panels/1/id"));

    secondPanel.insert(QStringLiteral("id"), QStringLiteral("secondary"));
    secondPanel.insert(QStringLiteral("applets"),
                       panel.value(QStringLiteral("applets")));
    root = validProfileObject();
    root.insert(QStringLiteral("panels"), QJsonArray{panel, secondPanel});
    verifyRejected(encode(root), ProfileErrorCode::DuplicateAppletId,
                   QStringLiteral("/panels/1/applets/0/id"));

    root = validProfileObject();
    root.insert(QStringLiteral("defaultTheme"), QStringLiteral(" theme"));
    verifyRejected(encode(root), ProfileErrorCode::InvalidIdentifier,
                   QStringLiteral("/defaultTheme"));
}

void ProfileJsonValidationTests::acceptsExactIntegralSyntaxAndInclusiveBounds()
{
    QByteArray json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"schemaVersion\":1"),
                    QByteArrayLiteral("\"schemaVersion\":1e0"));
    replaceRequired(&json,
                    QByteArrayLiteral("\"rows\":1"),
                    QByteArrayLiteral("\"rows\":4.0"));
    replaceRequired(&json,
                    QByteArrayLiteral("\"thickness\":32"),
                    QByteArrayLiteral("\"thickness\":192"));
    replaceRequired(&json,
                    QByteArrayLiteral("\"length\":1"),
                    QByteArrayLiteral("\"length\":0.1"));

    const LoadResult upperLower =
        ProfileLoader::fromJson(json, QStringLiteral("integral-and-bounds"));
    QVERIFY2(upperLower.ok, qPrintable(upperLower.error.diagnostic()));
    QCOMPARE(upperLower.profile.schemaVersion, 1);
    QCOMPARE(upperLower.profile.panels[0].rows, 4);
    QCOMPARE(upperLower.profile.panels[0].thickness, 192);
    QCOMPARE(upperLower.profile.panels[0].length, 0.1);

    QJsonObject root = validProfileObject();
    QJsonObject panel = root.value(QStringLiteral("panels")).toArray()[0].toObject();
    panel.insert(QStringLiteral("thickness"), 20);
    panel.insert(QStringLiteral("length"), 1.0);
    root.insert(QStringLiteral("panels"), QJsonArray{panel});
    const LoadResult lowerUpper =
        ProfileLoader::fromJson(encode(root), QStringLiteral("opposite-bounds"));
    QVERIFY2(lowerUpper.ok, qPrintable(lowerUpper.error.diagnostic()));
}

void ProfileJsonValidationTests::rejectsIntegerOverflow()
{
    QByteArray json = encode(validProfileObject());
    replaceRequired(&json,
                    QByteArrayLiteral("\"schemaVersion\":1"),
                    QByteArrayLiteral("\"schemaVersion\":9223372036854775808"));
    verifyRejected(json, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/schemaVersion"));
}

void ProfileJsonValidationTests::appliesDefaultsOnlyWhenFieldsAreAbsent()
{
    QJsonObject root = validProfileObject();
    root.remove(QStringLiteral("description"));
    root.remove(QStringLiteral("defaultTheme"));
    root.remove(QStringLiteral("workflow"));
    auto panel = root.value(QStringLiteral("panels")).toArray()[0].toObject();
    for (const auto &field : {QStringLiteral("output"),
                              QStringLiteral("edge"),
                              QStringLiteral("layer"),
                              QStringLiteral("hideMode"),
                              QStringLiteral("alignment"),
                              QStringLiteral("rows"),
                              QStringLiteral("thickness"),
                              QStringLiteral("length"),
                              QStringLiteral("applets")}) {
        panel.remove(field);
    }
    root.insert(QStringLiteral("panels"), QJsonArray{panel});

    const LoadResult result = ProfileLoader::fromJson(encode(root), QStringLiteral("defaults"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
    QCOMPARE(result.profile.defaultTheme, QStringLiteral("qinda-dark"));
    QCOMPARE(result.profile.workflow.overview, QStringLiteral("compact"));
    QCOMPARE(result.profile.panels.constFirst().output, QStringLiteral("*"));
    QCOMPARE(result.profile.panels.constFirst().rows, 1);
    QCOMPARE(result.profile.panels.constFirst().thickness, 32);
    QCOMPARE(result.profile.panels.constFirst().length, 1.0);
    QVERIFY(result.profile.panels.constFirst().applets.isEmpty());

    root.insert(QStringLiteral("defaultTheme"), QJsonValue::Null);
    verifyRejected(encode(root), ProfileErrorCode::InvalidFieldType,
                   QStringLiteral("/defaultTheme"));
}

void ProfileJsonValidationTests::preservesJsonSettingsAndDropsUnknownFields()
{
    QJsonObject root = validProfileObject();
    root.insert(QStringLiteral("futureRoot"), 42);
    auto panel = root.value(QStringLiteral("panels")).toArray()[0].toObject();
    auto applet = panel.value(QStringLiteral("applets")).toArray()[0].toObject();
    const QJsonObject settings{
        {QStringLiteral("null"), QJsonValue::Null},
        {QStringLiteral("integer"), QJsonValue(qint64{9'007'199'254'740'993})},
        {QStringLiteral("number"), 1.25},
        {QStringLiteral("array"), QJsonArray{true, QStringLiteral("value")}},
        {QStringLiteral("object"), QJsonObject{{QStringLiteral("nested"), 7}}},
    };
    applet.insert(QStringLiteral("settings"), settings);
    panel.insert(QStringLiteral("applets"), QJsonArray{applet});
    root.insert(QStringLiteral("panels"), QJsonArray{panel});

    const LoadResult result = ProfileLoader::fromJson(encode(root), QStringLiteral("round-trip"));
    QVERIFY2(result.ok, qPrintable(result.error.diagnostic()));
    QCOMPARE(QJsonObject::fromVariantMap(result.profile.panels[0].applets[0].settings), settings);
    QVERIFY(!result.profile.toJson().contains(QStringLiteral("futureRoot")));
}

QTEST_GUILESS_MAIN(ProfileJsonValidationTests)
#include "tst_profile_json_validation.moc"
