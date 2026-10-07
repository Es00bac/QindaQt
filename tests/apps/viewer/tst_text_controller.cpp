// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewer_controller.h"
#include "fixtures.h"
#include <QTest>
using namespace QindaQt::Viewer;

class ViewerTextControllerTest final : public QObject {
    Q_OBJECT
private slots:
    void findNavigateContinueAndCancel() {
        QTemporaryDir temp; ViewerController controller;
        controller.open(QUrl::fromLocalFile(ViewerFixtures::textPdf(temp.path())));
        QTRY_VERIFY(controller.ready() && !controller.busy());
        controller.find(QStringLiteral("BETA"), false, true);
        QTRY_VERIFY(!controller.searchBusy() && !controller.busy());
        QCOMPARE(controller.page(), 1);
        QVERIFY(controller.matchReady());
        QCOMPARE(controller.pageText().mid(controller.matchStart(), controller.matchLength()),
                 QStringLiteral("BETA"));
        controller.find(QStringLiteral("Alpha"), false, true);
        QTRY_VERIFY(!controller.searchBusy() && !controller.busy());
        QCOMPARE(controller.page(), 1);
        controller.find(QStringLiteral("Alpha"), false, true);
        QTRY_VERIFY(!controller.searchBusy() && !controller.busy());
        QCOMPARE(controller.page(), 0);
        QVERIFY(controller.matchReady());
        controller.cancelSearch();
        QVERIFY(!controller.searchBusy() && !controller.matchReady());
        QVERIFY(controller.searchMessage().isEmpty());
        controller.find(QStringLiteral("absent"), false, false);
        QTRY_VERIFY(!controller.searchBusy());
        QVERIFY(!controller.matchReady());
        QVERIFY(controller.searchMessage().contains(QStringLiteral("No matches")));
    }
    void closeReplaceAndManualNavigationFenceDisclosure() {
        QTemporaryDir temp; ViewerController controller;
        const auto pdf = QUrl::fromLocalFile(ViewerFixtures::textPdf(temp.path()));
        controller.open(pdf); QTRY_VERIFY(controller.ready() && !controller.busy());
        controller.find(QStringLiteral("BETA"), false, false);
        controller.close();
        QTest::qWait(100);
        QVERIFY(controller.pageText().isEmpty() && controller.searchMessage().isEmpty());
        QVERIFY(!controller.ready() && !controller.searchBusy());
        controller.open(pdf); QTRY_VERIFY(controller.ready() && !controller.busy());
        controller.find(QStringLiteral("BETA"), false, false);
        controller.open(QUrl::fromLocalFile(ViewerFixtures::image(temp.path())));
        QTRY_VERIFY(controller.ready() && !controller.busy());
        QVERIFY(!controller.pdf() && !controller.textAllowed());
        QVERIFY(controller.pageText().isEmpty() && controller.searchMessage().isEmpty());
        controller.open(pdf); QTRY_VERIFY(controller.ready() && !controller.busy());
        controller.find(QStringLiteral("Alpha"), false, false);
        controller.goToPage(1);
        QTRY_VERIFY(!controller.busy() && !controller.searchBusy());
        QCOMPARE(controller.page(), 1);
        QVERIFY(!controller.matchReady() && controller.searchMessage().isEmpty());
    }
    void singleflightAndInvalidQuery() {
        QTemporaryDir temp; ViewerController controller;
        controller.open(QUrl::fromLocalFile(ViewerFixtures::textPdf(temp.path())));
        QTRY_VERIFY(controller.ready() && !controller.busy());
        controller.find(QStringLiteral("Alpha"), false, false);
        QVERIFY(controller.searchBusy());
        controller.find(QStringLiteral("BETA"), false, true);
        QTRY_VERIFY(!controller.searchBusy());
        QCOMPARE(controller.page(), 0);
        QVERIFY(controller.matchReady());
        controller.find(QString(513, QLatin1Char('a')), false, false);
        QVERIFY(!controller.searchBusy() && !controller.matchReady());
        QVERIFY(controller.searchMessage().contains(QStringLiteral("512")));
    }
};
QTEST_MAIN(ViewerTextControllerTest)
#include "tst_text_controller.moc"
