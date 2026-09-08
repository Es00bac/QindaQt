// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h>
#include "status_notifier_applet_test_fakes.h"
#include <QtTest>

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifierApplet;
using namespace QindaQt::StatusNotifierApplet::Tests;

namespace {
OwnerKey stage(FakeStatusNotifierSource &source)
{
    const OwnerKey key {QStringLiteral(":1.42"), QStringLiteral("/StatusNotifierItem"), 3};
    source.m_presentation.state = PresentationState::Ready;
    source.m_presentation.items = {makePresentationItem(QStringLiteral("fixture"), key.uniqueName,
        key.objectPath, key.generation, QStringLiteral("Fixture"), QStringLiteral("active"))};
    ItemDescriptor descriptor;
    descriptor.identity = QStringLiteral("fixture");
    descriptor.title = QStringLiteral("Fixture");
    source.m_descriptors = {descriptor};
    source.m_generations[key.uniqueName] = key.generation;
    source.m_itemIsMenu = true;
    source.m_exportedMenu = true;
    source.m_menuState = {{QStringLiteral("status"), QStringLiteral("ready")}};
    return key;
}
}

class TrayMenuControllerTests final : public QObject {
    Q_OBJECT
private slots:
    void preservesCapturedRevisionCoordinatesAndScroll()
    {
        FakeStatusNotifierSource source;
        const auto key = stage(source);
        StatusNotifierAppletController controller(&source, true, true);
        QVERIFY(controller.itemIsMenu(key.uniqueName, key.objectPath, key.generation));
        QVERIFY(controller.hasExportedMenu(key.uniqueName, key.objectPath, key.generation));
        QVERIFY(controller.activateItem(key.uniqueName, key.objectPath, key.generation, -540, 1080));
        QCOMPARE(source.m_calls.last().x, -540);
        QCOMPARE(source.m_calls.last().y, 1080);
        QVERIFY(controller.openContextMenu(key.uniqueName, key.objectPath, key.generation, -100, 40));
        QCOMPARE(source.m_calls.last().kind, QStringLiteral("contextMenu"));
        QCOMPARE(source.m_calls.last().x, -100);
        QVERIFY(controller.invokeMenu(key.uniqueName, key.objectPath, key.generation,
                                      QStringLiteral("9007199254740993"), 4));
        QCOMPARE(source.m_lastRevision, quint64(9007199254740993ULL));
        QCOMPARE(source.m_lastMenuId, 4);
        QVERIFY(controller.aboutToShowMenu(key.uniqueName, key.objectPath, key.generation,
                                           QStringLiteral("9007199254740993"), 8));
        QCOMPARE(source.m_calls.last().kind, QStringLiteral("aboutToShowMenu"));
        QVERIFY(controller.scrollItem(key.uniqueName, key.objectPath, key.generation,
                                      -120, QStringLiteral("horizontal")));
        QCOMPARE(source.m_calls.last().x, -120);
        QCOMPARE(source.m_scrollOrientation, QStringLiteral("horizontal"));
    }
    void refusesStaleHiddenAndMalformedAuthority()
    {
        FakeStatusNotifierSource source;
        const auto key = stage(source);
        StatusNotifierAppletController controller(&source, true, true);
        for (const auto &serial : {QStringLiteral("0"), QStringLiteral("01"), QStringLiteral("-1"),
                                  QStringLiteral("1e3"), QStringLiteral("18446744073709551616")}) {
            QVERIFY(!controller.invokeMenu(key.uniqueName, key.objectPath, key.generation, serial, 4));
        }
        source.m_generations[key.uniqueName] = 4;
        QVERIFY(!controller.invokeMenu(key.uniqueName, key.objectPath, 3, QStringLiteral("1"), 4));
        QVERIFY(!controller.hasExportedMenu(key.uniqueName, key.objectPath, 3));
        QVERIFY(source.m_calls.isEmpty());
        QCOMPARE(source.m_menuReads, 0);
    }
    void deniedCapabilitiesWithholdMenuObservationAndDispatch()
    {
        FakeStatusNotifierSource source;
        const auto key = stage(source);
        StatusNotifierAppletController deniedRead(&source, false, true);
        QVERIFY(!deniedRead.itemIsMenu(key.uniqueName, key.objectPath, 3));
        QVERIFY(!deniedRead.hasExportedMenu(key.uniqueName, key.objectPath, 3));
        QCOMPARE(deniedRead.menuStateFor(key.uniqueName, key.objectPath, 3).value(QStringLiteral("status")),
                 QVariant(QStringLiteral("none")));
        QVERIFY(!deniedRead.invokeMenu(key.uniqueName, key.objectPath, 3, QStringLiteral("1"), 4));
        QCOMPARE(source.m_menuReads, 0);
        StatusNotifierAppletController deniedActivate(&source, true, false);
        QVERIFY(!deniedActivate.invokeMenu(key.uniqueName, key.objectPath, 3, QStringLiteral("1"), 4));
        QVERIFY(!deniedActivate.aboutToShowMenu(key.uniqueName, key.objectPath, 3, QStringLiteral("1"), 8));
        QVERIFY(!deniedActivate.scrollItem(key.uniqueName, key.objectPath, 3, 120, QStringLiteral("vertical")));
        QVERIFY(source.m_calls.isEmpty());
    }
    void menuRefreshDoesNotRecreateTrayItems()
    {
        FakeStatusNotifierSource source;
        stage(source);
        StatusNotifierAppletController controller(&source, true, true);
        QSignalSpy rows(&controller, &StatusNotifierAppletController::stateReprojected);
        QSignalSpy menus(&controller, &StatusNotifierAppletController::menuChanged);
        const int renders = source.m_renderCalls;
        Q_EMIT source.menuChanged();
        QCOMPARE(menus.count(), 1);
        QCOMPARE(rows.count(), 0);
        QCOMPARE(source.m_renderCalls, renders);
    }
};
QTEST_MAIN(TrayMenuControllerTests)
#include "tst_status_notifier_applet_menu.moc"
