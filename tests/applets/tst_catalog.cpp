// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/applets/manifest_loader.h"

#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace QindaQt::Applets;

namespace {

QString firstPartyDirectory()
{
    return QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets");
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

} // namespace

class ManifestCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsRepresentativeFirstPartySet();
    void exposesCapabilityDeclarations();
    void preservesPreviousCatalogOnDuplicateFailure();
    void rejectsEmptyDirectory();
};

void ManifestCatalogTest::loadsRepresentativeFirstPartySet()
{
    ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(firstPartyDirectory(), &error), qPrintable(error));
    QCOMPARE(catalog.manifests().size(), 7);

    const QSet<QString> expected{
        QStringLiteral("launcher"),
        QStringLiteral("task-list"),
        QStringLiteral("global-menu"),
        QStringLiteral("system-tray"),
        QStringLiteral("clock"),
        QStringLiteral("notification-center"),
        QStringLiteral("clipboard"),
    };
    QSet<QString> actual;
    for (const AppletManifest &manifest : catalog.manifests()) {
        actual.insert(manifest.id);
        QVERIFY(manifest.supportsHost(ApiVersion::current()));
    }
    QCOMPARE(actual, expected);
    QVERIFY(catalog.findById(QStringLiteral("clock")) != nullptr);
    QVERIFY(catalog.findById(QStringLiteral("missing")) == nullptr);
}

void ManifestCatalogTest::exposesCapabilityDeclarations()
{
    ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(firstPartyDirectory(), &error), qPrintable(error));

    const AppletManifest *clock = catalog.findById(QStringLiteral("clock"));
    const AppletManifest *taskList = catalog.findById(QStringLiteral("task-list"));
    const AppletManifest *tray = catalog.findById(QStringLiteral("system-tray"));
    const AppletManifest *notificationCenter =
        catalog.findById(QStringLiteral("notification-center"));
    QVERIFY(clock != nullptr);
    QVERIFY(taskList != nullptr);
    QVERIFY(tray != nullptr);
    QVERIFY(notificationCenter != nullptr);
    QVERIFY(clock->capabilities.isEmpty());
    QVERIFY(taskList->capabilities.contains(Capability::WindowManage));
    QVERIFY(tray->capabilities.contains(Capability::StatusItemActivate));
    QVERIFY(notificationCenter->capabilities.isEmpty());
    QVERIFY(notificationCenter->placementZones
            == QVector<PlacementZone>({PlacementZone::PanelStart,
                                       PlacementZone::PanelCenter,
                                       PlacementZone::PanelEnd,
                                       PlacementZone::PanelFill}));
    QVERIFY(notificationCenter->orientations
            == QVector<Orientation>({Orientation::Horizontal,
                                     Orientation::Vertical}));
    QCOMPARE(notificationCenter->settingsSchema.value(QStringLiteral("type")).toString(),
             QStringLiteral("object"));
    QVERIFY(notificationCenter->settingsSchema.value(QStringLiteral("properties"))
                .toObject().isEmpty());
}

void ManifestCatalogTest::preservesPreviousCatalogOnDuplicateFailure()
{
    ManifestCatalog catalog;
    QString error;
    QVERIFY2(catalog.loadDirectory(firstPartyDirectory(), &error), qPrintable(error));
    const qsizetype originalSize = catalog.manifests().size();

    const AppletManifest *clock = catalog.findById(QStringLiteral("clock"));
    QVERIFY(clock != nullptr);
    const QByteArray serialized = ManifestLoader::toJson(*clock);
    QTemporaryDir duplicateDirectory;
    QVERIFY(duplicateDirectory.isValid());
    QVERIFY(writeFile(duplicateDirectory.filePath(QStringLiteral("one.json")), serialized));
    QVERIFY(writeFile(duplicateDirectory.filePath(QStringLiteral("two.json")), serialized));

    QVERIFY(!catalog.loadDirectory(duplicateDirectory.path(), &error));
    QVERIFY(error.contains(QStringLiteral("Duplicate")));
    QCOMPARE(catalog.manifests().size(), originalSize);
    QVERIFY(catalog.findById(QStringLiteral("clock")) != nullptr);
}

void ManifestCatalogTest::rejectsEmptyDirectory()
{
    QTemporaryDir emptyDirectory;
    QVERIFY(emptyDirectory.isValid());
    ManifestCatalog catalog;
    QString error;
    QVERIFY(!catalog.loadDirectory(emptyDirectory.path(), &error));
    QVERIFY(error.contains(QStringLiteral("empty")));
}

QTEST_GUILESS_MAIN(ManifestCatalogTest)
#include "tst_catalog.moc"
