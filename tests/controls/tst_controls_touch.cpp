// SPDX-License-Identifier: GPL-3.0-or-later
// The QindaQt.Controls touch conventions (ADR-0193): a held finger asks for
// the context menu, a tap or a mouse does not, and controls grow to the touch
// target when the token says the last input was a finger.
#include "control_test_support.h"

#include "qindaqt/design_tokens/token_facade.h"

#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Controls::TestSupport;

class ControlsTouchTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void heldFingerRequestsTheContextMenuOnce();
    void tapAndMouseDoNotRequestTheMenu();
    void touchModeGrowsControlsToTheMinimumTarget();

private:
    std::unique_ptr<QQuickView> loadProbe();
};

void ControlsTouchTests::initTestCase()
{
    pinDeterministicFonts();
}

std::unique_ptr<QQuickView> ControlsTouchTests::loadProbe()
{
    auto view = std::make_unique<QQuickView>();
    view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    if (!publishTheme(*view->engine(), QStringLiteral("qinda-dark.json"), {}, &error)) {
        qWarning("%s", qPrintable(error));
        return nullptr;
    }
    view->setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_CONTROLS_TEST_QML_DIR "/ControlsTouchProbe.qml")));
    if (!view->errors().isEmpty()) {
        qWarning("%s", qPrintable(view->errors().constFirst().toString()));
        return nullptr;
    }
    view->resize(400, 300);
    view->show();
    return view;
}

void ControlsTouchTests::heldFingerRequestsTheContextMenuOnce()
{
    auto view = loadProbe();
    QVERIFY(view);
    QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    auto *root = view->rootObject();
    QVERIFY(root != nullptr);
    QQuickItem *target = item(root, "touchTarget");
    QVERIFY(target != nullptr);
    auto *touch = QTest::createTouchDevice();
    const QPoint inside = target->mapToScene(QPointF(50.0, 40.0)).toPoint();
    QTest::touchEvent(view.get(), touch).press(1, inside, view.get());
    QTRY_COMPARE_WITH_TIMEOUT(root->property("contextRequests").toInt(), 1, 3000);
    QCOMPARE(root->property("lastContextPosition").toPointF(), QPointF(50.0, 40.0));
    // Holding on does not repeat the request; lifting ends it cleanly.
    QTest::qWait(300);
    QCOMPARE(root->property("contextRequests").toInt(), 1);
    QTest::touchEvent(view.get(), touch).release(1, inside, view.get());
    QTest::qWait(50);
    QCOMPARE(root->property("contextRequests").toInt(), 1);
    QCOMPARE(root->property("rightClicks").toInt(), 0);
}

void ControlsTouchTests::tapAndMouseDoNotRequestTheMenu()
{
    auto view = loadProbe();
    QVERIFY(view);
    QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    auto *root = view->rootObject();
    QQuickItem *target = item(root, "touchTarget");
    QVERIFY(target != nullptr);
    auto *touch = QTest::createTouchDevice();
    const QPoint inside = target->mapToScene(QPointF(50.0, 40.0)).toPoint();
    QTest::touchEvent(view.get(), touch).press(1, inside, view.get());
    QTest::qWait(120);
    QTest::touchEvent(view.get(), touch).release(1, inside, view.get());
    QTest::qWait(700);
    QCOMPARE(root->property("contextRequests").toInt(), 0);
    // A mouse has a right button: the pointer path stays the pointer path.
    QTest::mousePress(view.get(), Qt::LeftButton, Qt::NoModifier, inside);
    QTest::qWait(800);
    QTest::mouseRelease(view.get(), Qt::LeftButton, Qt::NoModifier, inside);
    QCOMPARE(root->property("contextRequests").toInt(), 0);
    QTest::mouseClick(view.get(), Qt::RightButton, Qt::NoModifier, inside);
    QTRY_COMPARE(root->property("rightClicks").toInt(), 1);
    QCOMPARE(root->property("contextRequests").toInt(), 0);
}

void ControlsTouchTests::touchModeGrowsControlsToTheMinimumTarget()
{
    auto view = loadProbe();
    QVERIFY(view);
    QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    auto *root = view->rootObject();
    QQuickItem *button = item(root, "touchButton");
    QVERIFY(button != nullptr);
    auto *facade = view->engine()->singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        QStringLiteral("QindaQt.Tokens"), QStringLiteral("Tokens"));
    QVERIFY(facade != nullptr);
    facade->setTouchState(true, false);
    QCOMPARE(facade->touch().value(QStringLiteral("minimumTarget")).toDouble(), 0.0);
    QCOMPARE(button->implicitHeight(), 40.0);
    facade->setTouchState(true, true);
    QCOMPARE(facade->touch().value(QStringLiteral("active")).toBool(), true);
    QCOMPARE(facade->touch().value(QStringLiteral("minimumTarget")).toDouble(), 44.0);
    QTRY_COMPARE(button->implicitHeight(), 44.0);
    // Back to a pointer: the shipped size returns.
    facade->setTouchState(true, false);
    QTRY_COMPARE(button->implicitHeight(), 40.0);
    // Active without a touchscreen is not a state: availability gates it.
    facade->setTouchState(false, true);
    QCOMPARE(facade->touch().value(QStringLiteral("active")).toBool(), false);
}

QTEST_MAIN(ControlsTouchTests)
#include "tst_controls_touch.moc"
