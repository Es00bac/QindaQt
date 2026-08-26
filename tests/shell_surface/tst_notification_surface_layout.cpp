// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_surface/notification_surface_layout.h"

#include <QtTest>

using namespace QindaQt;

class NotificationSurfaceLayoutTests final : public QObject {
    Q_OBJECT

private slots:
    void preservesPreferredSizesAcrossReferenceOutputs_data();
    void preservesPreferredSizesAcrossReferenceOutputs();
    void clampsToHighDpiLogicalGeometry();
    void plansHeaderOnlyFeedbackWithoutPopups();
    void rejectsOutputsThatCannotPresentUsableControls();
};

void NotificationSurfaceLayoutTests::
    preservesPreferredSizesAcrossReferenceOutputs_data()
{
    QTest::addColumn<QSize>("logicalSize");
    QTest::newRow("1080p") << QSize(1'920, 1'080);
    QTest::newRow("wuxga") << QSize(1'920, 1'200);
    QTest::newRow("1440p") << QSize(2'560, 1'440);
}

void NotificationSurfaceLayoutTests::
    preservesPreferredSizesAcrossReferenceOutputs()
{
    QFETCH(QSize, logicalSize);
    const auto result = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        logicalSize, {0, 16, 16, 0}, {0, 16, 16, 0}, 3);
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.layout->popupSize, QSize(400, 476));
    QCOMPARE(result.layout->centerSize, QSize(440, 640));
    QCOMPARE(result.layout->visiblePopupCount, 3);
}

void NotificationSurfaceLayoutTests::clampsToHighDpiLogicalGeometry()
{
    // A 1920x1080 mode at 200% scaling is 960x540 logical pixels to Qt.
    const auto result = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        {960, 540}, {0, 16, 16, 0}, {0, 16, 16, 0}, 8);
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.layout->popupSize, QSize(400, 476));
    QCOMPARE(result.layout->centerSize, QSize(440, 524));
    QCOMPARE(result.layout->visiblePopupCount, 3);

    const auto compact = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        {400, 300}, {0, 16, 16, 0}, {0, 16, 16, 0}, 3);
    QVERIFY2(compact.ok(), qPrintable(compact.error));
    QCOMPARE(compact.layout->popupSize, QSize(384, 284));
    QCOMPARE(compact.layout->centerSize, QSize(384, 284));
}

void NotificationSurfaceLayoutTests::plansHeaderOnlyFeedbackWithoutPopups()
{
    const auto result = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        {1'920, 1'080}, {0, 16, 16, 0}, {0, 16, 16, 0}, 0);
    QVERIFY2(result.ok(), qPrintable(result.error));
    QCOMPARE(result.layout->popupSize, QSize(400, 38));
    QCOMPARE(result.layout->centerSize, QSize(440, 640));
    QCOMPARE(result.layout->visiblePopupCount, 0);
}

void NotificationSurfaceLayoutTests::
    rejectsOutputsThatCannotPresentUsableControls()
{
    const auto result = ShellSurface::NotificationSurfaceLayoutPlanner::plan(
        {240, 220}, {0, 16, 16, 0}, {0, 16, 16, 0}, 1);
    QVERIFY(!result.ok());
    QVERIFY(!result.error.isEmpty());
}

QTEST_GUILESS_MAIN(NotificationSurfaceLayoutTests)

#include "tst_notification_surface_layout.moc"
