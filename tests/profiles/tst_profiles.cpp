// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/profiles/profile_catalog.h"
#include "qindaqt/profiles/profile_loader.h"

#include <QSet>
#include <QtTest>

using namespace QindaQt::Profiles;

class ProfileTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsEveryBuiltInProfile();
    void rejectsInvalidPanelGeometry();
    void catalogSelectsByStableId();
    void macosProfileUsesQindaMacosTheme();
};

void ProfileTests::loadsEveryBuiltInProfile()
{
    const QString directory = QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles");
    const auto results = ProfileLoader::fromDirectory(directory);
    QVERIFY2(results.size() >= 9, "The built-in profile set unexpectedly shrank");

    QSet<QString> ids;
    for (const auto &result : results) {
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.profile.panels.isEmpty());
        QVERIFY(!ids.contains(result.profile.id));
        ids.insert(result.profile.id);
    }
}

void ProfileTests::rejectsInvalidPanelGeometry()
{
    constexpr auto invalid = R"json({
        "schemaVersion": 1,
        "id": "bad",
        "name": "Bad",
        "panels": [{
            "id": "oversized",
            "edge": "top",
            "rows": 99,
            "thickness": 12,
            "length": 2.0,
            "applets": []
        }]
    })json";
    const auto result = ProfileLoader::fromJson(invalid, QStringLiteral("fixture"));
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("invalid rows")));
}

void ProfileTests::catalogSelectsByStableId()
{
    ProfileCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));
    QVERIFY(catalog.selectById(QStringLiteral("unity-inspired")));
    QCOMPARE(catalog.current().value(QStringLiteral("id")).toString(), QStringLiteral("unity-inspired"));
    QVERIFY(!catalog.selectById(QStringLiteral("missing-profile")));
}

void ProfileTests::macosProfileUsesQindaMacosTheme()
{
    const auto result = ProfileLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles/macos-inspired.json"));
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.profile.defaultTheme, QStringLiteral("qinda-macos"));
}

QTEST_GUILESS_MAIN(ProfileTests)
#include "tst_profiles.moc"
