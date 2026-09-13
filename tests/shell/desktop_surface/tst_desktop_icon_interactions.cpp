// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_surface_qml_test_support.h"

#include <QDir>
#include <QFile>
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

int visibleMenuItem(const SurfaceHost &host, const QString &objectName)
{
    for (QQuickItem *item : host.visualItemsNamed(objectName)) {
        if (item->isVisible()) {
            return 1;
        }
    }
    return 0;
}
} // namespace

class DesktopIconInteractionTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void contextMenuOpensAtThePointerAndOffersRename();
    void iconCanBeDraggedAndItsPositionIsStored();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
};

void DesktopIconInteractionTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME",
            (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
}

void DesktopIconInteractionTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
}

void DesktopIconInteractionTests::contextMenuOpensAtThePointerAndOffersRename()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Notes.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 1);
    QQuickItem *tile = tiles.constFirst();
    const QPointF scene = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    host.clickWindow(Qt::RightButton, Qt::NoModifier, scene);

    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    auto *anchor = host.child<QQuickItem>(QStringLiteral("desktopIconContextMenuAnchor"));
    QVERIFY(menu != nullptr);
    QVERIFY(anchor != nullptr);
    QTRY_VERIFY(menu->property("opened").toBool());
    QCOMPARE(anchor->x() + anchor->width(), scene.x());
    QCOMPARE(anchor->y(), scene.y());
    QCOMPARE(visibleMenuItem(host, QStringLiteral("desktopIconContextOpen")), 1);
    QCOMPARE(visibleMenuItem(host, QStringLiteral("desktopIconContextRename")), 1);
    QCOMPARE(host.child<QObject>(QStringLiteral("desktopContextMenu"))->property("opened").toBool(),
             false);
}

void DesktopIconInteractionTests::iconCanBeDraggedAndItsPositionIsStored()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Move me.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QQuickItem *tile = host.visualItemsNamed(QStringLiteral("desktopIconsTile")).constFirst();
    QTRY_VERIFY(tile->x() >= 6.0 && tile->y() >= 6.0);
    const QPointF start = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    QTest::mousePress(host.window.get(), Qt::LeftButton, Qt::NoModifier, start.toPoint());
    QTest::mouseMove(host.window.get(), (start + QPointF(20, 20)).toPoint(), 20);
    QTest::mouseMove(host.window.get(), (start + QPointF(90, 60)).toPoint(), 20);
    QTest::mouseMove(host.window.get(), (start + QPointF(180, 120)).toPoint(), 20);
    QTest::mouseRelease(host.window.get(), Qt::LeftButton, Qt::NoModifier,
                        (start + QPointF(180, 120)).toPoint());
    QTRY_VERIFY(tile->x() > 100.0);

    auto *store = host.child<QObject>(QStringLiteral("desktopIconLayoutStore"));
    QVariantMap stored;
    QVERIFY(store != nullptr);
    QVERIFY(QMetaObject::invokeMethod(store, "position", Q_RETURN_ARG(QVariantMap, stored),
                                      Q_ARG(QString, QStringLiteral("OFFSCREEN0")),
                                      Q_ARG(QString, tile->property("layoutKey").toString())));
    QCOMPARE(stored.value(QStringLiteral("x")).toReal(), tile->x());
    QCOMPARE(stored.value(QStringLiteral("y")).toReal(), tile->y());
}

QTEST_MAIN(DesktopIconInteractionTests)
#include "tst_desktop_icon_interactions.moc"
