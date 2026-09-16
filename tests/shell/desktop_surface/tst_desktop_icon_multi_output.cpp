// SPDX-License-Identifier: GPL-3.0-or-later

// ADR-0167: the desktop is ONE desktop spread over several outputs, not one
// desktop per output. These rows pin the user-reported outcome - "the desktop
// is one thing, not two things just because there are two monitors, there
// shouldn't be two sets of desktop icons. Default to primary display, allow
// for placement on other displays manually."

#include "desktop_surface_qml_test_support.h"

#include "qindaqt/shell/desktop_surface/desktop_icon_layout_store.h"

#include <QDir>
#include <QFile>
#include <QQmlExtensionPlugin>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;
using QindaQt::Shell::DesktopSurface::DesktopIconLayoutStore;

namespace {

// The fixture's outputs are deliberately NARROWER than the 800x600 test
// window, so a drag that crosses the seam between them stays inside the window
// the test can deliver pointer events to. Ownership and local mapping come
// from these rectangles, not from the window size.
constexpr int kOutputWidth = 400;
constexpr int kOutputHeight = 600;

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

// Two side-by-side outputs in the global frame: PRIMARY at the origin and
// SECOND immediately to its right, the same shape as the reporter's session.
QVariantList twoOutputs()
{
    return QVariantList{
        QVariantMap{{QStringLiteral("name"), QStringLiteral("PRIMARY")},
                    {QStringLiteral("x"), 0},
                    {QStringLiteral("y"), 0},
                    {QStringLiteral("width"), kOutputWidth},
                    {QStringLiteral("height"), kOutputHeight}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("SECOND")},
                    {QStringLiteral("x"), kOutputWidth},
                    {QStringLiteral("y"), 0},
                    {QStringLiteral("width"), kOutputWidth},
                    {QStringLiteral("height"), kOutputHeight}}};
}

QQuickItem *tileNamed(const QList<QQuickItem *> &tiles, const QString &label)
{
    for (QQuickItem *tile : tiles) {
        if (tile->property("entryLabel").toString() == label) {
            return tile;
        }
    }
    return nullptr;
}

int visibleTileCount(const SurfaceHost &host)
{
    int count = 0;
    for (const QQuickItem *tile :
         host.visualItemsNamed(QStringLiteral("desktopIconsTile"))) {
        if (tile->isVisible()) {
            ++count;
        }
    }
    return count;
}

} // namespace

class DesktopIconMultiOutputTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void cleanup();
    void everyIconAppearsOnExactlyOneOutput();
    void unplacedIconsDefaultToThePrimaryOutput();
    void aManualPlacementMovesTheIconToTheOtherOutput();
    void draggingPastTheSeamLandsOnTheOtherOutput();
    void aRemovedOutputsIconsFallBackToThePrimary();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
};

void DesktopIconMultiOutputTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME",
            (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Alpha.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Beta.txt")));
    QVERIFY(writeFile(m_home->path() + QStringLiteral("/Desktop/Gamma.txt")));
}

void DesktopIconMultiOutputTests::cleanup()
{
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
}

// The reported defect: two outputs each drew their own complete set of icons.
void DesktopIconMultiOutputTests::everyIconAppearsOnExactlyOneOutput()
{
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    StubLauncher launcher;
    SurfaceHost primary;
    SurfaceHost second;
    QString error;
    QVERIFY2(primary.create(nullptr, &launcher, {}, &error, twoOutputs(),
                            QStringLiteral("PRIMARY"),
                            QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QVERIFY2(second.create(nullptr, &launcher, {}, &error, twoOutputs(),
                           QStringLiteral("SECOND"),
                           QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QTRY_VERIFY(primary.window->isExposed());
    QTRY_VERIFY(second.window->isExposed());

    // Both surfaces instantiate a delegate per entry - that keeps the model
    // stable - but the icon is DRAWN by exactly one of them.
    QCOMPARE(primary.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    QCOMPARE(second.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    QTRY_COMPARE(visibleTileCount(primary), 3);
    QCOMPARE(visibleTileCount(second), 0);
}

void DesktopIconMultiOutputTests::unplacedIconsDefaultToThePrimaryOutput()
{
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    StubLauncher launcher;
    SurfaceHost second;
    QString error;
    // Only the non-primary surface exists here: it must still draw nothing,
    // because an icon nobody placed belongs to the primary output.
    QVERIFY2(second.create(nullptr, &launcher, {}, &error, twoOutputs(),
                           QStringLiteral("SECOND"),
                           QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QTRY_VERIFY(second.window->isExposed());
    QTRY_COMPARE(second.visualItemsNamed(QStringLiteral("desktopIconsTile")).size(), 3);
    QCOMPARE(visibleTileCount(second), 0);
}

// "allow for placement on other displays manually" - a global placement inside
// the second output's rectangle moves the icon there, and the primary stops
// drawing it.
void DesktopIconMultiOutputTests::aManualPlacementMovesTheIconToTheOtherOutput()
{
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    StubLauncher launcher;
    SurfaceHost primary;
    SurfaceHost second;
    QString error;
    QVERIFY2(primary.create(nullptr, &launcher, {}, &error, twoOutputs(),
                            QStringLiteral("PRIMARY"),
                            QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QVERIFY2(second.create(nullptr, &launcher, {}, &error, twoOutputs(),
                           QStringLiteral("SECOND"),
                           QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QTRY_VERIFY(primary.window->isExposed());
    QTRY_VERIFY(second.window->isExposed());
    QTRY_COMPARE(visibleTileCount(primary), 3);

    QQuickItem *tile = tileNamed(
        primary.visualItemsNamed(QStringLiteral("desktopIconsTile")),
        QStringLiteral("Beta.txt"));
    QVERIFY(tile != nullptr);
    const QString layoutKey = tile->property("layoutKey").toString();
    QVERIFY(store.setPosition(layoutKey, kOutputWidth + 120, 90));

    QTRY_COMPARE(visibleTileCount(primary), 2);
    QTRY_COMPARE(visibleTileCount(second), 1);
    // The second output renders it in ITS OWN local coordinates.
    QQuickItem *moved = tileNamed(
        second.visualItemsNamed(QStringLiteral("desktopIconsTile")),
        QStringLiteral("Beta.txt"));
    QVERIFY(moved != nullptr);
    QTRY_COMPARE(moved->x(), 120.0);
    QCOMPARE(moved->y(), 90.0);
}

// The same outcome through the gesture a user actually performs: drag an icon
// past the seam. The surface holding the pointer grab publishes live positions
// through the shared store, so the receiving output can adopt the icon.
void DesktopIconMultiOutputTests::draggingPastTheSeamLandsOnTheOtherOutput()
{
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    StubLauncher launcher;
    SurfaceHost primary;
    SurfaceHost second;
    QString error;
    QVERIFY2(primary.create(nullptr, &launcher, {}, &error, twoOutputs(),
                            QStringLiteral("PRIMARY"),
                            QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QVERIFY2(second.create(nullptr, &launcher, {}, &error, twoOutputs(),
                           QStringLiteral("SECOND"),
                           QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QTRY_VERIFY(primary.window->isExposed());
    QTRY_VERIFY(second.window->isExposed());
    QTRY_COMPARE(visibleTileCount(primary), 3);

    QQuickItem *tile = tileNamed(
        primary.visualItemsNamed(QStringLiteral("desktopIconsTile")),
        QStringLiteral("Alpha.txt"));
    QVERIFY(tile != nullptr);
    const QString layoutKey = tile->property("layoutKey").toString();
    const QPointF press =
        tile->mapToScene(QPointF(tile->width() / 2, tile->height() / 2));

    // The pointer travels past this surface's right edge; the grab keeps the
    // events coming here even though the icon now belongs to the other output.
    QTest::mousePress(primary.window.get(), Qt::LeftButton, Qt::NoModifier,
                      press.toPoint());
    QTest::mouseMove(primary.window.get(), (press + QPointF(200, 40)).toPoint(), 10);
    QTest::mouseMove(primary.window.get(), (press + QPointF(500, 80)).toPoint(), 10);
    // Mid-drag the receiving output already draws it.
    QTRY_COMPARE(visibleTileCount(second), 1);
    QTest::mouseRelease(primary.window.get(), Qt::LeftButton, Qt::NoModifier,
                        (press + QPointF(500, 80)).toPoint());

    const QVariantMap stored = store.position(layoutKey);
    QVERIFY(!stored.isEmpty());
    QVERIFY(stored.value(QStringLiteral("x")).toReal() >= kOutputWidth);
    QTRY_COMPARE(visibleTileCount(primary), 2);
    QTRY_COMPARE(visibleTileCount(second), 1);
}

// Unplugging an output must not strand its icons where nothing can draw them.
void DesktopIconMultiOutputTests::aRemovedOutputsIconsFallBackToThePrimary()
{
    DesktopIconLayoutStore store(SurfaceHost::storagePath());
    StubLauncher launcher;
    QString error;
    {
        SurfaceHost second;
        QVERIFY2(second.create(nullptr, &launcher, {}, &error, twoOutputs(),
                               QStringLiteral("SECOND"),
                               QStringLiteral("PRIMARY"), &store),
                 qPrintable(error));
        QTRY_VERIFY(second.window->isExposed());
        QQuickItem *tile = tileNamed(
            second.visualItemsNamed(QStringLiteral("desktopIconsTile")),
            QStringLiteral("Gamma.txt"));
        QVERIFY(tile != nullptr);
        QVERIFY(store.setPosition(tile->property("layoutKey").toString(),
                                  kOutputWidth + 200, 150));
        QTRY_COMPARE(visibleTileCount(second), 1);
    }

    // The second output is gone: only the primary's rectangle remains, and the
    // icon placed off it is drawn by the primary rather than by nobody.
    SurfaceHost primary;
    const QVariantList onlyPrimary{twoOutputs().constFirst()};
    QVERIFY2(primary.create(nullptr, &launcher, {}, &error, onlyPrimary,
                            QStringLiteral("PRIMARY"),
                            QStringLiteral("PRIMARY"), &store),
             qPrintable(error));
    QTRY_VERIFY(primary.window->isExposed());
    QTRY_COMPARE(visibleTileCount(primary), 3);
}

QTEST_MAIN(DesktopIconMultiOutputTests)
#include "tst_desktop_icon_multi_output.moc"
