// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtGui/QWindow>
#include <QtTest/QtTest>

#include "nativepopupplacement.h"

using QindaQt::Shell::GlobalMenu::NativePopupPlacement;

// Pins the native mechanism the offscreen QML rows cannot prove: the faux
// QtWayland positioner contract must exist as REAL QObject dynamic properties
// on the popup window, readable through QObject::property() exactly as
// QWaylandXdgSurface::createPositioner() reads them. Assigning the same names
// from QML only creates a JavaScript wrapper expando, invisible to QtWayland
// — the ef0941e4 failure mode.
class NativePopupPlacementTests : public QObject
{
    Q_OBJECT

private slots:
    void writesFauxPositionerContractAsDynamicProperties();
    void failClosedOnNullWindowOrInvalidRect();
};

void NativePopupPlacementTests::writesFauxPositionerContractAsDynamicProperties()
{
    QWindow window;
    const QRectF anchorSceneRect(36.4, 6.2, 32.0, 24.0);

    const NativePopupPlacement placement;
    placement.configurePopupWindow(&window, anchorSceneRect);

    QCOMPARE(window.property("_q_waylandPopupAnchorRect").toRect(),
             anchorSceneRect.toAlignedRect());
    QCOMPARE(window.property("_q_waylandPopupAnchor").value<Qt::Edges>(),
             Qt::Edges(Qt::BottomEdge | Qt::LeftEdge));
    QCOMPARE(window.property("_q_waylandPopupGravity").value<Qt::Edges>(),
             Qt::Edges(Qt::BottomEdge | Qt::RightEdge));
    // slide_x | slide_y | flip_y, mirroring QtWayland's Menu default.
    QCOMPARE(window.property("_q_waylandPopupConstraintAdjustment").toUInt(),
             quint32(11));

    const QList<QByteArray> dynamicNames = window.dynamicPropertyNames();
    QVERIFY(dynamicNames.contains("_q_waylandPopupAnchorRect"));
    QVERIFY(dynamicNames.contains("_q_waylandPopupAnchor"));
    QVERIFY(dynamicNames.contains("_q_waylandPopupGravity"));
    QVERIFY(dynamicNames.contains("_q_waylandPopupConstraintAdjustment"));
}

void NativePopupPlacementTests::failClosedOnNullWindowOrInvalidRect()
{
    const NativePopupPlacement placement;
    placement.configurePopupWindow(nullptr, QRectF(0, 0, 10, 10));

    QWindow window;
    placement.configurePopupWindow(&window, QRectF());
    QVERIFY(window.dynamicPropertyNames().isEmpty());
}

QTEST_MAIN(NativePopupPlacementTests)
#include "tst_nativepopupplacement.moc"
