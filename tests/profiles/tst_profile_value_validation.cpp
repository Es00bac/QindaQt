// SPDX-License-Identifier: GPL-3.0-or-later
#include "profile_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/profiles/profile_validation.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

#include <limits>

using namespace QindaQt::Profiles;
using namespace QindaQt::Profiles::TestFixtures;

namespace {

void verifyRejected(const LayoutProfile &profile,
                    ProfileErrorCode code,
                    const QString &path)
{
    const auto result = ProfileValidator::validate(profile);
    QVERIFY(!result.succeeded());
    QCOMPARE(result.error.code, code);
    QCOMPARE(result.error.path, path);
    QVERIFY(!result.error.message.isEmpty());
}

QString illFormedUtf16()
{
    return QString(QChar(0xD800));
}

} // namespace

class ProfileValueValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidProfileAndPanelLayout();
    void rejectsInvalidTypedLayoutValues();
    void rejectsInvalidTypedIdentities();
    void rejectsDuplicatePanelIdentity();
    void enforcesProfileGlobalAppletIdentity();
    void acceptsJsonNativeSettingsValues();
    void rejectsLossySettingsValues();
    void rejectsLossyWrappedJsonValues();
    void rejectsIllFormedProgrammaticStrings();
    void escapesSettingsPathsAndChoosesDeterministicFailure();
    void boundsSettingsNesting();
    void roundTripsAcceptedProgrammaticBoundaries();
};

void ProfileValueValidationTests::acceptsValidProfileAndPanelLayout()
{
    const LayoutProfile profile = validProfile();
    const auto result = ProfileValidator::validate(profile);
    QVERIFY2(result.succeeded(), "valid profile was rejected");
    QVERIFY(!result.error.hasError());
    QVERIFY(result.error.diagnostic().isEmpty());
    QVERIFY2(ProfileValidator::validatePanelLayout(profile.panels.constFirst()).succeeded(),
             "valid panel layout was rejected");
}

void ProfileValueValidationTests::rejectsInvalidTypedLayoutValues()
{
    LayoutProfile profile = validProfile();
    profile.panels[0].edge = static_cast<Edge>(99);
    verifyRejected(profile, ProfileErrorCode::InvalidEnumValue,
                   QStringLiteral("/panels/0/edge"));

    profile = validProfile();
    profile.panels[0].rows = 0;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/rows"));

    profile = validProfile();
    profile.panels[0].rows = 5;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/rows"));

    profile = validProfile();
    profile.panels[0].thickness = 19;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/thickness"));

    profile = validProfile();
    profile.panels[0].thickness = 193;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/thickness"));

    profile = validProfile();
    profile.panels[0].length = std::numeric_limits<double>::quiet_NaN();
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/length"));

    profile = validProfile();
    profile.panels[0].length = 0.09;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/length"));

    profile = validProfile();
    profile.panels[0].length = 1.01;
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/length"));

    profile = validProfile();
    profile.panels[0].length = std::numeric_limits<double>::infinity();
    verifyRejected(profile, ProfileErrorCode::OutOfRange,
                   QStringLiteral("/panels/0/length"));

    profile = validProfile();
    profile.panels[0].layer = static_cast<Layer>(99);
    verifyRejected(profile, ProfileErrorCode::InvalidEnumValue,
                   QStringLiteral("/panels/0/layer"));

    profile = validProfile();
    profile.panels[0].hideMode = static_cast<HideMode>(99);
    verifyRejected(profile, ProfileErrorCode::InvalidEnumValue,
                   QStringLiteral("/panels/0/hideMode"));

    profile = validProfile();
    profile.panels[0].alignment = static_cast<Alignment>(99);
    verifyRejected(profile, ProfileErrorCode::InvalidEnumValue,
                   QStringLiteral("/panels/0/alignment"));
}

void ProfileValueValidationTests::rejectsInvalidTypedIdentities()
{
    LayoutProfile profile = validProfile();
    profile.id = QStringLiteral(" profile");
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier, QStringLiteral("/id"));

    profile = validProfile();
    profile.name = QStringLiteral(" \t ");
    verifyRejected(profile, ProfileErrorCode::InvalidValue, QStringLiteral("/name"));

    profile = validProfile();
    profile.workflow.menu.clear();
    verifyRejected(profile, ProfileErrorCode::InvalidValue,
                   QStringLiteral("/workflow/menu"));

    profile = validProfile();
    profile.panels[0].output = QStringLiteral("   ");
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier,
                   QStringLiteral("/panels/0/output"));

    profile = validProfile();
    profile.panels[0].applets[0].plugin = QStringLiteral("clock ");
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier,
                   QStringLiteral("/panels/0/applets/0/plugin"));
}

void ProfileValueValidationTests::rejectsDuplicatePanelIdentity()
{
    LayoutProfile profile = validProfile();
    PanelSpec duplicate = profile.panels.constFirst();
    duplicate.applets.clear();
    profile.panels.append(duplicate);

    verifyRejected(profile, ProfileErrorCode::DuplicatePanelId,
                   QStringLiteral("/panels/1/id"));
}

void ProfileValueValidationTests::enforcesProfileGlobalAppletIdentity()
{
    LayoutProfile profile = validProfile();
    PanelSpec second = profile.panels.constFirst();
    second.id = QStringLiteral("secondary");
    profile.panels.append(second);

    verifyRejected(profile, ProfileErrorCode::DuplicateAppletId,
                   QStringLiteral("/panels/1/applets/0/id"));
}

void ProfileValueValidationTests::acceptsJsonNativeSettingsValues()
{
    LayoutProfile profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("null"), QVariant::fromValue(nullptr)},
        {QStringLiteral("boolean"), true},
        {QStringLiteral("integer"), std::numeric_limits<qint64>::max()},
        {QStringLiteral("unsigned"), static_cast<qulonglong>(42)},
        {QStringLiteral("number"), 1.25},
        {QStringLiteral("string"), QStringLiteral("value")},
        {QStringLiteral("strings"), QStringList{QStringLiteral("one"), QStringLiteral("two")}},
        {QStringLiteral("list"), QVariantList{true, qint64{7}, QStringLiteral("value")}},
        {QStringLiteral("map"), QVariantMap{{QStringLiteral("nested"), false}}},
        {QStringLiteral("hash"), QVariantHash{{QStringLiteral("nested"), 3.5}}},
        {QStringLiteral("json-value"), QVariant::fromValue(QJsonValue(QStringLiteral("text")))},
        {QStringLiteral("json-array"),
         QVariant::fromValue(QJsonArray{true, QStringLiteral("value")})},
        {QStringLiteral("json-object"),
         QVariant::fromValue(QJsonObject{{QStringLiteral("nested"), 12}})},
    };

    const auto result = ProfileValidator::validate(profile);
    QVERIFY2(result.succeeded(), qPrintable(result.error.diagnostic()));
}

void ProfileValueValidationTests::rejectsLossySettingsValues()
{
    LayoutProfile profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("nan"), std::numeric_limits<double>::quiet_NaN()},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/nan"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("invalid"), QVariant{}},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/invalid"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("unsigned"), std::numeric_limits<qulonglong>::max()},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/unsigned"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("bytes"), QByteArrayLiteral("not-json")},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/bytes"));

    // The layout-only boundary must not start owning applet settings merely
    // because it shares the persisted PanelSpec value.
    QVERIFY(ProfileValidator::validatePanelLayout(profile.panels.constFirst()).succeeded());
}

void ProfileValueValidationTests::rejectsLossyWrappedJsonValues()
{
    LayoutProfile profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("wrapped-nan"),
         QVariant::fromValue(QJsonValue(std::numeric_limits<double>::quiet_NaN()))},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/wrapped-nan"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("array"),
         QVariant::fromValue(
             QJsonArray{QJsonValue(std::numeric_limits<double>::infinity())})},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/array/0"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("object"),
         QVariant::fromValue(QJsonObject{
             {QStringLiteral("infinite"),
              QJsonValue(-std::numeric_limits<double>::infinity())},
         })},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/object/infinite"));
}

void ProfileValueValidationTests::rejectsIllFormedProgrammaticStrings()
{
    const QString invalid = illFormedUtf16();

    LayoutProfile profile = validProfile();
    profile.id = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier, QStringLiteral("/id"));

    profile = validProfile();
    profile.name = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidValue, QStringLiteral("/name"));

    profile = validProfile();
    profile.description = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidValue, QStringLiteral("/description"));

    profile = validProfile();
    profile.workflow.menu = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidValue,
                   QStringLiteral("/workflow/menu"));

    profile = validProfile();
    profile.panels[0].output = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier,
                   QStringLiteral("/panels/0/output"));

    profile = validProfile();
    profile.panels[0].applets[0].plugin = invalid;
    verifyRejected(profile, ProfileErrorCode::InvalidIdentifier,
                   QStringLiteral("/panels/0/applets/0/plugin"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("string"), invalid},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/string"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("strings"), QStringList{invalid}},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/strings/0"));

    profile = validProfile();
    profile.panels[0].applets[0].settings.insert(invalid, true);
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("json-value"), QVariant::fromValue(QJsonValue(invalid))},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/json-value"));

    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("json-array"), QVariant::fromValue(QJsonArray{invalid})},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/json-array/0"));

    QJsonObject invalidObject;
    invalidObject.insert(invalid, true);
    profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("json-object"), QVariant::fromValue(invalidObject)},
    };
    verifyRejected(profile, ProfileErrorCode::NonJsonSettingsValue,
                   QStringLiteral("/panels/0/applets/0/settings/json-object"));
}

void ProfileValueValidationTests::escapesSettingsPathsAndChoosesDeterministicFailure()
{
    LayoutProfile profile = validProfile();
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("hash"),
         QVariantHash{{QStringLiteral("z-invalid"), QByteArrayLiteral("later")},
                      {QStringLiteral("a~/invalid"), QByteArrayLiteral("first")}}},
    };

    const auto result = ProfileValidator::validate(profile);
    QVERIFY(!result.succeeded());
    QCOMPARE(result.error.code, ProfileErrorCode::NonJsonSettingsValue);
    QCOMPARE(result.error.path,
             QStringLiteral("/panels/0/applets/0/settings/hash/a~0~1invalid"));
    QCOMPARE(result.error.panelId, QStringLiteral("main"));
    QCOMPARE(result.error.appletId, QStringLiteral("clock-instance"));
}

void ProfileValueValidationTests::boundsSettingsNesting()
{
    QVariant nested = true;
    for (int depth = 0; depth < 66; ++depth) {
        nested = QVariantList{nested};
    }

    LayoutProfile profile = validProfile();
    profile.panels[0].applets[0].settings = {{QStringLiteral("deep"), nested}};
    const auto result = ProfileValidator::validate(profile);
    QVERIFY(!result.succeeded());
    QCOMPARE(result.error.code, ProfileErrorCode::NonJsonSettingsValue);
    QVERIFY(result.error.path.startsWith(
        QStringLiteral("/panels/0/applets/0/settings/deep")));
}

void ProfileValueValidationTests::roundTripsAcceptedProgrammaticBoundaries()
{
    const QString supplementary = QString::fromUtf8("well-formed \xF0\x9F\x99\x82");
    LayoutProfile profile = validProfile();
    profile.description = supplementary;
    profile.panels[0].applets[0].settings = {
        {QStringLiteral("null"), QVariant::fromValue(nullptr)},
        {QStringLiteral("minimum-integer"), std::numeric_limits<qint64>::min()},
        {QStringLiteral("maximum-integer"), std::numeric_limits<qint64>::max()},
        {QStringLiteral("maximum-unsigned"),
         static_cast<qulonglong>(std::numeric_limits<qint64>::max())},
        {QStringLiteral("maximum-finite"), std::numeric_limits<double>::max()},
        {QStringLiteral("string"), supplementary},
        {QStringLiteral("strings"), QStringList{supplementary}},
        {QStringLiteral("list"), QVariantList{supplementary, qint64{-7}, false}},
        {QStringLiteral("map"), QVariantMap{{supplementary, true}}},
        {QStringLiteral("hash"), QVariantHash{{supplementary, 1.5}}},
        {QStringLiteral("json-value"), QVariant::fromValue(QJsonValue(supplementary))},
        {QStringLiteral("json-array"), QVariant::fromValue(QJsonArray{supplementary, 12})},
        {QStringLiteral("json-object"),
         QVariant::fromValue(QJsonObject{{supplementary, QJsonValue::Null}})},
    };

    const auto validation = ProfileValidator::validate(profile);
    QVERIFY2(validation.succeeded(), qPrintable(validation.error.diagnostic()));

    const QJsonObject serializedObject = profile.toJson();
    const QByteArray serialized =
        QJsonDocument(serializedObject).toJson(QJsonDocument::Compact);
    const LoadResult loaded =
        ProfileLoader::fromJson(serialized, QStringLiteral("typed boundary round trip"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error.diagnostic()));
    QCOMPARE(loaded.profile.toJson(), serializedObject);
}

QTEST_GUILESS_MAIN(ProfileValueValidationTests)
#include "tst_profile_value_validation.moc"
