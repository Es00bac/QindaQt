// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_surface_qml_test_support.h"

#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

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
    void aDragTracksThePointerExactly();
    void iconCanBeDraggedAndItsPositionIsStored();
    void marqueeSelectsCrossedIconsAndAnEmptyClickClears();
    void ctrlClickTogglesWhileShiftClickRanges();
    void groupDragMovesAndPersistsTheWholeSelection();
    void deleteMovesEverySelectedIconToTrash();
    void cutCopyAndPasteRideTheComposedClipboard();
    void iconSizeIsClampedToTheSupportedRange();

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

// Former-red regression for the user-reported "desktop icons do not move
// smoothly". The tile's MouseArea is anchored to the tile, so measuring the
// drag delta in tile coordinates moved the measuring frame with the icon and
// every event after the first reported (delta_n - delta_n-1) instead of
// delta_n: the icon lagged, jumped backwards and fought the pointer. The drag
// must now follow the pointer exactly, including a move back toward the press
// point.
void DesktopIconInteractionTests::aDragTracksThePointerExactly()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Track me.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QQuickItem *tile = host.visualItemsNamed(QStringLiteral("desktopIconsTile")).constFirst();
    QTRY_VERIFY(tile->x() >= 6.0 && tile->y() >= 6.0);

    const QPointF origin(tile->x(), tile->y());
    const QPointF press = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    QTest::mousePress(host.window.get(), Qt::LeftButton, Qt::NoModifier, press.toPoint());

    // Each step asserts the ABSOLUTE offset from the press point, so an error
    // that accumulates or alternates is caught on the very next event.
    const QList<QPoint> offsets{{30, 20}, {90, 60}, {45, 25}, {140, 95}};
    for (const QPoint &offset : offsets) {
        QTest::mouseMove(host.window.get(), (press + offset).toPoint(), 10);
        QCOMPARE(tile->x(), origin.x() + offset.x());
        QCOMPARE(tile->y(), origin.y() + offset.y());
    }
    QTest::mouseRelease(host.window.get(), Qt::LeftButton, Qt::NoModifier,
                        (press + offsets.constLast()).toPoint());
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

    // The drop snaps to the nearest free grid cell and GLIDES there, so the
    // resting position is only truthful once that animation has settled.
    const QString layoutKey = tile->property("layoutKey").toString();
    QVariantMap stored;
    QVERIFY(host.layoutStore != nullptr);
    QTRY_VERIFY(!(stored = host.layoutStore->position(layoutKey)).isEmpty());
    QTRY_COMPARE(tile->x(), stored.value(QStringLiteral("x")).toReal());
    QTRY_COMPARE(tile->y(), stored.value(QStringLiteral("y")).toReal());
}

namespace {

// Press-drag-release along a path of scene points (the first entry is the
// press origin, the last the release point).
void dragWindow(QQuickWindow *window, const QList<QPointF> &path,
                Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QVERIFY(!path.isEmpty());
    QTest::mousePress(window, Qt::LeftButton, modifiers, path.first().toPoint());
    for (int i = 1; i < path.size(); ++i) {
        QTest::mouseMove(window, path.at(i).toPoint());
    }
    QTest::mouseRelease(window, Qt::LeftButton, modifiers, path.last().toPoint());
}

// Tiles restore their stored/fallback geometry via a deferred call after
// the window appears; wait until the three fixture icons stack in listing
// order before deriving click coordinates from their positions.
#define QTRY_TILES_LAID_OUT(tiles)                                              \
    QTRY_VERIFY2(                                                               \
        (tiles).size() >= 3                                                     \
            && tileNamed((tiles), QStringLiteral("Alpha.txt"))->y()             \
                   < tileNamed((tiles), QStringLiteral("Beta.txt"))->y()        \
            && tileNamed((tiles), QStringLiteral("Beta.txt"))->y()              \
                   < tileNamed((tiles), QStringLiteral("Gamma.txt"))->y(),      \
        "tiles did not settle into their laid-out positions")

QQuickItem *tileNamed(const QList<QQuickItem *> &tiles, const QString &label)
{
    for (QQuickItem *tile : tiles) {
        if (tile->property("entryLabel").toString() == label) {
            return tile;
        }
    }
    return nullptr;
}

int selectedTileCount(const QList<QQuickItem *> &tiles)
{
    int count = 0;
    for (const QQuickItem *tile : tiles) {
        if (tile->property("selected").toBool()) {
            ++count;
        }
    }
    return count;
}

void triggerMenuItem(const SurfaceHost &host, const QString &objectName)
{
    const auto items = host.visualItemsNamed(objectName);
    QCOMPARE(items.size(), 1);
    QVERIFY(QMetaObject::invokeMethod(items.constFirst(), "triggered"));
}

} // namespace

void DesktopIconInteractionTests::marqueeSelectsCrossedIconsAndAnEmptyClickClears()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Beta.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 3);

    // A band from empty lower-left space up across the whole icon column
    // selects every crossed icon.
    dragWindow(host.window.get(),
               {QPointF(30, 420), QPointF(200, 300), QPointF(450, 150),
                QPointF(700, 20)});
    QCOMPARE(selectedTileCount(tiles), 3);
    auto *view = host.child<QQuickItem>(QStringLiteral("desktopIconsView"));
    QCOMPARE(view->property("selectedIds").toMap().size(), 3);

    // A plain click on empty space (press-release without a drag) clears it.
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, QPointF(30, 420));
    QCOMPARE(selectedTileCount(tiles), 0);
    QVERIFY(view->property("selectedIds").toMap().isEmpty());
}

void DesktopIconInteractionTests::ctrlClickTogglesWhileShiftClickRanges()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Beta.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 3);

    QQuickItem *alphaTile = tileNamed(tiles, QStringLiteral("Alpha.txt"));
    QQuickItem *betaTile = tileNamed(tiles, QStringLiteral("Beta.txt"));
    QQuickItem *gammaTile = tileNamed(tiles, QStringLiteral("Gamma.txt"));
    QVERIFY(alphaTile != nullptr && betaTile != nullptr && gammaTile != nullptr);
    QTRY_TILES_LAID_OUT(tiles);
    const QPointF alphaCenter =
        alphaTile->mapToScene(QPointF(alphaTile->width() / 2, alphaTile->height() / 2));
    const QPointF betaCenter =
        betaTile->mapToScene(QPointF(betaTile->width() / 2, betaTile->height() / 2));
    const QPointF gammaCenter =
        gammaTile->mapToScene(QPointF(gammaTile->width() / 2, gammaTile->height() / 2));

    // Shift+click ranges from the anchor set by the last plain click; the
    // in-between icon joins even though it was never clicked directly.
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, alphaCenter);
    QCOMPARE(selectedTileCount(tiles), 1);
    host.clickWindow(Qt::LeftButton, Qt::ShiftModifier, gammaCenter);
    QCOMPARE(selectedTileCount(tiles), 3);
    QVERIFY(alphaTile->property("selected").toBool());
    QVERIFY(betaTile->property("selected").toBool());
    QVERIFY(gammaTile->property("selected").toBool());

    // A plain click collapses the range to one icon.
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, betaCenter);
    QCOMPARE(selectedTileCount(tiles), 1);
    QVERIFY(betaTile->property("selected").toBool());

    // Ctrl+click toggles individual icons on and off.
    host.clickWindow(Qt::LeftButton, Qt::ControlModifier, gammaCenter);
    QCOMPARE(selectedTileCount(tiles), 2);
    QVERIFY(betaTile->property("selected").toBool());
    QVERIFY(gammaTile->property("selected").toBool());
    host.clickWindow(Qt::LeftButton, Qt::ControlModifier, gammaCenter);
    QCOMPARE(selectedTileCount(tiles), 1);
    QVERIFY(betaTile->property("selected").toBool());
}

void DesktopIconInteractionTests::groupDragMovesAndPersistsTheWholeSelection()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Beta.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 3);

    QQuickItem *alphaTile = tileNamed(tiles, QStringLiteral("Alpha.txt"));
    QQuickItem *betaTile = tileNamed(tiles, QStringLiteral("Beta.txt"));
    QQuickItem *gammaTile = tileNamed(tiles, QStringLiteral("Gamma.txt"));
    QVERIFY(alphaTile != nullptr && betaTile != nullptr && gammaTile != nullptr);
    QTRY_TILES_LAID_OUT(tiles);
    const QPointF alphaCenter =
        alphaTile->mapToScene(QPointF(alphaTile->width() / 2, alphaTile->height() / 2));
    const QPointF gammaCenter =
        gammaTile->mapToScene(QPointF(gammaTile->width() / 2, gammaTile->height() / 2));
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, alphaCenter);
    host.clickWindow(Qt::LeftButton, Qt::ControlModifier, gammaCenter);
    QCOMPARE(selectedTileCount(tiles), 2);

    const QPointF alphaStart(alphaTile->x(), alphaTile->y());
    const QPointF gammaStart(gammaTile->x(), gammaTile->y());
    const QPointF betaStart(betaTile->x(), betaTile->y());
    // Press on the already-selected Alpha keeps the two-icon selection and
    // drags both together by the same delta.
    dragWindow(host.window.get(),
               {alphaCenter, alphaCenter + QPointF(40, 30),
                alphaCenter + QPointF(120, 70)});
    QVERIFY(alphaTile->x() > alphaStart.x());
    QVERIFY(gammaTile->x() > gammaStart.x());
    // Both selected icons carry the SAME translation. The group is clamped as
    // one rigid set, so reaching a desktop edge can never collapse it into a
    // pile; each then settles onto its own grid cell, which preserves the
    // offsets because the cells are the same pitch.
    QTRY_COMPARE(alphaTile->x() - alphaStart.x(), gammaTile->x() - gammaStart.x());
    QTRY_COMPARE(alphaTile->y() - alphaStart.y(), gammaTile->y() - gammaStart.y());
    // The unselected icon stays put.
    QCOMPARE(betaTile->x(), betaStart.x());
    QCOMPARE(betaTile->y(), betaStart.y());

    QVERIFY(host.layoutStore != nullptr);
    for (const QQuickItem *tile : {alphaTile, gammaTile}) {
        const QString layoutKey = tile->property("layoutKey").toString();
        const QVariantMap stored = host.layoutStore->position(layoutKey);
        QVERIFY(!stored.isEmpty());
        QTRY_COMPARE(tile->x(), stored.value(QStringLiteral("x")).toReal());
        QTRY_COMPARE(tile->y(), stored.value(QStringLiteral("y")).toReal());
    }
}

void DesktopIconInteractionTests::deleteMovesEverySelectedIconToTrash()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Beta.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    const auto tiles = host.visualItemsNamed(QStringLiteral("desktopIconsTile"));
    QCOMPARE(tiles.size(), 3);

    QQuickItem *alphaTile = tileNamed(tiles, QStringLiteral("Alpha.txt"));
    QQuickItem *gammaTile = tileNamed(tiles, QStringLiteral("Gamma.txt"));
    QVERIFY(alphaTile != nullptr && gammaTile != nullptr);
    QTRY_TILES_LAID_OUT(tiles);
    const QPointF alphaCenter =
        alphaTile->mapToScene(QPointF(alphaTile->width() / 2, alphaTile->height() / 2));
    const QPointF gammaCenter =
        gammaTile->mapToScene(QPointF(gammaTile->width() / 2, gammaTile->height() / 2));
    host.clickWindow(Qt::LeftButton, Qt::NoModifier, alphaCenter);
    host.clickWindow(Qt::LeftButton, Qt::ControlModifier, gammaCenter);
    QCOMPARE(selectedTileCount(tiles), 2);

    // Right-click on a selected icon opens the icon menu; Delete applies to
    // the whole selection.
    host.clickWindow(Qt::RightButton, Qt::NoModifier, gammaCenter);
    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    QTRY_VERIFY(menu->property("opened").toBool());
    triggerMenuItem(host, QStringLiteral("desktopIconContextDelete"));

    const QDir trashFiles(m_home->path() + QStringLiteral("/.local/share/Trash/files"));
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("Alpha.txt")), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(trashFiles.exists(QStringLiteral("Gamma.txt")), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        host.visualItemsNamed(QStringLiteral("desktopIconsTile")).size() == 1, 5000);
    QVERIFY(!QFileInfo::exists(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(!QFileInfo::exists(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
}

void DesktopIconInteractionTests::cutCopyAndPasteRideTheComposedClipboard()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher, {}, &error), qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QQuickItem *tile = host.visualItemsNamed(QStringLiteral("desktopIconsTile")).constFirst();
    const QPointF center = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
    auto *contents = host.child<QObject>(QStringLiteral("desktopContentsController"));
    QVERIFY(contents != nullptr);
    auto *view = host.child<QQuickItem>(QStringLiteral("desktopIconsView"));
    QVERIFY(view != nullptr);

    host.clickWindow(Qt::RightButton, Qt::NoModifier, center);
    auto *menu = host.child<QObject>(QStringLiteral("desktopIconContextMenu"));
    QTRY_VERIFY(menu->property("opened").toBool());
    triggerMenuItem(host, QStringLiteral("desktopIconContextCut"));
    QCOMPARE(contents->property("clipboardMode").toString(), QStringLiteral("cut"));
    QVERIFY(view->property("canPaste").toBool());

    // Paste from the empty-area desktop menu: the target is the Desktop
    // itself, so the move refuses as an already-exists no-op with feedback
    // instead of removing anything.
    host.clickWindow(Qt::RightButton, Qt::NoModifier, QPointF(30, 420));
    auto *desktopMenu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QTRY_VERIFY(desktopMenu->property("opened").toBool());
    const auto pasteItems = host.visualItemsNamed(QStringLiteral("desktopContextPaste"));
    QCOMPARE(pasteItems.size(), 1);
    QVERIFY(pasteItems.constFirst()->isEnabled());
    triggerMenuItem(host, QStringLiteral("desktopContextPaste"));
    QTRY_VERIFY_WITH_TIMEOUT(
        contents->property("feedback").toString().contains(QStringLiteral("Nothing to paste")),
        5000);
    QVERIFY(QFileInfo::exists(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
}

void DesktopIconInteractionTests::iconSizeIsClampedToTheSupportedRange()
{
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(nullptr, &launcher,
                         {{QStringLiteral("iconSize"), 8}}, &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());
    QQuickItem *icon =
        host.visualItemsNamed(QStringLiteral("desktopIconsTileIcon")).constFirst();
    QCOMPARE(icon->property("size").toInt(), 16);

    // The setting is live: growing past the supported ceiling clamps to 128.
    QVERIFY(host.window->setProperty(
        "applets", makeApplets({{QStringLiteral("iconSize"), 500}})));
    QTRY_COMPARE(icon->property("size").toInt(), 128);
}

QTEST_MAIN(DesktopIconInteractionTests)
#include "tst_desktop_icon_interactions.moc"
