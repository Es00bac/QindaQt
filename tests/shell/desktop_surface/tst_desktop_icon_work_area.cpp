// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0261: desktop icons live inside each output's WORK AREA - the output
// minus the exclusive zones the shell's own panels reserve - while the
// desktop surface itself keeps spanning the whole output. The reported defect
// was the first row of icons sitting under the top bar, because the flow used
// the full output rectangle with a 6 px margin.
//
// The rows host the real compiled surface offscreen and feed it the same
// `outputRects` shape DesktopSurfaceController publishes (its own row pins
// that shape). Geometry: 800x600 outputs, 48 px icons, so a tile is 104x88 on
// a 108x92 grid with a 6 px margin.

#include "desktop_surface_qml_test_support.h"

#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QMargins>
#include <QPainter>
#include <QQmlExtensionPlugin>
#include <QRect>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;
using QindaQt::Shell::DesktopSurface::DesktopIconLayoutStore;

namespace {

constexpr int kMargin = 6;
constexpr int kTileWidth = 104;
constexpr int kTileHeight = 88;
constexpr int kColumn = 108;
constexpr int kRow = 92;
const QRect kOutput(0, 0, 800, 600);

QVariantMap rectMap(const QRect &rect)
{
    return QVariantMap{{QStringLiteral("x"), rect.x()},
                       {QStringLiteral("y"), rect.y()},
                       {QStringLiteral("width"), rect.width()},
                       {QStringLiteral("height"), rect.height()}};
}

// One output entry as DesktopSurfaceController publishes it.
QVariantMap output(const QString &name, const QRect &geometry,
                   const QMargins &reserved = {})
{
    QVariantMap entry = rectMap(geometry);
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("workArea"), rectMap(geometry.marginsRemoved(reserved)));
    return entry;
}

QVariantList onlyOutput(const QMargins &reserved)
{
    return {output(QStringLiteral("OFFSCREEN0"), kOutput, reserved)};
}

// The tile drawing listing entry `index` (the Repeater index, which is what
// the default flow is keyed on).
QQuickItem *tileAt(const SurfaceHost &host, int index)
{
    for (QQuickItem *tile : host.visualItemsNamed(QStringLiteral("desktopIconsTile"))) {
        if (tile->property("index").toInt() == index) {
            return tile;
        }
    }
    return nullptr;
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

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

// Pixels differing from an empty corner of the surface, inside `band`.
int paintedPixels(const QImage &image, const QRect &band)
{
    const QRgb empty = image.pixel(image.width() - 1, image.height() - 1);
    int painted = 0;
    const QRect clipped = band.intersected(image.rect());
    for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
        for (int x = clipped.left(); x <= clipped.right(); ++x) {
            painted += image.pixel(x, y) != empty ? 1 : 0;
        }
    }
    return painted;
}

// Review artifact only: set QINDAQT_DESKTOP_WORK_AREA_CAPTURE to a .png path
// to keep the probe's frame, drawn over a dark desktop with the reserved band
// tinted. It never affects the verdict.
void keepCapture(const QImage &image, const QRect &reserved)
{
    const QString path = qEnvironmentVariable("QINDAQT_DESKTOP_WORK_AREA_CAPTURE");
    if (path.isEmpty()) {
        return;
    }
    QImage frame(image.size(), QImage::Format_ARGB32_Premultiplied);
    frame.fill(QColor(0x20, 0x26, 0x30));
    QPainter painter(&frame);
    painter.fillRect(reserved, QColor(0xd0, 0x40, 0x40, 0x90));
    painter.drawImage(0, 0, image);
    painter.end();
    QVERIFY2(frame.save(path), qPrintable(path));
}

} // namespace

class DesktopIconWorkAreaTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void firstIconStartsBelowTheTopBar();
    void aBottomDockShortensEveryColumn();
    void sidePanelsPushTheFlowInward();
    void eachOutputKeepsItsOwnPanels();
    void anOutputWithoutReservationsIsAllWorkArea();
    void aStoredPositionUnderTheBarIsClampedWithoutRewritingTheStore();
    void aReservationChangeReflowsLive();
    void aDragCannotParkAnIconUnderThePanel();

private:
    bool createHost(SurfaceHost *host, const QVariantList &outputs,
                    const QVariantMap &settings = {});

    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
    StubLauncher m_launcher;
};

void DesktopIconWorkAreaTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME",
            (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
    // Seven icons: one more than a 600 px output fits in a column without a
    // dock, and two more than it fits with one.
    for (int i = 1; i <= 7; ++i) {
        QVERIFY(writeFile(m_home->path()
                          + QStringLiteral("/Desktop/Item %1.txt").arg(i)));
    }
}

void DesktopIconWorkAreaTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
}

bool DesktopIconWorkAreaTests::createHost(SurfaceHost *host, const QVariantList &outputs,
                                          const QVariantMap &settings)
{
    QString error;
    if (!host->create(nullptr, &m_launcher, settings, &error, outputs)) {
        qWarning().noquote() << error;
        return false;
    }
    return QTest::qWaitForWindowExposed(host->window.get());
}

// The reported defect, as a probe: the first icon's top is below the top
// bar's reservation, and nothing at all is painted inside the reserved band.
void DesktopIconWorkAreaTests::firstIconStartsBelowTheTopBar()
{
    const QMargins topBar(0, 26, 0, 0);
    SurfaceHost host;
    QVERIFY(createHost(&host, onlyOutput(topBar)));
    QQuickItem *first = tileAt(host, 0);
    QVERIFY(first != nullptr);
    QTRY_COMPARE(first->y(), qreal(26 + kMargin));
    QCOMPARE(first->x(), qreal(kMargin));
    QVERIFY(first->y() >= topBar.top());
    for (int index = 0; index < 7; ++index) {
        QVERIFY(tileAt(host, index)->y() >= topBar.top());
    }

    const QImage image = host.window->grabWindow();
    QVERIFY(!image.isNull());
    const QRect reserved(0, 0, image.width(), topBar.top());
    keepCapture(image, reserved);
    QCOMPARE(paintedPixels(image, reserved), 0);
    QVERIFY(paintedPixels(image, QRect(0, topBar.top(), image.width(), image.height())) > 0);
}

// Without the 72 px dock a column holds six icons; with it only five, so the
// sixth starts the second column instead of sitting under the dock.
void DesktopIconWorkAreaTests::aBottomDockShortensEveryColumn()
{
    SurfaceHost host;
    QVERIFY(createHost(&host, onlyOutput(QMargins(0, 24, 0, 72))));
    QQuickItem *sixth = tileAt(host, 5);
    QVERIFY(sixth != nullptr);
    QTRY_COMPARE(sixth->x(), qreal(kMargin + kColumn));
    QCOMPARE(sixth->y(), qreal(24 + kMargin));
    for (int index = 0; index < 7; ++index) {
        QQuickItem *tile = tileAt(host, index);
        QVERIFY(tile->y() >= 24);
        QVERIFY2(tile->y() + kTileHeight <= kOutput.height() - 72,
                 qPrintable(QStringLiteral("icon %1 reaches under the dock").arg(index)));
    }
}

// The stock default layout's full-height left shelf, then a right rail with
// right-edge placement: the flow starts beside the panel, not under it.
void DesktopIconWorkAreaTests::sidePanelsPushTheFlowInward()
{
    {
        SurfaceHost host;
        QVERIFY(createHost(&host, onlyOutput(QMargins(52, 26, 0, 0))));
        QQuickItem *first = tileAt(host, 0);
        QVERIFY(first != nullptr);
        QTRY_COMPARE(first->x(), qreal(52 + kMargin));
        QCOMPARE(first->y(), qreal(26 + kMargin));
    }
    SurfaceHost host;
    QVERIFY(createHost(&host, onlyOutput(QMargins(0, 0, 64, 0)),
                       {{QStringLiteral("placement"), QStringLiteral("right")}}));
    QQuickItem *first = tileAt(host, 0);
    QVERIFY(first != nullptr);
    QTRY_COMPARE(first->x() + kTileWidth, qreal(kOutput.width() - 64 - kMargin));
    QCOMPARE(first->y(), qreal(kMargin));
}

// Two outputs with different panels: the primary's top bar shapes the default
// flow, and an icon saved under the second output's left panel is drawn by
// that output just beside the panel.
void DesktopIconWorkAreaTests::eachOutputKeepsItsOwnPanels()
{
    const QVariantList outputs{
        output(QStringLiteral("PRIMARY"), QRect(0, 0, 400, 600), QMargins(0, 26, 0, 0)),
        output(QStringLiteral("SECOND"), QRect(400, 0, 400, 600), QMargins(52, 0, 0, 0))};
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    SurfaceHost primary;
    SurfaceHost second;
    QString error;
    QVERIFY2(primary.create(nullptr, &m_launcher, {}, &error, outputs,
                            QStringLiteral("PRIMARY"), QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QVERIFY2(second.create(nullptr, &m_launcher, {}, &error, outputs,
                           QStringLiteral("SECOND"), QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QVERIFY(QTest::qWaitForWindowExposed(primary.window.get()));
    QVERIFY(QTest::qWaitForWindowExposed(second.window.get()));

    QQuickItem *first = tileAt(primary, 0);
    QVERIFY(first != nullptr);
    QTRY_COMPARE(first->y(), qreal(26 + kMargin));
    QCOMPARE(first->x(), qreal(kMargin));

    const QString key = tileAt(primary, 6)->property("layoutKey").toString();
    QVERIFY(store.setPosition(key, 400 + 10, 200));
    QQuickItem *moved = tileAt(second, 6);
    QVERIFY(moved != nullptr);
    QTRY_VERIFY(moved->isVisible());
    QTRY_COMPARE(moved->x(), qreal(52 + kMargin));
    QCOMPARE(moved->y(), 200.0);
    QVERIFY(!tileAt(primary, 6)->isVisible());
}

// An output nothing reserves on - including one whose only panel auto-hides
// and is hidden, which publishes no zone - keeps the full-output flow, and an
// entry without any work area behaves exactly as before work areas existed.
void DesktopIconWorkAreaTests::anOutputWithoutReservationsIsAllWorkArea()
{
    {
        SurfaceHost host;
        QVERIFY(createHost(&host, onlyOutput(QMargins())));
        QQuickItem *first = tileAt(host, 0);
        QVERIFY(first != nullptr);
        QTRY_COMPARE(first->y(), qreal(kMargin));
        QCOMPARE(first->x(), qreal(kMargin));
    }
    QVariantMap bare = rectMap(kOutput);
    bare.insert(QStringLiteral("name"), QStringLiteral("OFFSCREEN0"));
    SurfaceHost host;
    QVERIFY(createHost(&host, {bare}));
    QQuickItem *sixth = tileAt(host, 5);
    QVERIFY(sixth != nullptr);
    QTRY_COMPARE(sixth->y(), qreal(kMargin + 5 * kRow));
    QCOMPARE(sixth->x(), qreal(kMargin));
}

// "An icon persisted under the bar moves just below it" - at resolution only.
// The saved document keeps every position exactly as written, so the icon
// gets its place back if the bar goes away, and an unrelated saved icon is
// not moved at all.
void DesktopIconWorkAreaTests::aStoredPositionUnderTheBarIsClampedWithoutRewritingTheStore()
{
    SurfaceHost host;
    QVERIFY(createHost(&host, onlyOutput(QMargins(0, 26, 0, 0))));
    QQuickItem *underBar = tileNamed(host, QStringLiteral("Item 3.txt"));
    QQuickItem *elsewhere = tileNamed(host, QStringLiteral("Item 4.txt"));
    QVERIFY(underBar != nullptr && elsewhere != nullptr);
    const QString underKey = underBar->property("layoutKey").toString();
    const QString elsewhereKey = elsewhere->property("layoutKey").toString();
    QVERIFY(host.layoutStore->setPosition(underKey, 330, 4));
    QVERIFY(host.layoutStore->setPosition(elsewhereKey, 450, 300));

    QTRY_COMPARE(underBar->y(), qreal(26 + kMargin));
    QCOMPARE(underBar->x(), 330.0);
    QTRY_COMPARE(elsewhere->x(), 450.0);
    QCOMPARE(elsewhere->y(), 300.0);

    QCOMPARE(host.layoutStore->position(underKey).value(QStringLiteral("y")).toReal(), 4.0);
    QCOMPARE(host.layoutStore->position(elsewhereKey).value(QStringLiteral("y")).toReal(),
             300.0);
    DesktopIconLayoutStore reread(SurfaceHost::storagePath());
    QCOMPARE(reread.position(underKey).value(QStringLiteral("y")).toReal(), 4.0);
}

// A reservation that appears, changes or goes away (a panel added, resized,
// or auto-hidden) reflows the live surface without recreating it.
void DesktopIconWorkAreaTests::aReservationChangeReflowsLive()
{
    SurfaceHost host;
    QVERIFY(createHost(&host, onlyOutput(QMargins())));
    QQuickItem *first = tileAt(host, 0);
    QQuickItem *saved = tileNamed(host, QStringLiteral("Item 7.txt"));
    QVERIFY(first != nullptr && saved != nullptr);
    QVERIFY(host.layoutStore->setPosition(saved->property("layoutKey").toString(), 450, 10));
    QTRY_COMPARE(first->y(), qreal(kMargin));
    QTRY_COMPARE(saved->y(), 10.0);

    host.window->setProperty("outputRects", onlyOutput(QMargins(0, 40, 0, 0)));
    QTRY_COMPARE(first->y(), qreal(40 + kMargin));
    QTRY_COMPARE(saved->y(), qreal(40 + kMargin));

    host.window->setProperty("outputRects", onlyOutput(QMargins(60, 0, 0, 0)));
    QTRY_COMPARE(first->x(), qreal(60 + kMargin));
    QTRY_COMPARE(first->y(), qreal(kMargin));
    QTRY_COMPARE(saved->y(), 10.0);

    host.window->setProperty("outputRects", onlyOutput(QMargins()));
    QTRY_COMPARE(first->x(), qreal(kMargin));
}

// The live drag stops at the panel's edge, and the drop - snapped or
// free-form - is saved inside the work area.
void DesktopIconWorkAreaTests::aDragCannotParkAnIconUnderThePanel()
{
    for (const bool snap : {true, false}) {
        QTemporaryDir layoutHome;
        QVERIFY(layoutHome.isValid());
        qputenv("XDG_DATA_HOME", layoutHome.path().toLocal8Bit());
        SurfaceHost host;
        QVERIFY(createHost(&host, onlyOutput(QMargins(0, 26, 0, 0)),
                           {{QStringLiteral("snapToGrid"), snap}}));
        QQuickItem *tile = tileAt(host, 2);
        QVERIFY(tile != nullptr);
        QTRY_COMPARE(tile->y(), qreal(26 + kMargin + 2 * kRow));
        const QPointF press = tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));
        const QPointF release = press + QPointF(300, -250);
        QTest::mousePress(host.window.get(), Qt::LeftButton, Qt::NoModifier, press.toPoint());
        QTest::mouseMove(host.window.get(), (press + QPointF(100, -100)).toPoint(), 10);
        QTest::mouseMove(host.window.get(), release.toPoint(), 10);
        // Mid-drag the icon rests against the bar, not under it.
        QCOMPARE(tile->y(), 26.0);
        QTest::mouseRelease(host.window.get(), Qt::LeftButton, Qt::NoModifier,
                            release.toPoint());

        const QVariantMap stored =
            host.layoutStore->position(tile->property("layoutKey").toString());
        QVERIFY(!stored.isEmpty());
        const qreal savedY = stored.value(QStringLiteral("y")).toReal();
        QCOMPARE(savedY, snap ? qreal(26 + kMargin) : 26.0);
        QTRY_COMPARE(tile->y(), savedY);
    }
}

QTEST_MAIN(DesktopIconWorkAreaTests)
#include "tst_desktop_icon_work_area.moc"
