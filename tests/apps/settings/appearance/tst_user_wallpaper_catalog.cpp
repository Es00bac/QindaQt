// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/user_wallpaper_catalog.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using QindaQt::Apps::SettingsAppearance::UserWallpaperCatalog;
class UserWallpaperCatalogTests final : public QObject {
    Q_OBJECT
private slots:
    void copyPreservesSourceAndNeverOverwrites();
    void folderPersistsAndExternalChangesRefresh();
    void invalidInputsLeaveGalleryIntact();
};
void UserWallpaperCatalogTests::copyPreservesSourceAndNeverOverwrites()
{
    QTemporaryDir temp;
    const QString source = temp.filePath(QStringLiteral("wall # café.png"));
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(source));
    UserWallpaperCatalog catalog(nullptr, temp.filePath("prefs.ini"), temp.filePath("gallery"));
    const QString imported = catalog.importImage(QUrl::fromLocalFile(source));
    QVERIFY(!imported.isEmpty());
    QVERIFY(QFileInfo::exists(source));
    QCOMPARE(catalog.wallpapers().size(), 1);
    QCOMPARE(QImage(imported), image);
    QCOMPARE(catalog.importImage(QUrl::fromLocalFile(imported)), imported);
    QCOMPARE(catalog.wallpapers().size(), 1);
    const QString second = catalog.importImage(QUrl::fromLocalFile(source));
    QVERIFY(second != imported);
    QCOMPARE(QImage(imported), image);
    QCOMPARE(catalog.wallpapers().size(), 2);
}
void UserWallpaperCatalogTests::folderPersistsAndExternalChangesRefresh()
{
    QTemporaryDir temp;
    QVERIFY(QDir().mkpath(temp.filePath("chosen")));
    UserWallpaperCatalog catalog(nullptr, temp.filePath("prefs.ini"), temp.filePath("default"));
    QVERIFY(catalog.setFolder(QUrl::fromLocalFile(temp.filePath("chosen"))));
    UserWallpaperCatalog reopened(nullptr, temp.filePath("prefs.ini"), temp.filePath("default"));
    QCOMPARE(reopened.folder(), temp.filePath("chosen"));
    QImage image(4, 4, QImage::Format_RGB32);
    image.fill(Qt::blue);
    const QString external = temp.filePath("chosen/external.png");
    QVERIFY(image.save(external));
    QTRY_COMPARE(catalog.wallpapers().size(), 1);
    QVERIFY(QFile::remove(external));
    QTRY_COMPARE(catalog.wallpapers().size(), 0);
}
void UserWallpaperCatalogTests::invalidInputsLeaveGalleryIntact()
{
    QTemporaryDir temp;
    const QString prefs = temp.filePath("prefs.ini");
    UserWallpaperCatalog catalog(nullptr, prefs, temp.filePath("gallery"));
    QVERIFY(catalog.importImage(QUrl("https://example.org/image.png")).isEmpty());
    QFile invalid(temp.filePath("invalid.png"));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write("not an image"); invalid.close();
    QVERIFY(catalog.importImage(QUrl::fromLocalFile(invalid.fileName())).isEmpty());
    QVERIFY(!catalog.setFolder(QUrl::fromLocalFile(invalid.fileName())));
    QCOMPARE(catalog.folder(), temp.filePath("gallery"));
    QVERIFY(!catalog.error().isEmpty());
    QCOMPARE(catalog.wallpapers().size(), 0);
    QImage image(4, 4, QImage::Format_RGB32); image.fill(Qt::red);
    const QString source = temp.filePath("source.png");
    QVERIFY(image.save(source));
    // A regular file at the destination directory makes copy impossible,
    // independent of the uid running the test (including root).
    UserWallpaperCatalog blocked(nullptr, prefs, invalid.fileName());
    QVERIFY(blocked.importImage(QUrl::fromLocalFile(source)).isEmpty());
    QVERIFY(QFileInfo::exists(source));
    QVERIFY(!blocked.error().isEmpty());
}
QTEST_GUILESS_MAIN(UserWallpaperCatalogTests)
#include "tst_user_wallpaper_catalog.moc"
