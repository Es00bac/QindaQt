// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop_surface_qml_test_support.h"

#include <QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_DesktopSurfacePlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Tests::DesktopSurface;

// Settings-labeled Desktop context-menu entries must dispatch the
// org.qindaqt.Settings desktop action their label promises (Appearance or
// Display route), while entries without an action keep the primary launch.
// Split from tst_desktop_surface_qml.cpp by failure mode: this row fails only
// on route-action wiring regressions.
class DesktopContextMenuActionTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void settingsEntriesDispatchRouteActions();
};

void DesktopContextMenuActionTests::settingsEntriesDispatchRouteActions()
{
    StubPlaces places;
    StubDesktopControlsAccess access(&places);
    StubLauncher launcher;
    SurfaceHost host;
    QString error;
    QVERIFY2(host.create(&access, &launcher,
                         {{QStringLiteral("contextMenuStyle"),
                           QStringLiteral("windows")}},
                         &error),
             qPrintable(error));
    QTRY_VERIFY(host.window->isExposed());

    auto *menu = host.child<QObject>(QStringLiteral("desktopContextMenu"));
    QVERIFY(menu != nullptr);

    const struct {
        const char *style;
        const char *itemName;
        const char *entryId;
        const char *actionId;
    } cases[] = {
        {"windows", "desktopContextDisplayProperties", "org.qindaqt.Settings",
         "display"},
        {"mac", "desktopContextScreenSaver", "org.qindaqt.Settings",
         "appearance"},
        {"traditional", "desktopContextSettings", "org.qindaqt.Settings",
         "appearance"},
        {"traditional", "desktopContextTerminal", "org.qindaqt.Terminal", ""},
    };

    for (const auto &testCase : cases) {
        QVERIFY(host.window->setProperty(
            "applets", makeApplets({{QStringLiteral("contextMenuStyle"),
                                     QLatin1String(testCase.style)}})));

        // The descriptor is the contract: the entry the label promises must
        // name the Settings entry and the desktop action that opens that
        // route.
        const QVariantList entries = menu->property("entries").toList();
        bool descriptorFound = false;
        for (const QVariant &entry : entries) {
            const QVariantMap map = entry.toMap();
            if (map.value(QStringLiteral("objectName"))
                != QLatin1String(testCase.itemName))
                continue;
            descriptorFound = true;
            QCOMPARE(map.value(QStringLiteral("kind")).toString(),
                     QStringLiteral("launch"));
            QCOMPARE(map.value(QStringLiteral("targetId")).toString(),
                     QLatin1String(testCase.entryId));
            QCOMPARE(map.value(QStringLiteral("actionId")).toString(),
                     QLatin1String(testCase.actionId));
        }
        QVERIFY(descriptorFound);

        // Drive the row's triggered signal — exactly what a pointer release
        // inside the row emits — and require the launcher facade to receive
        // both the entry id and the route action id.
        // AGENT-NOTE: the delegate cannot be clicked through its window here:
        // QQC2 evicts Menu items from the content model while the popup is
        // hidden (see DesktopContextMenu.qml's AGENT-NOTE), so in this
        // offscreen harness the styled menu hosts no scene-attached rows; no
        // existing context-menu row is pointer-tested for the same reason.
        const auto items = host.visualItemsNamed(testCase.itemName);
        QCOMPARE(items.size(), 1);
        QVERIFY(items.constFirst()->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(items.constFirst(), "triggered"));
        QCOMPARE(launcher.activated.size(), 1);
        QCOMPARE(launcher.activated.constFirst(),
                 QLatin1String(testCase.entryId));
        QCOMPARE(launcher.activatedActions.constFirst(),
                 QLatin1String(testCase.actionId));
        launcher.activated.clear();
        launcher.activatedActions.clear();
    }
}

QTEST_MAIN(DesktopContextMenuActionTests)

#include "tst_desktop_context_menu_actions.moc"
