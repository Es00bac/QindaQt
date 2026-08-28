// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/app_shell/action_registry.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::AppShell;

namespace {

QList<ActionSpec> validActions()
{
    return {{.id = QStringLiteral("file.save"),
             .menuId = QStringLiteral("file"),
             .menuLabel = QStringLiteral("File"),
             .label = QStringLiteral("Save"),
             .accessibleDescription = QStringLiteral("Save the current item"),
             .shortcut = QKeySequence(QKeySequence::Save),
             .menuOrder = 0,
             .order = 20},
            {.id = QStringLiteral("edit.copy"),
             .menuId = QStringLiteral("edit"),
             .menuLabel = QStringLiteral("Edit"),
             .label = QStringLiteral("Copy"),
             .accessibleDescription = QStringLiteral("Copy the selection"),
             .shortcut = QKeySequence(QKeySequence::Copy),
             .menuOrder = 10,
             .order = 10},
            {.id = QStringLiteral("file.new"),
             .menuId = QStringLiteral("file"),
             .menuLabel = QStringLiteral("File"),
             .label = QStringLiteral("New"),
             .accessibleDescription = QStringLiteral("Create a new item"),
             .shortcut = QKeySequence(QKeySequence::New),
             .menuOrder = 0,
             .order = 10}};
}

} // namespace

class ActionRegistryTest final : public QObject {
    Q_OBJECT

private slots:
    void publishesDeterministicMenuSnapshot();
    void rejectsInvalidReplacementAtomically();
    void keepsDomainTruthOutsideActivation();
    void updatesCheckableProjectionOnly();
};

void ActionRegistryTest::publishesDeterministicMenuSnapshot()
{
    ActionRegistry registry;
    QCOMPARE(registry.replaceActions(validActions()).code, ErrorCode::None);

    const QVariantList menus = registry.menus();
    QCOMPARE(menus.size(), 2);
    const QVariantMap fileMenu = menus.at(0).toMap();
    QCOMPARE(fileMenu.value(QStringLiteral("id")).toString(), QStringLiteral("file"));
    const QVariantList fileActions = fileMenu.value(QStringLiteral("actions")).toList();
    QCOMPARE(fileActions.size(), 2);
    QCOMPARE(fileActions.at(0).toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("file.new"));
    QCOMPARE(fileActions.at(1).toMap().value(QStringLiteral("shortcut")).toString(),
             QKeySequence(QKeySequence::Save).toString(QKeySequence::PortableText));
}

void ActionRegistryTest::rejectsInvalidReplacementAtomically()
{
    ActionRegistry registry;
    QCOMPARE(registry.replaceActions(validActions()).code, ErrorCode::None);
    const QVariantList before = registry.menus();
    QSignalSpy changed(&registry, &ActionRegistry::menusChanged);

    QList<ActionSpec> duplicate = validActions();
    duplicate.append(duplicate.first());
    QCOMPARE(registry.replaceActions(duplicate).code, ErrorCode::DuplicateAction);
    QCOMPARE(registry.menus(), before);
    QCOMPARE(changed.count(), 0);

    QList<ActionSpec> hostile = validActions();
    hostile[0].label = QString(MaximumLabelLength + 1, QLatin1Char('x'));
    QCOMPARE(registry.replaceActions(hostile).code, ErrorCode::InvalidArgument);
    QCOMPARE(registry.menus(), before);
}

void ActionRegistryTest::keepsDomainTruthOutsideActivation()
{
    ActionRegistry registry;
    QCOMPARE(registry.replaceActions(validActions()).code, ErrorCode::None);
    QSignalSpy requested(&registry, &ActionRegistry::activationRequested);

    QCOMPARE(registry.requestActivation(QStringLiteral("file.save")).code,
             ErrorCode::None);
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.takeFirst().at(0).toString(), QStringLiteral("file.save"));

    QCOMPARE(registry.setEnabled(QStringLiteral("file.save"), false).code,
             ErrorCode::None);
    QCOMPARE(registry.requestActivation(QStringLiteral("file.save")).code,
             ErrorCode::Unavailable);
    QCOMPARE(requested.count(), 0);
    QCOMPARE(registry.requestActivation(QStringLiteral("missing.action")).code,
             ErrorCode::UnknownAction);
}

void ActionRegistryTest::updatesCheckableProjectionOnly()
{
    ActionRegistry registry;
    QList<ActionSpec> actions = validActions();
    actions[0].checkable = true;
    QCOMPARE(registry.replaceActions(actions).code, ErrorCode::None);
    QCOMPARE(registry.setChecked(QStringLiteral("file.save"), true).code,
             ErrorCode::None);
    const QVariantMap save = registry.menus().first()
                                 .toMap()
                                 .value(QStringLiteral("actions"))
                                 .toList()
                                 .at(1)
                                 .toMap();
    QVERIFY(save.value(QStringLiteral("checked")).toBool());
    QCOMPARE(registry.setChecked(QStringLiteral("edit.copy"), true).code,
             ErrorCode::InvalidArgument);
}

QTEST_MAIN(ActionRegistryTest)
#include "tst_action_registry.moc"
