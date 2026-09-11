// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_surface_qml_test_support.h"

#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;

namespace {

// Empty area of the 800x600 surface, away from the icon column.
constexpr QPointF kEmptySpot(400, 300);

int visibleMenuItem(const SurfaceHost &host, const QString &objectName)
{
    const auto items = host.visualItemsNamed(objectName);
    for (QQuickItem *item : items) {
        if (item->isVisible()) {
            return 1;
        }
    }
    return 0;
}

} // namespace

class DesktopSurfaceQmlTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void placementSettingSwitchesAnchorEdge();
    void selectionAndDoubleClickOpenThroughPlacesSeam();
    void contextMenuStyleSwitchesItemSets();
    void modifierRightClickOpensApplicationsPopup();
    void nullFacadesDisableMenuEntries();
};

void DesktopSurfaceQmlTests::placementSettingSwitchesAnchorEdge()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("placement"), QStringLiteral("left")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    auto *view = host.child<QQuickItem>(QStringLiteral("desktopIconsView"));
    QVERIFY(view != nullptr);
    QCOMPARE(view->property("placementRight").toBool(), false);
    QCOMPARE(view->x(), 0.0);
    QTRY_VERIFY(host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size()
                == 3);

    QVERIFY(host.window->setProperty("applets",
                                     makeApplets({{QStringLiteral("placement"),
                                                   QStringLiteral("right")}})));
    QTRY_COMPARE(view->property("placementRight").toBool(), true);
    // The block now hugs the right edge: its right side sits one margin from
    // the output edge.
    QTRY_VERIFY(view->x() + view->width() + 6 > 790.0);
}

void DesktopSurfaceQmlTests::selectionAndDoubleClickOpenThroughPlacesSeam()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("placement"), QStringLiteral("left")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles =
        host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 3);
    QQuickItem *desktopTile = tiles.at(1);
    QCOMPARE(desktopTile->property("placeId").toString(),
             QStringLiteral("desktop"));

    // Single click selects exactly one tile.
    const QPointF center(desktopTile->width() / 2, desktopTile->height() / 2);
    const QPointF sceneCenter = desktopTile->mapToScene(center);
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, sceneCenter);
    QCOMPARE(desktopTile->property("selected").toBool(), true);
    QCOMPARE(host.child<QQuickItem>(QStringLiteral("desktopIconsView"))
                 ->property("selectedId")
                 .toString(),
             QStringLiteral("desktop"));

    // Double click opens through the places facade seam.
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, sceneCenter);
    QTest::mouseDClick(host.window.get(), Qt::LeftButton, Qt::NoModifier,
                       sceneCenter.toPoint());
    QTRY_COMPARE(places.opened.size(), 1);
    QCOMPARE(places.opened.constFirst(), QStringLiteral("desktop"));

    // An empty-area left click clears the selection.
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, kEmptySpot);
    QCOMPARE(desktopTile->property("selected").toBool(), false);
}

void DesktopSurfaceQmlTests::contextMenuStyleSwitchesItemSets()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("contextMenuStyle"),
                           QStringLiteral("windows")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    auto *menu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(menu != nullptr);
    QCOMPARE(menu->property("popupType").toInt(), 1); // QQuickPopup::Window

    const struct {
        const char *style;
        QList<QPair<QString, int>> expected;
    } cases[] = {
        {"windows",
         {{"desktopContextArrange", 1},
          {"desktopContextRefresh", 1},
          {"desktopContextNewFolder", 1},
          {"desktopContextDisplayProperties", 1},
          {"desktopContextApplications", 0},
          {"desktopContextOpen", 0},
          {"desktopContextTerminal", 0}}},
        {"mac",
         {{"desktopContextArrange", 0},
          {"desktopContextNewFolder", 1},
          {"desktopContextOpen", 1},
          {"desktopContextSortBy", 1},
          {"desktopContextCleanUp", 1},
          {"desktopContextScreenSaver", 1},
          {"desktopContextApplications", 0},
          {"desktopContextTerminal", 0}}},
        {"traditional",
         {{"desktopContextArrange", 0},
          {"desktopContextOpen", 0},
          {"desktopContextApplications", 1},
          {"desktopContextTerminal", 1},
          {"desktopContextCreateFolder", 1},
          {"desktopContextSettings", 1}}},
    };

    for (const auto &testCase : cases) {
        QVERIFY(host.window->setProperty(
            "applets", makeApplets({{QStringLiteral("contextMenuStyle"),
                                     QLatin1String(testCase.style)}})));
        host.clickWindow(Qt::RightButton, Qt::NoModifier, kEmptySpot);
        QTRY_VERIFY(menu->property("opened").toBool());
        for (const auto &expected : testCase.expected) {
            const int actual = visibleMenuItem(host, expected.first);
            if (actual != expected.second) {
                const auto found = host.visualItemsNamed(expected.first);
                qDebug() << "menu item mismatch in style" << testCase.style
                         << "item" << expected.first << "expected"
                         << expected.second << "actual" << actual
                         << "found" << found.size()
                         << (found.isEmpty()
                                 ? QVariant()
                                 : QVariant(found.constFirst()->isVisible()));
            }
            QCOMPARE(actual, expected.second);
        }
        menu->setProperty("visible", false);
        QTRY_VERIFY(!menu->property("opened").toBool());
    }
}

void DesktopSurfaceQmlTests::modifierRightClickOpensApplicationsPopup()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(
                 &access, &launcher,
                 {{QStringLiteral("applicationsMenuModifier"),
                   QStringLiteral("shift")}},
                 &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    // Plain right click opens the style menu (default windows style).
    host.clickWindow(Qt::RightButton, Qt::NoModifier, kEmptySpot);
    auto *contextMenu =
        host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(contextMenu != nullptr);
    QTRY_VERIFY(contextMenu->property("opened").toBool());
    contextMenu->setProperty("visible", false);
    QTRY_VERIFY(!contextMenu->property("opened").toBool());

    // Shift+right click opens the Applications popup regardless of style.
    host.clickWindow(Qt::RightButton, Qt::ShiftModifier, kEmptySpot);
    auto *applicationsMenu =
        host.child<QObject>(QStringLiteral("desktopApplicationsMenu"));
    QVERIFY(applicationsMenu != nullptr);
    QCOMPARE(applicationsMenu->property("popupType").toInt(), 1);
    QTRY_VERIFY(applicationsMenu->property("opened").toBool());
    QCOMPARE(contextMenu->property("opened").toBool(), false);

    const auto rows =
        host.visualItemsNamed(QStringLiteral("desktopApplicationsRow"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(QAccessible::queryAccessibleInterface(rows.constFirst())
                 ->text(QAccessible::Name),
             QStringLiteral("Terminal"));
    const QPointF rowCenter(rows.constFirst()->width() / 2,
                            rows.constFirst()->height() / 2);
    const QPointF rowScene = rows.constFirst()->mapToScene(rowCenter);
    QTest::mouseClick(
        rows.constFirst()->window(), Qt::LeftButton, Qt::NoModifier,
        rowScene.toPoint());
    QTRY_COMPARE(launcher.activated.size(), 1);
    QCOMPARE(launcher.activated.constFirst(),
             QStringLiteral("org.qindaqt.Terminal"));
    QTRY_VERIFY(!applicationsMenu->property("opened").toBool());
}

void DesktopSurfaceQmlTests::nullFacadesDisableMenuEntries()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, nullptr,
                         {{QStringLiteral("contextMenuStyle"),
                           QStringLiteral("traditional")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    host.clickWindow(Qt::RightButton, Qt::NoModifier, kEmptySpot);
    auto *menu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->property("opened").toBool());
    // Launcher-gated entries disable; the places icons still render.
    QVERIFY(!host.visualItemsNamed(QStringLiteral("desktopContextTerminal"))
                 .constFirst()
                 ->isEnabled());
    QVERIFY(!host.visualItemsNamed(QStringLiteral("desktopContextSettings"))
                 .constFirst()
                 ->isEnabled());
    menu->setProperty("visible", false);
    QTRY_VERIFY(!menu->property("opened").toBool());

    // No crash with no facade at all, and no tiles without places.
    SurfaceHost bareHost;
    QVERIFY2(bareHost.create(nullptr, nullptr, {}, &error),
             qPrintable(error));
    QTRY_VERIFY(bareHost.window->isExposed());
    QCOMPARE(bareHost.visualItemsNamed(QStringLiteral("desktopIconsTile"))
                 .size(),
             0);
    bareHost.clickWindow(Qt::RightButton, Qt::NoModifier, kEmptySpot);
    auto *bareMenu =
        bareHost.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(bareMenu != nullptr);
    QTRY_VERIFY(bareMenu->property("opened").toBool());
    bareMenu->setProperty("visible", false);
}

QTEST_MAIN(DesktopSurfaceQmlTests)
#include "tst_desktop_surface_qml.moc"
