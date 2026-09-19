// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_provider.h"
#include "viewer_actions.h"
#include "viewer_controller.h"
#include "fixtures.h"
#include <qindaqt/app_shell/application_coordinator.h>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

class ViewerUiTest final : public QObject {
    Q_OBJECT
private slots:
    void documentInteractionAndLayouts();
};

void ViewerUiTest::documentInteractionAndLayouts()
{
    QTemporaryDir temp;
    QindaQt::Viewer::ViewerController viewer;
    QindaQt::AppShell::ApplicationCoordinator coordinator;
    QVERIFY(coordinator.replaceActions(QindaQt::Viewer::viewerActions()).ok());
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *provider = new QindaQt::Viewer::FrameProvider;
    engine.addImageProvider(QStringLiteral("document"), provider);
    connect(&viewer, &QindaQt::Viewer::ViewerController::frameChanged, &engine,
            [&] { provider->setFrame(viewer.frame()); });
    engine.rootContext()->setContextProperty(QStringLiteral("viewer"), &viewer);
    engine.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);
    engine.load(QUrl::fromLocalFile(QStringLiteral(VIEWER_MAIN_QML)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    viewer.open(QUrl::fromLocalFile(ViewerFixtures::pdf(temp.path())));
    QTRY_VERIFY(viewer.ready() && !viewer.busy());
    QTest::qWait(250);
    auto *viewport = window->findChild<QQuickItem *>(QStringLiteral("documentViewport"));
    auto *document = window->findChild<QQuickItem *>(QStringLiteral("documentImage"));
    QVERIFY(viewport && document);
    QVERIFY(document->width() > 0 && document->height() > 0);
    QVERIFY(window->grabWindow().save(QStringLiteral(VIEWER_ARTIFACTS_DIR "/viewer-960x680.png")));

    auto *next = window->findChild<QQuickItem *>(QStringLiteral("nextPage"));
    QVERIFY(next && next->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        next->mapToScene(QPointF(next->width() / 2, next->height() / 2)).toPoint());
    QTRY_VERIFY(viewer.page() == 1 && !viewer.busy());
    QCOMPARE(viewer.frame().pixelColor(10, 10), QColor(Qt::blue));
    QTest::keyClick(window, Qt::Key_PageUp);
    QTRY_VERIFY(viewer.page() == 0 && !viewer.busy());
    QTest::keyClick(window, Qt::Key_R, Qt::ControlModifier);
    QTRY_VERIFY(viewer.pageSize().height() > viewer.pageSize().width() && !viewer.busy());
    QTest::keyClick(window, Qt::Key_0, Qt::ControlModifier);
    QTRY_COMPARE(viewport->property("zoom").toDouble(), 1.0);
    QTest::keyClick(window, Qt::Key_2, Qt::ControlModifier);
    QTRY_COMPARE(viewport->property("fitMode").toString(), QStringLiteral("width"));
    QTest::keyClick(window, Qt::Key_1, Qt::ControlModifier);
    QTRY_COMPARE(viewport->property("fitMode").toString(), QStringLiteral("page"));

    window->resize(640, 480);
    QTest::qWait(300);
    QVERIFY(window->grabWindow().save(QStringLiteral(VIEWER_ARTIFACTS_DIR "/viewer-640x480.png")));
    for (const auto &name : {"openButton", "nextPage", "rotateButton", "fitPageButton"}) {
        auto *item = window->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY(item && item->isVisible());
        const QRectF bounds(item->mapToScene(QPointF(0, 0)), item->size());
        QVERIFY2(bounds.left() >= 0 && bounds.right() <= window->width(), name);
        QVERIFY2(bounds.top() >= 0 && bounds.bottom() < window->height(), name);
    }
    QTest::keyClick(window, Qt::Key_W, Qt::ControlModifier);
    QVERIFY(!viewer.ready());
    viewer.open(QUrl::fromLocalFile(QStringLiteral(VIEWER_FIXTURES_DIR "/password.pdf")));
    QTRY_VERIFY(viewer.locked() && !viewer.busy());
    auto *password = window->findChild<QQuickItem *>(QStringLiteral("passwordField"));
    QVERIFY(password);
    QTRY_VERIFY(password->hasActiveFocus());
    QCOMPARE(password->property("echoMode").toInt(), 2);
    for (const char key : QByteArray("viewer-test"))
        QTest::keyClick(window, key);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(viewer.ready() && !viewer.busy());
    QCOMPARE(password->property("text").toString(), QString());
    QTest::qWait(150);
    for (const auto &warning : warnings)
        qWarning() << warning;
    QCOMPARE(warnings.size(), 0);
}
QTEST_MAIN(ViewerUiTest)
#include "tst_ui.moc"
