// SPDX-License-Identifier: GPL-3.0-or-later
//
// ADR-0282: the optional standard desktop icons in the compiled desktop
// surface, offscreen, over a redirected HOME and XDG_DATA_HOME.
#include "desktop_surface_qml_test_support.h"

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

namespace {

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

QQuickItem *tileLabelled(const QList<QQuickItem *> &tiles, const QString &label)
{
    for (QQuickItem *tile : tiles) {
        if (tile->property("entryLabel").toString() == label) {
            return tile;
        }
    }
    return nullptr;
}

QPointF centreOf(const QQuickItem *tile)
{
    return tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
}

int visibleItems(const SurfaceHost &host, const QString &objectName)
{
    int count = 0;
    for (QQuickItem *item : host.visualItemsNamed(objectName)) {
        count += item->isVisible() ? 1 : 0;
    }
    return count;
}

const QVariantMap HomeAndTrash{{QStringLiteral("showHomeIcon"), true},
                               {QStringLiteral("showTrashIcon"), true}};

} // namespace

class DesktopStandardIconsTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void standardIconsComeFirstWithTheirOwnNames();
    void eachStandardIconIsASetting();
    void aStandardIconOnlyOpens();
    void deleteNeverTouchesAStandardIcon();
    void droppingAnIconOnTheTrashMovesItToTheTrash();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
};

void DesktopStandardIconsTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME", (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Music")));
}

void DesktopStandardIconsTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
}

void DesktopStandardIconsTests::standardIconsComeFirstWithTheirOwnNames()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Notes.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, HomeAndTrash, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QQuickItem *home = tileLabelled(tiles, QStringLiteral("Home"));
    QQuickItem *trash = tileLabelled(tiles, QStringLiteral("Trash"));
    QQuickItem *notes = tileLabelled(tiles, QStringLiteral("Notes.txt"));
    QVERIFY(home != nullptr && trash != nullptr && notes != nullptr);
    // Flowed first, in the standard order, before the Desktop folder's files.
    QTRY_VERIFY(home->y() < trash->y() && trash->y() < notes->y());
    QCOMPARE(home->property("modelData").toMap().value(QStringLiteral("iconName")).toString(),
             QStringLiteral("user-home"));
    QCOMPARE(trash->property("modelData").toMap().value(QStringLiteral("iconName")).toString(),
             QStringLiteral("user-trash"));
    QCOMPARE(trash->property("modelData").toMap().value(QStringLiteral("accessibleName")).toString(),
             QStringLiteral("Trash, empty"));
}

void DesktopStandardIconsTests::eachStandardIconIsASetting()
{
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher,
                         {{QStringLiteral("showHomeIcon"), false},
                          {QStringLiteral("showTrashIcon"), false},
                          {QStringLiteral("showMusicIcon"), true},
                          {QStringLiteral("showVideosIcon"), true}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    // Videos is switched on but has no folder, so it shows no icon.
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 1);
    QVERIFY(tileLabelled(host.visualItemsNamed(QStringLiteral("desktopIconsTile")),
                         QStringLiteral("Music"))
            != nullptr);
}

void DesktopStandardIconsTests::aStandardIconOnlyOpens()
{
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, HomeAndTrash, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 2);
    QQuickItem *home =
        tileLabelled(host.visualItemsNamed(QStringLiteral("desktopIconsTile")), QStringLiteral("Home"));
    QVERIFY(home != nullptr);
    host.clickWindow(Qt::RightButton, Qt::NoModifier, centreOf(home));
    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->property("opened").toBool());
    QCOMPARE(visibleItems(host, QStringLiteral("desktopIconContextOpen")), 1);
    QCOMPARE(visibleItems(host, QStringLiteral("desktopIconContextRename")), 0);
    QCOMPARE(visibleItems(host, QStringLiteral("desktopIconContextTrash")), 0);
    QCOMPARE(visibleItems(host, QStringLiteral("desktopIconContextCut")), 0);
}

void DesktopStandardIconsTests::deleteNeverTouchesAStandardIcon()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Old.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, HomeAndTrash, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    auto *view = host.child<QQuickItem>(QStringLiteral("desktopIconsView"));
    QVERIFY(view != nullptr);
    QVERIFY(QMetaObject::invokeMethod(view, "selectAll"));
    QVERIFY(QMetaObject::invokeMethod(view, "trashSelection"));
    const QDir trashFiles(m_home->path() + QStringLiteral("/.local/share/Trash/files"));
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("Old.txt")), 5000);
    QVERIFY(QFileInfo(m_home->path()).isDir());
    // Home and the (now full) Trash are still there.
    QTRY_COMPARE_WITH_TIMEOUT(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 2,
                              5000);
    QQuickItem *trash =
        tileLabelled(host.visualItemsNamed(QStringLiteral("desktopIconsTile")), QStringLiteral("Trash"));
    QVERIFY(trash != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(
        trash->property("modelData").toMap().value(QStringLiteral("iconName")).toString(),
        QStringLiteral("user-trash-full"), 5000);
}

void DesktopStandardIconsTests::droppingAnIconOnTheTrashMovesItToTheTrash()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Drop me.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, HomeAndTrash, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QTRY_COMPARE(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QQuickItem *file = tileLabelled(tiles, QStringLiteral("Drop me.txt"));
    QQuickItem *trash = tileLabelled(tiles, QStringLiteral("Trash"));
    QVERIFY(file != nullptr && trash != nullptr);
    QTRY_VERIFY(trash->y() < file->y());
    const QPointF from = centreOf(file);
    const QPointF to = centreOf(trash);
    QTest::mousePress(host.window.get(), Qt::LeftButton, Qt::NoModifier, from.toPoint());
    QTest::mouseMove(host.window.get(), (from + QPointF(0, -20)).toPoint(), 10);
    QTest::mouseMove(host.window.get(), to.toPoint(), 10);
    QTest::mouseRelease(host.window.get(), Qt::LeftButton, Qt::NoModifier, to.toPoint());
    const QDir trashFiles(m_home->path() + QStringLiteral("/.local/share/Trash/files"));
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("Drop me.txt")), 5000);
    QVERIFY(!QFileInfo::exists(m_home->path() + QStringLiteral("/Desktop/Drop me.txt")));
}

QTEST_MAIN(DesktopStandardIconsTests)
#include "tst_desktop_standard_icons.moc"
