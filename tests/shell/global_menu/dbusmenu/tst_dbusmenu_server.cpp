// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;

namespace
{

Protocol::MenuTree menuTree()
{
    return {.ownerWindowId = {},
            .epoch = {},
            .revision = 0,
            .items = {{.id = QStringLiteral("menu.file"),
                       .kind = Protocol::MenuItemKind::Submenu,
                       .text = QStringLiteral("File"),
                       .children = {{.id = QStringLiteral("file.open"),
                                     .kind = Protocol::MenuItemKind::Action,
                                     .text = QStringLiteral("Open"),
                                     .shortcutText = QStringLiteral("Ctrl+O")}}}}};
}

DbusMenu::LayoutItem childAt(const DbusMenu::LayoutItem &parent, qsizetype index)
{
    return parent.children.at(index).value<DbusMenu::LayoutItem>();
}

} // namespace

class DbusMenuServerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void implementsFilteredV4SurfaceAndGroupedCalls();
    void rejectedSnapshotPreservesPublishedTruth();
};

void DbusMenuServerTest::implementsFilteredV4SurfaceAndGroupedCalls()
{
    // AGENT-NOTE: P1-03 regression proof. This accepted transport-owned
    // server must implement the complete v4 surface; an AppShell-local subset
    // cannot substitute for it.
    DbusMenu::DbusMenuServer server;
    QVERIFY(server.publish(menuTree()));
    QCOMPARE(server.version(), quint32{4});

    quint32 revision = 0;
    DbusMenu::LayoutItem root;
    server.GetLayout(0, 1, {QStringLiteral("label")}, revision, root);
    QCOMPARE(revision, quint32{1});
    QCOMPARE(root.children.size(), 1);
    const DbusMenu::LayoutItem submenu = childAt(root, 0);
    const QVariantMap expectedSubmenuProperties{
        {QStringLiteral("label"), QStringLiteral("File")}};
    QCOMPARE(submenu.properties, expectedSubmenuProperties);
    QVERIFY(submenu.children.isEmpty());

    DbusMenu::LayoutItem completeRoot;
    server.GetLayout(0, -1, {}, revision, completeRoot);
    const DbusMenu::LayoutItem completeSubmenu = childAt(completeRoot, 0);
    const DbusMenu::LayoutItem action = childAt(completeSubmenu, 0);
    QCOMPARE(server.GetProperty(action.id, QStringLiteral("label")).variant(),
             QVariant(QStringLiteral("Open")));
    const DbusMenu::PropertyEntryList group =
        server.GetGroupProperties({action.id}, {QStringLiteral("enabled")});
    QCOMPARE(group.size(), 1);
    QCOMPARE(group.constFirst().id, action.id);
    const QVariantMap expectedActionProperties{{QStringLiteral("enabled"), true}};
    QCOMPARE(group.constFirst().properties, expectedActionProperties);

    QSignalSpy activated(&server, &DbusMenu::DbusMenuServer::actionActivated);
    const QList<int> eventErrors = server.EventGroup(
        {{.id = action.id,
          .eventId = QStringLiteral("clicked"),
          .data = QDBusVariant{},
          .timestamp = 0},
         {.id = 999,
          .eventId = QStringLiteral("clicked"),
          .data = QDBusVariant{},
          .timestamp = 0}});
    QCOMPARE(eventErrors, QList<int>{999});
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.constFirst().constFirst().toString(),
             QStringLiteral("file.open"));

    QList<int> showErrors;
    QCOMPARE(server.AboutToShowGroup({completeSubmenu.id, 999}, showErrors),
             QList<int>{});
    QCOMPARE(showErrors, QList<int>{999});
}

void DbusMenuServerTest::rejectedSnapshotPreservesPublishedTruth()
{
    DbusMenu::DbusMenuServer server;
    QVERIFY(server.publish(menuTree()));
    Protocol::MenuTree malformed = menuTree();
    malformed.items.first().children.first().checked = true;
    QVERIFY(!server.publish(malformed));
    QCOMPARE(server.revision(), quint32{1});

    quint32 revision = 0;
    DbusMenu::LayoutItem root;
    server.GetLayout(0, -1, {}, revision, root);
    QCOMPARE(childAt(childAt(root, 0), 0).properties.value(QStringLiteral("label")),
             QVariant(QStringLiteral("Open")));
}

QTEST_GUILESS_MAIN(DbusMenuServerTest)

#include "tst_dbusmenu_server.moc"
