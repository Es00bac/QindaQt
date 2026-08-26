// SPDX-License-Identifier: GPL-3.0-or-later
#include "layoutgeometry.h"

#include <QTest>

using namespace QindaQt;

class LayoutGeometryTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsEveryPageInsideOneOuterFrame();
};

void LayoutGeometryTest::keepsEveryPageInsideOneOuterFrame()
{
    Core::WindowContainer container(QStringLiteral("container"));
    QString error;
    QVERIFY(container.addPage(QStringLiteral("page-main"), QStringLiteral("leaf-a"),
                              QStringLiteral("window-a"), &error));
    QVERIFY(container.splitWindow({QStringLiteral("window-a"),
                                   QStringLiteral("window-b"),
                                   QStringLiteral("leaf-b"),
                                   QStringLiteral("split-main"),
                                   Core::SplitOrientation::Horizontal,
                                   0.5,
                                   Core::InsertPosition::Second},
                                  &error));
    QVERIFY(container.addPage(QStringLiteral("page-third"), QStringLiteral("leaf-c"),
                              QStringLiteral("window-c"), &error));

    const QRectF outer(50.0, 60.0, 800.0, 600.0);
    const auto first = Compositor::KWinIntegration::LayoutGeometryPlanner::plan(
        container, outer);
    QCOMPARE(first.visibleWindows,
             QSet<QString>({QStringLiteral("window-a"), QStringLiteral("window-b")}));
    QCOMPARE(first.frames.value(QStringLiteral("window-c")), outer);
    QCOMPARE(first.frames.value(QStringLiteral("window-a")),
             QRectF(50.0, 60.0, 400.0, 600.0));
    QCOMPARE(first.frames.value(QStringLiteral("window-b")),
             QRectF(450.0, 60.0, 400.0, 600.0));

    QVERIFY(container.activatePage(QStringLiteral("page-third"), &error));
    QCOMPARE(Compositor::KWinIntegration::LayoutGeometryPlanner::activeWindowIds(container),
             QSet<QString>({QStringLiteral("window-c")}));
    const auto second = Compositor::KWinIntegration::LayoutGeometryPlanner::plan(
        container, outer);
    QCOMPARE(second.visibleWindows, QSet<QString>({QStringLiteral("window-c")}));
    QCOMPARE(second.frames.value(QStringLiteral("window-c")), outer);
}

QTEST_GUILESS_MAIN(LayoutGeometryTest)
#include "tst_layoutgeometry.moc"
