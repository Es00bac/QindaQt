// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_provider.h"
#include "viewer_actions.h"
#include "viewer_controller.h"
#include "fixtures.h"
#include <qindaqt/app_shell/application_coordinator.h>
#include <QClipboard>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

class ViewerTextUiTest final : public QObject {
    Q_OBJECT
private slots:
    void selectionSearchKeyboardAndLayouts();
};
void ViewerTextUiTest::selectionSearchKeyboardAndLayouts()
{
    QTemporaryDir temp; QindaQt::Viewer::ViewerController viewer;
    QindaQt::AppShell::ApplicationCoordinator coordinator;
    QVERIFY(coordinator.replaceActions(QindaQt::Viewer::viewerActions()).ok());
    connect(&viewer, &QindaQt::Viewer::ViewerController::stateChanged, &coordinator,
            [&] { QindaQt::Viewer::updateViewerActions(coordinator, viewer); });
    QindaQt::Viewer::updateViewerActions(coordinator, viewer);
    QQmlApplicationEngine engine; QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    auto *provider = new QindaQt::Viewer::FrameProvider;
    engine.addImageProvider(QStringLiteral("document"), provider);
    connect(&viewer, &QindaQt::Viewer::ViewerController::frameChanged, &engine,
            [&] { provider->setFrame(viewer.frame()); });
    engine.rootContext()->setContextProperty(QStringLiteral("viewer"), &viewer);
    engine.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);
    engine.load(QUrl::fromLocalFile(QStringLiteral(VIEWER_MAIN_QML)));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window && QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    viewer.open(QUrl::fromLocalFile(ViewerFixtures::textPdf(temp.path())));
    QTRY_VERIFY(viewer.ready() && !viewer.busy()); QTest::qWait(250);
    auto *query = window->findChild<QQuickItem *>(QStringLiteral("viewerFindQuery"));
    auto *area = window->findChild<QQuickItem *>(QStringLiteral("viewerPageText"));
    auto *dialog = window->findChild<QObject *>(QStringLiteral("viewerTextDialog"));
    QVERIFY(query && area && dialog);
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(dialog->property("visible").toBool() && query->hasActiveFocus());
    QCOMPARE(area->property("textFormat").toInt(), 0);
    QVERIFY(area->property("text").toString().contains(QStringLiteral("<b>literal & café</b>")));
    for (int index = 0; index < 12 && !area->hasActiveFocus(); ++index)
        QTest::keyClick(window, Qt::Key_Tab);
    QTRY_VERIFY(area->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
    QGuiApplication::clipboard()->setText(QStringLiteral("before explicit copy"));
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(QGuiApplication::clipboard()->text().contains(QStringLiteral("<b>literal & café</b>")));
    QTest::keyClick(window, Qt::Key_Home, Qt::ControlModifier);
    QCOMPARE(viewer.page(), 0);
    query->forceActiveFocus();
    for (const char key : QByteArray("BETA")) QTest::keyClick(window, key);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(!viewer.searchBusy() && !viewer.busy() && viewer.page() == 1);
    QVERIFY(dialog->property("visible").toBool());
    QTRY_COMPARE(area->property("selectedText").toString(), QStringLiteral("BETA"));
    auto *copy = window->findChild<QQuickItem *>(QStringLiteral("viewerCopyText"));
    QVERIFY(copy && copy->isEnabled());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                     copy->mapToScene(QPointF(copy->width()/2, copy->height()/2)).toPoint());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("BETA"));
    QVERIFY(window->grabWindow().save(QStringLiteral(VIEWER_ARTIFACTS_DIR "/text-normal.png")));
    window->resize(420, 320); QTest::qWait(200);
    for (int index = 0; index < 12; ++index) QTest::keyClick(window, Qt::Key_Tab);
    QVERIFY(window->grabWindow().save(QStringLiteral(VIEWER_ARTIFACTS_DIR "/text-compact.png")));
    const double dialogWidth = dialog->property("width").toDouble();
    const double dialogHeight = dialog->property("height").toDouble();
    QVERIFY(dialogWidth > 0 && dialogWidth <= window->width());
    QVERIFY(dialogHeight > 0 && dialogHeight <= window->height());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QVERIFY(!viewer.searchBusy());
    viewer.close();
    QVERIFY(viewer.pageText().isEmpty());
    QCOMPARE(warnings.size(), 0);
}
QTEST_MAIN(ViewerTextUiTest)
#include "tst_text_ui.moc"
