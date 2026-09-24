// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0273: the desktop's menus are File Manager menus. Their entries speak
// the File Manager's words from its public catalog, and Get Info, Open With
// and New File hand the icon (or the Desktop folder) to File Manager through
// its public boundary. PATH holds only a recording stand-in for
// qindaqt-file-manager, so no real application starts.
#include "desktop_surface_qml_test_support.h"

#include "file_manager_menu_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlExtensionPlugin>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;
namespace MenuCatalog = QindaQt::Apps::FileManager::MenuCatalog;

namespace {

const QPointF kEmptySpot(700, 520);

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

QString catalogLabel(const char *actionId)
{
    const auto definition = MenuCatalog::findAction(QString::fromLatin1(actionId));
    return definition ? definition->label : QString();
}

// The one visible row named `objectName`, or nullptr.
QQuickItem *visibleRow(const SurfaceHost &host, const QString &objectName)
{
    for (QQuickItem *item : host.visualItemsNamed(objectName)) {
        if (item->isVisible()) {
            return item;
        }
    }
    return nullptr;
}

void triggerRow(const SurfaceHost &host, const QString &objectName)
{
    QQuickItem *row = visibleRow(host, objectName);
    QVERIFY2(row != nullptr, qPrintable(objectName));
    QVERIFY(row->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(row, "triggered"));
}

QQuickItem *tileNamed(const SurfaceHost &host, const QString &label)
{
    for (QQuickItem *tile : host.visualItemsNamed(QStringLiteral("desktopIconsTile"))) {
        if (tile->property("entryLabel").toString() == label) {
            return tile;
        }
    }
    return nullptr;
}

} // namespace

class DesktopFileManagerMenuTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void iconMenuSpeaksTheFileManagersWords();
    void getInfoAndOpenWithHandTheIconToFileManager();
    void backgroundMenuHandsNewFileAndGetInfoToFileManager();

private:
    void openIconMenu(SurfaceHost &host, const QString &label);
    void openDesktopMenu(SurfaceHost &host);
    [[nodiscard]] QByteArray launches() const;

    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
    QByteArray m_previousPath;
    QString m_desktop;
    QString m_record;
};

void DesktopFileManagerMenuTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    m_previousPath = qgetenv("PATH");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME", (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    m_desktop = m_home->path() + QStringLiteral("/Desktop");
    QVERIFY(QDir().mkpath(m_desktop));
    QVERIFY(writeFile(m_desktop + QStringLiteral("/Notes.txt")));
    QVERIFY(QDir().mkpath(m_desktop + QStringLiteral("/Projects")));

    // The only qindaqt-file-manager on PATH records its argv, one per line.
    const QString bin = m_home->filePath(QStringLiteral("bin"));
    m_record = m_home->filePath(QStringLiteral("argv"));
    QVERIFY(QDir().mkpath(bin));
    QFile program(bin + QStringLiteral("/qindaqt-file-manager"));
    QVERIFY(program.open(QIODevice::WriteOnly));
    program.write(
        QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" >> '%1'\n").arg(m_record).toLocal8Bit());
    program.close();
    QVERIFY(program.setPermissions(QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    qputenv("PATH", bin.toLocal8Bit());
}

void DesktopFileManagerMenuTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    qputenv("PATH", m_previousPath);
    m_home.reset();
}

void DesktopFileManagerMenuTests::openIconMenu(SurfaceHost &host, const QString &label)
{
    QQuickItem *tile = nullptr;
    QTRY_VERIFY((tile = tileNamed(host, label)) != nullptr);
    QTRY_VERIFY(tile->x() >= 6.0 && tile->y() >= 6.0);
    host.clickWindow(Qt::RightButton, Qt::NoModifier,
                     tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2)));
    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->property("opened").toBool());
}

void DesktopFileManagerMenuTests::openDesktopMenu(SurfaceHost &host)
{
    host.clickWindow(Qt::RightButton, Qt::NoModifier, kEmptySpot);
    auto *menu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->property("opened").toBool());
}

QByteArray DesktopFileManagerMenuTests::launches() const
{
    QFile file(m_record);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void DesktopFileManagerMenuTests::iconMenuSpeaksTheFileManagersWords()
{
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    openIconMenu(host, QStringLiteral("Notes.txt"));
    const struct {
        const char *objectName;
        const char *actionId;
    } rows[] = {
        {"desktopIconContextOpen", "file.open"},
        {"desktopIconContextOpenWith", "file.open-with"},
        {"desktopIconContextGetInfo", "file.properties"},
        {"desktopIconContextRename", "file.rename"},
        {"desktopIconContextCut", "edit.cut"},
        {"desktopIconContextCopy", "edit.copy"},
        {"desktopIconContextTrash", "file.trash"},
    };
    for (const auto &expected : rows) {
        QQuickItem *row = visibleRow(host, QLatin1String(expected.objectName));
        QVERIFY2(row != nullptr, expected.objectName);
        // The catalog knows every id the menu asks for, in the same words.
        QVERIFY2(!catalogLabel(expected.actionId).isEmpty(), expected.actionId);
        QCOMPARE(row->property("text").toString(), catalogLabel(expected.actionId));
    }
    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    menu->setProperty("visible", false);
    QTRY_VERIFY(!menu->property("opened").toBool());

    // A folder has no Open With; everything else stays.
    openIconMenu(host, QStringLiteral("Projects"));
    QVERIFY(visibleRow(host, QStringLiteral("desktopIconContextOpenWith")) == nullptr);
    QVERIFY(visibleRow(host, QStringLiteral("desktopIconContextGetInfo")) != nullptr);
    QVERIFY(visibleRow(host, QStringLiteral("desktopIconContextTrash")) != nullptr);
}

void DesktopFileManagerMenuTests::getInfoAndOpenWithHandTheIconToFileManager()
{
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const QByteArray folder = QFileInfo(m_desktop).canonicalFilePath().toLocal8Bit();

    openIconMenu(host, QStringLiteral("Notes.txt"));
    triggerRow(host, QStringLiteral("desktopIconContextGetInfo"));
    const QByteArray getInfo = "--select=Notes.txt\n--action=file.properties\n" + folder + '\n';
    QTRY_COMPARE(launches(), getInfo);

    openIconMenu(host, QStringLiteral("Notes.txt"));
    triggerRow(host, QStringLiteral("desktopIconContextOpenWith"));
    QTRY_COMPARE(launches(),
                 getInfo + "--select=Notes.txt\n--action=file.open-with\n" + folder + '\n');
    auto *contents = host.child<QObject>(QStringLiteral("desktopContentsController"));
    QVERIFY(contents != nullptr);
    QCOMPARE(contents->property("feedback").toString(), QString());
}

void DesktopFileManagerMenuTests::backgroundMenuHandsNewFileAndGetInfoToFileManager()
{
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    // Every style offers the File Manager's folder actions in its words.
    for (const char *style : {"windows", "mac", "traditional"}) {
        QVERIFY(host.window->setProperty(
            "applets",
            makeApplets({{QStringLiteral("contextMenuStyle"), QLatin1String(style)}})));
        openDesktopMenu(host);
        const struct {
            const char *objectName;
            const char *actionId;
        } rows[] = {
            {"desktopContextNewFolder", "file.new-folder"},
            {"desktopContextNewFile", "file.new-file"},
            {"desktopContextSelectAll", "edit.select-all"},
            {"desktopContextGetInfo", "file.properties"},
        };
        for (const auto &expected : rows) {
            QQuickItem *row = visibleRow(host, QLatin1String(expected.objectName));
            QVERIFY2(row != nullptr, expected.objectName);
            QVERIFY2(!catalogLabel(expected.actionId).isEmpty(), expected.actionId);
            QCOMPARE(row->property("text").toString(), catalogLabel(expected.actionId));
        }
        auto *menu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
        menu->setProperty("visible", false);
        QTRY_VERIFY(!menu->property("opened").toBool());
    }

    const QByteArray folder = QFileInfo(m_desktop).canonicalFilePath().toLocal8Bit();
    openDesktopMenu(host);
    triggerRow(host, QStringLiteral("desktopContextNewFile"));
    const QByteArray newFile = "--action=file.new-file\n" + folder + '\n';
    QTRY_COMPARE(launches(), newFile);
    openDesktopMenu(host);
    triggerRow(host, QStringLiteral("desktopContextGetInfo"));
    QTRY_COMPARE(launches(), newFile + "--action=file.properties\n" + folder + '\n');

    // Select All selects every icon, the File Manager's Edit ▸ Select All.
    openDesktopMenu(host);
    triggerRow(host, QStringLiteral("desktopContextSelectAll"));
    const auto selectedTiles = [&host] {
        int count = 0;
        for (QQuickItem *tile : host.visualItemsNamed(QStringLiteral("desktopIconsTile"))) {
            count += tile->property("selected").toBool() ? 1 : 0;
        }
        return count;
    };
    QTRY_COMPARE(selectedTiles(), 2);
}

QTEST_MAIN(DesktopFileManagerMenuTests)
#include "tst_desktop_file_manager_menus.moc"
