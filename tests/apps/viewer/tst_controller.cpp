// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewer_controller.h"
#include "fixtures.h"
#include <QSignalSpy>
#include <QTest>
#include <limits>

using namespace QindaQt::Viewer;
class ControllerTest final : public QObject {
    Q_OBJECT
private slots:
    void localPathsAndUrls();
    void navigationRotationAndClose();
    void latestOpenWins();
    void failedOpenAndUnlock();
};

void ControllerTest::localPathsAndUrls()
{
    QTemporaryDir temp;
    const QString path = ViewerFixtures::image(temp.path());
    QCOMPARE(ViewerController::localArgument(path), QUrl::fromLocalFile(path));
    QCOMPARE(ViewerController::localArgument(QUrl::fromLocalFile(path).toString()), QUrl::fromLocalFile(path));
    ViewerController controller;
    controller.open(ViewerController::localArgument(path));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.ready());
    controller.open(QUrl(QStringLiteral("https://example.com/picture.png")));
    QVERIFY(!controller.ready());
    QVERIFY(controller.error().contains(QStringLiteral("local")));
    controller.open(QUrl(QStringLiteral("file://remote/picture.png")));
    QVERIFY(!controller.ready());
}

void ControllerTest::navigationRotationAndClose()
{
    QTemporaryDir temp;
    ViewerController controller;
    QSignalSpy opened(&controller, &ViewerController::opened);
    controller.open(QUrl::fromLocalFile(ViewerFixtures::pdf(temp.path())));
    QTRY_COMPARE(opened.size(), 1);
    QCOMPARE(controller.pageCount(), 2);
    controller.goToPage(1);
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.frame().pixelColor(10, 10), QColor(Qt::blue));
    controller.goToPage(9);
    QCOMPARE(controller.page(), 1);
    controller.rotate(90);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.pageSize().height() > controller.pageSize().width());
    const quint64 revision = controller.frameRevision();
    controller.renderAt(std::numeric_limits<double>::quiet_NaN(), 1);
    QCOMPARE(controller.frameRevision(), revision);
    controller.renderAt(10000, 10000);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.frame().width() <= DocumentRenderer::MaxDimension);
    controller.close();
    QVERIFY(!controller.ready());
    QVERIFY(controller.frame().isNull());
    QVERIFY(controller.fileName().isEmpty());
}

void ControllerTest::latestOpenWins()
{
    QTemporaryDir temp;
    const QString pdf = ViewerFixtures::pdf(temp.path());
    const QString image = ViewerFixtures::image(temp.path());
    ViewerController controller;
    controller.open(QUrl::fromLocalFile(pdf));
    controller.open(QUrl::fromLocalFile(image));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.pageCount(), 1);
    QCOMPARE(controller.pageSize(), QSizeF(120, 80));
    controller.open(QUrl::fromLocalFile(pdf));
    controller.close();
    QTest::qWait(100);
    QVERIFY(!controller.ready());
    QVERIFY(controller.frame().isNull());
}

void ControllerTest::failedOpenAndUnlock()
{
    ViewerController controller;
    controller.open(QUrl::fromLocalFile(QStringLiteral("/not-present/viewer.pdf")));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.error().isEmpty());
    QSignalSpy password(&controller, &ViewerController::passwordRequired);
    controller.open(QUrl::fromLocalFile(QStringLiteral(VIEWER_FIXTURES_DIR "/password.pdf")));
    QTRY_COMPARE(password.size(), 1);
    QVERIFY(controller.locked());
    controller.unlock(QStringLiteral("bad"));
    QTRY_COMPARE(password.size(), 2);
    QVERIFY(controller.locked());
    controller.unlock(QStringLiteral("viewer-test"));
    QTRY_VERIFY(controller.ready());
    QVERIFY(!controller.locked());
    QVERIFY(controller.error().isEmpty());
}
QTEST_MAIN(ControllerTest)
#include "tst_controller.moc"
