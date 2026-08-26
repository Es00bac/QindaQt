// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/manifest_loader.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace QindaQt::Applets;

namespace {

QJsonObject validManifestObject()
{
    constexpr auto json = R"json({
        "schemaVersion": 1,
        "id": "fixture-applet",
        "name": "Fixture Applet",
        "apiVersion": "1.0",
        "entryPoint": {"kind": "qml", "value": "ui/Main.qml"},
        "placements": {
            "zones": ["panel-start", "panel-end"],
            "orientations": ["horizontal", "vertical"]
        },
        "sizing": {
            "mainAxis": {"minimum": 24, "preferred": 48, "maximum": 128},
            "crossAxis": {"minimum": 24, "preferred": 32, "maximum": 64}
        },
        "capabilities": ["settings.read"],
        "settingsSchema": {"type": "object", "properties": {}}
    })json";
    return QJsonDocument::fromJson(json).object();
}

ManifestLoadResult loadObject(const QJsonObject &object)
{
    return ManifestLoader::fromJson(QJsonDocument(object).toJson(QJsonDocument::Compact),
                                    QStringLiteral("fixture"));
}

} // namespace

class ManifestTest final : public QObject {
    Q_OBJECT

private slots:
    void negotiatesApiMinorVersions();
    void roundTripsEveryFirstPartyManifest();
    void preservesFutureApiRequirements();
    void rejectsUnknownCapabilities();
    void rejectsInvalidSizing();
    void rejectsPackageTraversal();
    void rejectsDuplicatePlacementValues();
    void rejectsMalformedSettingsSchema();
    void rejectsUnknownManifestSchema();
};

void ManifestTest::negotiatesApiMinorVersions()
{
    const ApiVersion host{1, 3};
    QVERIFY((ApiVersion{1, 0}.isSupportedBy(host)));
    QVERIFY((ApiVersion{1, 3}.isSupportedBy(host)));
    QVERIFY((!ApiVersion{1, 4}.isSupportedBy(host)));
    QVERIFY((!ApiVersion{2, 0}.isSupportedBy(host)));
    QVERIFY((!ApiVersion{0, 9}.isSupportedBy(host)));
    QCOMPARE(ApiVersion::parse(QStringLiteral("1.12")), std::optional(ApiVersion{1, 12}));
    QVERIFY(!ApiVersion::parse(QStringLiteral("1")).has_value());
}

void ManifestTest::roundTripsEveryFirstPartyManifest()
{
    const QDir directory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"));
    const QStringList fileNames =
        directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QCOMPARE(fileNames.size(), 5);
    for (const QString &fileName : fileNames) {
        const ManifestLoadResult loaded = ManifestLoader::fromFile(directory.filePath(fileName));
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        const ManifestLoadResult roundTrip = ManifestLoader::fromJson(
            ManifestLoader::toJson(loaded.manifest), QStringLiteral("round-trip"));
        QVERIFY2(roundTrip.ok, qPrintable(roundTrip.error));
        QVERIFY(roundTrip.manifest == loaded.manifest);
    }
}

void ManifestTest::preservesFutureApiRequirements()
{
    QJsonObject object = validManifestObject();
    object.insert(QStringLiteral("apiVersion"), QStringLiteral("1.1"));
    const ManifestLoadResult result = loadObject(object);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY(!result.manifest.supportsHost(ApiVersion::current()));
    QVERIFY(result.manifest.supportsHost(ApiVersion{1, 2}));
}

void ManifestTest::rejectsUnknownCapabilities()
{
    QJsonObject object = validManifestObject();
    object.insert(QStringLiteral("capabilities"), QJsonArray{QStringLiteral("shell.everything")});
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("capabilities")));
}

void ManifestTest::rejectsInvalidSizing()
{
    QJsonObject object = validManifestObject();
    QJsonObject sizing = object.value(QStringLiteral("sizing")).toObject();
    sizing.insert(QStringLiteral("mainAxis"),
                  QJsonObject{{QStringLiteral("minimum"), 100},
                              {QStringLiteral("preferred"), 50},
                              {QStringLiteral("maximum"), 40}});
    object.insert(QStringLiteral("sizing"), sizing);
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("mainAxis")));
}

void ManifestTest::rejectsPackageTraversal()
{
    QJsonObject object = validManifestObject();
    object.insert(QStringLiteral("entryPoint"),
                  QJsonObject{{QStringLiteral("kind"), QStringLiteral("executable")},
                              {QStringLiteral("value"), QStringLiteral("../escape")}});
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("package-relative")));
}

void ManifestTest::rejectsDuplicatePlacementValues()
{
    QJsonObject object = validManifestObject();
    QJsonObject placements = object.value(QStringLiteral("placements")).toObject();
    placements.insert(QStringLiteral("zones"),
                      QJsonArray{QStringLiteral("panel-start"),
                                 QStringLiteral("panel-start")});
    object.insert(QStringLiteral("placements"), placements);
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("duplicate")));
}

void ManifestTest::rejectsMalformedSettingsSchema()
{
    QJsonObject object = validManifestObject();
    object.insert(QStringLiteral("settingsSchema"),
                  QJsonObject{{QStringLiteral("type"), QStringLiteral("array")}});
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("settingsSchema.type")));
}

void ManifestTest::rejectsUnknownManifestSchema()
{
    QJsonObject object = validManifestObject();
    object.insert(QStringLiteral("schemaVersion"), 2);
    const ManifestLoadResult result = loadObject(object);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("schemaVersion")));
}

QTEST_GUILESS_MAIN(ManifestTest)
#include "tst_manifest.moc"
