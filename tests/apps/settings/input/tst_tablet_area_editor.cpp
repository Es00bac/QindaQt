// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_input/tablet_devices_model.h>
#include <qindaqt/services/tablet_devices/tablet_geometry.h>
#include <qindaqt/themes/theme_loader.h>

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include <cmath>
#include <memory>

#include "support/fake_tablet_ports.h"

namespace QindaQt::Apps::SettingsInput {

using namespace QindaQt::Tests;
using Services::TabletDevices::Rotation;
using Services::TabletDevices::TabletArea;

namespace {

QStringList &capturedMessages() {
    static QStringList messages;
    return messages;
}

void captureMessage(QtMsgType type, const QMessageLogContext &,
                    const QString &message) {
    if (type != QtDebugMsg && type != QtInfoMsg) {
        capturedMessages().append(message);
    }
}

// Only warnings raised by QML count here; the offscreen platform may say
// things about fonts or screens that are not this editor's business.
QStringList qmlWarnings() {
    QStringList warnings;
    for (const QString &message : capturedMessages()) {
        if (message.contains(QStringLiteral(".qml")) ||
            message.contains(QStringLiteral(".js")) ||
            message.contains(QStringLiteral("qrc:"))) {
            warnings.append(message);
        }
    }
    return warnings;
}

QObject *findInTree(QObject *root, const QString &objectName) {
    QList<QObject *> pending{root};
    QSet<QObject *> seen;
    while (!pending.isEmpty()) {
        QObject *node = pending.takeFirst();
        if (node == nullptr || seen.contains(node)) {
            continue;
        }
        seen.insert(node);
        if (node->objectName() == objectName) {
            return node;
        }
        pending.append(node->children());
        if (auto *item = qobject_cast<QQuickItem *>(node)) {
            const QList<QQuickItem *> childItems = item->childItems();
            for (QQuickItem *child : childItems) {
                pending.append(child);
            }
        }
    }
    return nullptr;
}

bool near(double first, double second, double tolerance) {
    return std::abs(first - second) <= tolerance;
}

QVariant lastWrite(const FakeTabletPort &port, const QString &property) {
    QVariant value;
    for (const auto &write : port.writes) {
        if (std::get<1>(write) == property) {
            value = std::get<2>(write);
        }
    }
    return value;
}

} // namespace

class TabletAreaEditorTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void dragInsideTheRectangleMovesIt();
    void dragACornerResizesFromTheOppositeCorner();
    void dragOnTheEmptySurfaceDrawsANewRectangle();
    void aClickChangesNothing();
    void aLockedAspectHoldsThroughAResize();
    void keysMoveResizeAndFill();
    void theCanvasIsNamedAndDescribedForAssistiveTechnology();
    void theEditorWritesWhatTheUserDrawsInKWinsFrame();

private:
    QQuickItem *load(const QByteArray &type, const QVariantMap &initial,
                     const QSize &size);
    [[nodiscard]] QPointF framePoint(QQuickItem *canvas, double x,
                                     double y) const;
    void drag(QPointF from, QPointF to);

    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QObject *root = nullptr;
};

void TabletAreaEditorTest::init() {
    capturedMessages().clear();
    qInstallMessageHandler(captureMessage);
}

void TabletAreaEditorTest::cleanup() {
    window.reset();
    engine.reset();
    root = nullptr;
    qInstallMessageHandler(nullptr);
}

QQuickItem *TabletAreaEditorTest::load(const QByteArray &type,
                                       const QVariantMap &initial,
                                       const QSize &size) {
    engine = std::make_unique<QQmlEngine>();
    engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    QString facadeError;
    auto *tokens = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *engine, &facadeError);
    if (tokens == nullptr) {
        qWarning("token facade: %s", qPrintable(facadeError));
        return nullptr;
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QString publishError;
    if (!theme.ok || !tokens->publish(theme.theme, {}, &publishError)) {
        qWarning("theme: %s %s", qPrintable(theme.error),
                 qPrintable(publishError));
        return nullptr;
    }
    QQmlComponent component(engine.get());
    component.setData(QByteArray("import QtQuick\n"
                                 "import QindaQt.SettingsApp.Input\n") +
                          type + QByteArray(" { objectName: \"subject\" }\n"),
                      QUrl(QStringLiteral("qindaqt-test://tablet-area")));
    // Same as the page row: the module's imports may resolve asynchronously.
    (void)QTest::qWaitFor(
        [&component] { return component.status() != QQmlComponent::Loading; },
        10000);
    if (component.status() != QQmlComponent::Ready) {
        qWarning("component: %s", qPrintable(component.errorString()));
        return nullptr;
    }
    root = component.createWithInitialProperties(initial);
    auto *item = qobject_cast<QQuickItem *>(root);
    if (item == nullptr) {
        qWarning("create: %s", qPrintable(component.errorString()));
        return nullptr;
    }
    window = std::make_unique<QQuickWindow>();
    window->resize(size);
    item->setParentItem(window->contentItem());
    item->setSize(QSizeF(size));
    window->show();
    if (!QTest::qWaitForWindowExposed(window.get())) {
        return nullptr;
    }
    return item;
}

QPointF TabletAreaEditorTest::framePoint(QQuickItem *canvas, double x,
                                         double y) const {
    auto *frame = qobject_cast<QQuickItem *>(
        findInTree(canvas, QStringLiteral("tabletAreaFrame")));
    if (frame == nullptr) {
        return {};
    }
    return frame->mapToScene(QPointF(frame->width() * x, frame->height() * y));
}

void TabletAreaEditorTest::drag(QPointF from, QPointF to) {
    QTest::mousePress(window.get(), Qt::LeftButton, Qt::NoModifier,
                      from.toPoint());
    QTest::mouseMove(window.get(), ((from + to) / 2).toPoint(), 10);
    QTest::mouseMove(window.get(), to.toPoint(), 10);
    QTest::mouseRelease(window.get(), Qt::LeftButton, Qt::NoModifier,
                        to.toPoint());
}

void TabletAreaEditorTest::dragInsideTheRectangleMovesIt() {
    // 400 x 250 at 16:10: the frame fills the canvas, so a pixel is 1/400
    // across and 1/250 down.
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("surfaceAspect"), 1.6},
                               {QStringLiteral("areaX"), 0.25},
                               {QStringLiteral("areaY"), 0.25},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.5}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    drag(framePoint(canvas, 0.5, 0.5), framePoint(canvas, 0.6, 0.6));
    QCOMPARE(committed.size(), 1);
    QVERIFY(near(committed.at(0).at(0).toDouble(), 0.35, 0.005));
    QVERIFY(near(committed.at(0).at(1).toDouble(), 0.35, 0.005));
    // Moving never resizes.
    QVERIFY(near(committed.at(0).at(2).toDouble(), 0.5, 1e-9));
    QVERIFY(near(committed.at(0).at(3).toDouble(), 0.5, 1e-9));
    // Past the edge it stops at the edge.
    drag(framePoint(canvas, 0.5, 0.5), framePoint(canvas, 0.99, 0.5));
    QCOMPARE(committed.size(), 2);
    QVERIFY(near(committed.at(1).at(0).toDouble(), 0.5, 1e-9));
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

void TabletAreaEditorTest::dragACornerResizesFromTheOppositeCorner() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("surfaceAspect"), 1.6},
                               {QStringLiteral("areaX"), 0.25},
                               {QStringLiteral("areaY"), 0.25},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.5}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    // Grab the bottom-right handle and pull it out.
    drag(framePoint(canvas, 0.75, 0.75), framePoint(canvas, 0.9, 0.9));
    QCOMPARE(committed.size(), 1);
    QVERIFY(near(committed.at(0).at(0).toDouble(), 0.25, 1e-9));
    QVERIFY(near(committed.at(0).at(1).toDouble(), 0.25, 1e-9));
    QVERIFY(near(committed.at(0).at(2).toDouble(), 0.65, 0.005));
    QVERIFY(near(committed.at(0).at(3).toDouble(), 0.65, 0.005));
    // The top-left handle anchors on the bottom-right corner.
    drag(framePoint(canvas, 0.25, 0.25), framePoint(canvas, 0.1, 0.4));
    QCOMPARE(committed.size(), 2);
    QVERIFY(near(committed.at(1).at(0).toDouble(), 0.1, 0.005));
    QVERIFY(near(committed.at(1).at(1).toDouble(), 0.4, 0.005));
    QVERIFY(near(committed.at(1).at(2).toDouble(), 0.65, 0.005));
    QVERIFY(near(committed.at(1).at(3).toDouble(), 0.35, 0.005));
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

void TabletAreaEditorTest::dragOnTheEmptySurfaceDrawsANewRectangle() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("surfaceAspect"), 1.6},
                               {QStringLiteral("areaX"), 0.5},
                               {QStringLiteral("areaY"), 0.5},
                               {QStringLiteral("areaWidth"), 0.4},
                               {QStringLiteral("areaHeight"), 0.4}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    drag(framePoint(canvas, 0.3, 0.4), framePoint(canvas, 0.05, 0.08));
    QCOMPARE(committed.size(), 1);
    QVERIFY(near(committed.at(0).at(0).toDouble(), 0.05, 0.005));
    QVERIFY(near(committed.at(0).at(1).toDouble(), 0.08, 0.005));
    QVERIFY(near(committed.at(0).at(2).toDouble(), 0.25, 0.005));
    QVERIFY(near(committed.at(0).at(3).toDouble(), 0.32, 0.005));
    // A slip smaller than the smallest usable area changes nothing.
    drag(framePoint(canvas, 0.1, 0.9), framePoint(canvas, 0.12, 0.92));
    QCOMPARE(committed.size(), 1);
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

void TabletAreaEditorTest::aClickChangesNothing() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("areaX"), 0.25},
                               {QStringLiteral("areaY"), 0.25},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.5}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    QTest::mouseClick(window.get(), Qt::LeftButton, Qt::NoModifier,
                      framePoint(canvas, 0.1, 0.1).toPoint());
    QTest::mouseClick(window.get(), Qt::LeftButton, Qt::NoModifier,
                      framePoint(canvas, 0.5, 0.5).toPoint());
    QCOMPARE(committed.size(), 0);
    // The click did give the canvas the keyboard.
    QVERIFY(canvas->hasActiveFocus());
}

void TabletAreaEditorTest::aLockedAspectHoldsThroughAResize() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("surfaceAspect"), 1.6},
                               {QStringLiteral("areaX"), 0.25},
                               {QStringLiteral("areaY"), 0.25},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.5},
                               {QStringLiteral("lockedAspect"), 1.0}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    // A mostly horizontal pull: the height follows the width.
    drag(framePoint(canvas, 0.75, 0.75), framePoint(canvas, 0.95, 0.8));
    QCOMPARE(committed.size(), 1);
    const double width = committed.at(0).at(2).toDouble();
    const double height = committed.at(0).at(3).toDouble();
    QVERIFY(near(width, height, 1e-9));
    QVERIFY(near(width, 0.7, 0.005));
    // Keyboard resizing keeps it too.
    canvas->forceActiveFocus();
    QTest::keyClick(window.get(), Qt::Key_Right, Qt::ShiftModifier);
    QCOMPARE(committed.size(), 2);
    QVERIFY(near(committed.at(1).at(2).toDouble(),
                 committed.at(1).at(3).toDouble(), 1e-9));
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

void TabletAreaEditorTest::keysMoveResizeAndFill() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("areaX"), 0.25},
                               {QStringLiteral("areaY"), 0.25},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.5}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QSignalSpy committed(canvas, SIGNAL(committed(double,double,double,double)));
    canvas->forceActiveFocus();
    QVERIFY(canvas->hasActiveFocus());

    QTest::keyClick(window.get(), Qt::Key_Right);
    QCOMPARE(committed.size(), 1);
    QVERIFY(near(committed.at(0).at(0).toDouble(), 0.26, 1e-9));
    QVERIFY(near(committed.at(0).at(2).toDouble(), 0.5, 1e-9));

    QTest::keyClick(window.get(), Qt::Key_Down, Qt::ShiftModifier);
    QCOMPARE(committed.size(), 2);
    QVERIFY(near(committed.at(1).at(3).toDouble(), 0.51, 1e-9));
    QVERIFY(near(committed.at(1).at(1).toDouble(), 0.25, 1e-9));

    QTest::keyClick(window.get(), Qt::Key_Home);
    QCOMPARE(committed.size(), 3);
    QVERIFY(near(committed.at(2).at(2).toDouble(), 1.0, 1e-9));
    QVERIFY(near(committed.at(2).at(3).toDouble(), 1.0, 1e-9));

    // At the edge a move changes nothing and says nothing.
    canvas->setProperty("areaX", 0.0);
    QTest::keyClick(window.get(), Qt::Key_Left);
    QCOMPARE(committed.size(), 3);
    // Shrinking never goes below the smallest usable area.
    canvas->setProperty("areaWidth", 0.05);
    QTest::keyClick(window.get(), Qt::Key_Left, Qt::ShiftModifier);
    QCOMPARE(committed.size(), 3);
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

void TabletAreaEditorTest::theCanvasIsNamedAndDescribedForAssistiveTechnology() {
    QQuickItem *canvas = load("TabletAreaCanvas",
                              {{QStringLiteral("surfaceName"),
                                QStringLiteral("Part of the tablet the pen uses")},
                               {QStringLiteral("areaX"), 0.1},
                               {QStringLiteral("areaY"), 0.2},
                               {QStringLiteral("areaWidth"), 0.5},
                               {QStringLiteral("areaHeight"), 0.6}},
                              QSize(400, 250));
    QVERIFY(canvas != nullptr);
    QQmlContext *context = qmlContext(canvas);
    QCOMPARE(QQmlProperty(canvas, QStringLiteral("Accessible.name"), context)
                 .read()
                 .toString(),
             QStringLiteral("Part of the tablet the pen uses"));
    const QString description =
        QQmlProperty(canvas, QStringLiteral("Accessible.description"), context)
            .read()
            .toString();
    // The rectangle in words, and how to change it without a pointer.
    QVERIFY2(description.contains(QStringLiteral("10%")) &&
                 description.contains(QStringLiteral("60%")) &&
                 description.contains(QStringLiteral("20%")) &&
                 description.contains(QStringLiteral("80%")),
             qPrintable(description));
    QVERIFY(description.contains(QStringLiteral("Arrow keys")));
    QCOMPARE(QQmlProperty(canvas, QStringLiteral("Accessible.role"), context)
                 .read()
                 .toInt(),
             int(QAccessible::Canvas));
    QVERIFY(canvas->activeFocusOnTab());
}

void TabletAreaEditorTest::theEditorWritesWhatTheUserDrawsInKWinsFrame() {
    FakeTabletPort port;
    FakeTabletOutputs outputs;
    outputs.scripted = {fakeMonitor(QStringLiteral("DP-1"), Rotation::Cw90,
                                    QRectF(0, 0, 1080, 1920))};
    port.scripted = {fakeBambooPen(QStringLiteral("DP-1"))};
    FakeTabletMappingStore store;
    TabletDevicesModel model(port, outputs, &store);
    model.refresh();
    QObject *placement = model.selection()->placement();

    QQuickItem *editor = load(
        "TabletAreaEditor",
        {{QStringLiteral("placement"), QVariant::fromValue(placement)}},
        QSize(720, 320));
    QVERIFY(editor != nullptr);
    auto *screenCanvas = qobject_cast<QQuickItem *>(
        findInTree(editor, QStringLiteral("tabletOutputCanvas")));
    auto *tabletCanvas = qobject_cast<QQuickItem *>(
        findInTree(editor, QStringLiteral("tabletInputCanvas")));
    QVERIFY(screenCanvas != nullptr && screenCanvas->isVisible());
    QVERIFY(tabletCanvas != nullptr && tabletCanvas->isVisible());
    // The screen is drawn as it appears: the monitor is portrait now.
    QVERIFY(std::abs(screenCanvas->property("surfaceAspect").toDouble() -
                     1080.0 / 1920.0) < 1e-9);

    // Pull the bottom-right corner in to the middle: the top-left quarter of
    // the screen as the user sees it.
    drag(framePoint(screenCanvas, 0.99, 0.99), framePoint(screenCanvas, 0.5, 0.5));
    const QVariantList shown =
        placement->property("outputArea").toList();
    QCOMPARE(shown.size(), 4);
    QVERIFY(near(shown.at(0).toDouble(), 0.0, 1e-9));
    QVERIFY(near(shown.at(1).toDouble(), 0.0, 1e-9));
    QVERIFY(near(shown.at(2).toDouble(), 0.5, 0.02));
    QVERIFY(near(shown.at(3).toDouble(), 0.5, 0.02));
    // KWin gets it in the panel's native frame, before its 90° transform.
    bool ok = false;
    const TabletArea written = TabletArea::fromVariant(
        lastWrite(port, QStringLiteral("outputArea")), &ok);
    QVERIFY(ok);
    const TabletArea expected = Services::TabletDevices::rotateArea(
        TabletArea{shown.at(0).toDouble(), shown.at(1).toDouble(),
                   shown.at(2).toDouble(), shown.at(3).toDouble()},
        Rotation::Cw270);
    QVERIFY(Services::TabletDevices::sameArea(written, expected));

    // Keyboard on the tablet canvas: one step narrower from the right.
    tabletCanvas->forceActiveFocus();
    QTest::keyClick(window.get(), Qt::Key_Left, Qt::ShiftModifier);
    const QVariantList input = placement->property("inputArea").toList();
    QCOMPARE(input.size(), 4);
    QVERIFY(near(input.at(2).toDouble(), 0.99, 1e-9));
    QVERIFY(lastWrite(port, QStringLiteral("inputArea")).isValid());

    // Keep proportions is offered because the tablet and screen sizes are
    // known, and it reshapes the screen area.
    auto *keep = findInTree(editor, QStringLiteral("tabletKeepProportions"));
    QVERIFY(keep != nullptr);
    QVERIFY(keep->property("visible").toBool());
    QVERIFY(placement->setProperty("keepProportions", true));
    QVERIFY(keep->property("checked").toBool());
    QVERIFY(screenCanvas->property("lockedAspect").toDouble() > 0.0);
    QVERIFY2(qmlWarnings().isEmpty(), qPrintable(qmlWarnings().join(u'\n')));
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_MAIN(QindaQt::Apps::SettingsInput::TabletAreaEditorTest)
#include "tst_tablet_area_editor.moc"
