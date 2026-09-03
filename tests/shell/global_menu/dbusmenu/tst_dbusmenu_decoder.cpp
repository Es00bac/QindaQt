// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/shell/global_menu/dbusmenu/dbusmenu_decoder.h>
#include <qindaqt/shell/global_menu/protocol/menu_limits.h>

#include "fake_dbusmenu_exporter.h"

#include <QtTest/QTest>

using namespace QindaQt::Shell::GlobalMenu;

class DbusMenuDecoderTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void convertsCanonicalPropertiesAndIgnoresUnknowns();
    void rejectsHostileBoundsAndMalformedKnownProperties();
    void validatesPropertySignalBounds();
};

void DbusMenuDecoderTest::convertsCanonicalPropertiesAndIgnoresUnknowns()
{
    DbusMenu::LayoutItem root = Test::menuLayout();
    auto action = root.children.first().value<DbusMenu::LayoutItem>();
    action.properties.insert(QStringLiteral("future-property"), QVariantList{1, 2, 3});
    action.properties.insert(QStringLiteral("icon-name"), QStringLiteral("document-open"));
    root.children[0] = QVariant::fromValue(action);

    const QUuid owner = QUuid::createUuid();
    const DbusMenu::DecodeResult result = DbusMenu::decodeLayout(owner, 9, root);
    QVERIFY(result.accepted);
    QCOMPARE(result.snapshot.tree.ownerWindowId, owner);
    QCOMPARE(result.snapshot.tree.revision, quint64{9});
    QCOMPARE(result.snapshot.tree.items.size(), 2);
    QCOMPARE(result.snapshot.tree.items.first().text, QStringLiteral("File"));
    QCOMPARE(result.snapshot.tree.items.first().mnemonicIndex, 0);
    QCOMPARE(result.snapshot.tree.items.first().shortcutText, QStringLiteral("Ctrl+O"));
    QCOMPARE(result.snapshot.tree.items.at(1).kind, Protocol::MenuItemKind::Submenu);
}

void DbusMenuDecoderTest::rejectsHostileBoundsAndMalformedKnownProperties()
{
    const QUuid owner = QUuid::createUuid();
    DbusMenu::LayoutItem root = Test::menuLayout();
    auto action = root.children.first().value<DbusMenu::LayoutItem>();
    action.properties[QStringLiteral("icon-data")] = QByteArray(DbusMenu::kMaxIconDataBytes + 1, 'x');
    root.children[0] = QVariant::fromValue(action);
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).accepted, false);

    root = Test::menuLayout();
    action = root.children.first().value<DbusMenu::LayoutItem>();
    for (qsizetype index = 0; index <= DbusMenu::kMaxPropertiesPerItem; ++index) {
        action.properties.insert(QStringLiteral("unknown-%1").arg(index), index);
    }
    root.children[0] = QVariant::fromValue(action);
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).reasonCode,
             QStringLiteral("too-many-item-properties"));

    root = Test::menuLayout();
    action = root.children.first().value<DbusMenu::LayoutItem>();
    action.properties[QStringLiteral("enabled")] = QStringLiteral("yes");
    root.children[0] = QVariant::fromValue(action);
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).reasonCode,
             QStringLiteral("invalid-item-properties"));

    root = Test::menuLayout();
    action = root.children.first().value<DbusMenu::LayoutItem>();
    action.properties[QStringLiteral("icon-name")] =
        QString(DbusMenu::kMaxIconNameUtf8Bytes + 1, u'x');
    root.children[0] = QVariant::fromValue(action);
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).accepted, false);

    DbusMenu::LayoutItem nested{.id = 100,
                                .properties = {{QStringLiteral("label"), QStringLiteral("leaf")}},
                                .children = {}};
    for (int depth = 0; depth <= Protocol::kMaxDepth; ++depth) {
        nested = DbusMenu::LayoutItem{
            .id = 101 + depth,
            .properties = {{QStringLiteral("label"), QStringLiteral("nested")},
                           {QStringLiteral("children-display"), QStringLiteral("submenu")}},
            .children = {QVariant::fromValue(nested)}};
    }
    root = {.id = 0, .properties = {}, .children = {QVariant::fromValue(nested)}};
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).reasonCode, QStringLiteral("too-deep"));

    root = Test::menuLayout(QString(DbusMenu::kMaxIconNameUtf8Bytes + 600, u'x'));
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).accepted, false);

    root = {.id = 0, .properties = {}, .children = {}};
    for (int index = 0; index <= Protocol::kMaxChildrenPerItem; ++index) {
        root.children.append(QVariant::fromValue(DbusMenu::LayoutItem{
            .id = index + 1,
            .properties = {{QStringLiteral("label"), QStringLiteral("bounded")}},
            .children = {}}));
    }
    QCOMPARE(DbusMenu::decodeLayout(owner, 1, root).reasonCode, QStringLiteral("invalid-root"));
}

void DbusMenuDecoderTest::validatesPropertySignalBounds()
{
    QString reason;
    QVERIFY(DbusMenu::validatePropertyUpdates(
        {{.id = 1, .properties = {{QStringLiteral("unknown"), 5}}}}, {}, &reason));
    QVERIFY(reason.isEmpty());
    QVERIFY(!DbusMenu::validatePropertyUpdates(
        {{.id = 0, .properties = {{QStringLiteral("enabled"), true}}}}, {}, &reason));
    QCOMPARE(reason, QStringLiteral("invalid-property-update"));
    DbusMenu::RemovedPropertyEntry removed{.id = 1, .names = {}};
    removed.names.fill(QStringLiteral("label"), DbusMenu::kMaxRemovedPropertyNames + 1);
    QVERIFY(!DbusMenu::validatePropertyUpdates({}, {removed}, &reason));
}

QTEST_GUILESS_MAIN(DbusMenuDecoderTest)

#include "tst_dbusmenu_decoder.moc"
