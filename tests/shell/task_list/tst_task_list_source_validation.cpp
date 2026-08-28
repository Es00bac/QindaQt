// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

const QString kAppId = QStringLiteral("app.editor");

class TaskListSourceValidationTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptedSeedGenerationSurvivesLaterHostilePublishes()
    {
        TaskListSource source;
        const auto seed = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(seed.ok());

        auto orphaned = TaskListTest::member(QStringLiteral("w-orphan"),
                                             QStringLiteral("c-none"));
        const auto rejected = source.publishGeneration({orphaned});
        QCOMPARE(rejected.ok(), false);
        QCOMPARE(rejected.error.code,
                 TaskListErrorCode::OrphanContainerMember);

        // AGENT-GUARD: A rejected batch must leave the seed generation and
        // revision untouched; presentation caches exactly this state.
        QCOMPARE(source.status(), TaskListSourceStatus::Ready);
        QCOMPARE(source.revision(), quint64(1));
        QCOMPARE(source.generation().entries.size(), 1);
        QCOMPARE(source.generation().entries.first().taskId,
                 QStringLiteral("w-1"));
    }

    void identityShapeIsEnforced()
    {
        TaskListSource source;
        auto emptyWindow = TaskListTest::standalone(QString(), kAppId);
        QCOMPARE(source.publishGeneration({emptyWindow}).error.code,
                 TaskListErrorCode::InvalidIdentity);

        auto emptyApp =
            TaskListTest::standalone(QStringLiteral("w-1"), QString());
        QCOMPARE(source.publishGeneration({emptyApp}).error.code,
                 TaskListErrorCode::InvalidIdentity);

        auto emptyName =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        emptyName.applicationName.clear();
        QCOMPARE(source.publishGeneration({emptyName}).error.code,
                 TaskListErrorCode::InvalidIdentity);

        auto noOutput = TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        noOutput.outputId.clear();
        QCOMPARE(source.publishGeneration({noOutput}).error.code,
                 TaskListErrorCode::InvalidIdentity);

        auto oversized = TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        oversized.title = QString(int(kMaxIdLength) + 1, QLatin1Char('x'));
        QCOMPARE(source.publishGeneration({oversized}).error.code,
                 TaskListErrorCode::LimitExceeded);

        auto emptyWorkspace =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        emptyWorkspace.workspaceIds = {QString()};
        QCOMPARE(source.publishGeneration({emptyWorkspace}).error.code,
                 TaskListErrorCode::InvalidWorkspaceScope);
    }

    void workspaceScopeIsEnforced()
    {
        TaskListSource source;
        auto noScope = TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        noScope.workspaceIds.clear();
        QCOMPARE(source.publishGeneration({noScope}).error.code,
                 TaskListErrorCode::InvalidWorkspaceScope);

        auto allPlusList = TaskListTest::allWorkspaces(
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId));
        allPlusList.workspaceIds = {QStringLiteral("ws-1")};
        QCOMPARE(source.publishGeneration({allPlusList}).error.code,
                 TaskListErrorCode::InvalidWorkspaceScope);

        auto duplicated = TaskListTest::withWorkspaces(
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId),
            {QStringLiteral("ws-1"), QStringLiteral("ws-1")});
        QCOMPARE(source.publishGeneration({duplicated}).error.code,
                 TaskListErrorCode::InvalidWorkspaceScope);

        auto overLimit =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        QStringList workspaces;
        for (int index = 0; index <= kMaxWorkspacesPerWindow; ++index) {
            workspaces.append(QStringLiteral("ws-%1").arg(index));
        }
        overLimit.workspaceIds = workspaces;
        QCOMPARE(source.publishGeneration({overLimit}).error.code,
                 TaskListErrorCode::LimitExceeded);
    }

    void windowFactBoundsAreEnforced()
    {
        TaskListSource source;
        QVector<TaskWindowFact> facts;
        facts.reserve(int(kMaxWindowFacts) + 1);
        for (quint32 index = 0; index <= kMaxWindowFacts; ++index) {
            facts.append(TaskListTest::standalone(
                QStringLiteral("w-%1").arg(index), kAppId));
        }
        const auto rejected = source.publishGeneration(facts);
        QCOMPARE(rejected.error.code, TaskListErrorCode::LimitExceeded);
        QCOMPARE(source.revision(), quint64(0));
    }

    void duplicateWindowIdsAreRejected()
    {
        TaskListSource source;
        const auto rejected = source.publishGeneration({
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId),
            TaskListTest::standalone(QStringLiteral("w-1"),
                                     QStringLiteral("app.files")),
        });
        QCOMPARE(rejected.error.code, TaskListErrorCode::DuplicateWindowId);
    }

    void containerRoleContractIsEnforced()
    {
        TaskListSource source;
        auto standaloneWithContainer =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        standaloneWithContainer.containerId = QStringLiteral("c-1");
        QCOMPARE(source.publishGeneration({standaloneWithContainer})
                     .error.code,
                 TaskListErrorCode::ConflictingContainerRole);

        auto groupedWithoutContainer = TaskListTest::makeFact(
            QStringLiteral("w-1"), kAppId);
        groupedWithoutContainer.role =
            TaskWindowRole::ContainerPrimary;
        QCOMPARE(source.publishGeneration({groupedWithoutContainer})
                     .error.code,
                 TaskListErrorCode::ConflictingContainerRole);

        const auto orphan = source.publishGeneration({
            TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
        });
        QCOMPARE(orphan.error.code, TaskListErrorCode::OrphanContainerMember);

        const auto twoPrimaries = source.publishGeneration({
            TaskListTest::primary(QStringLiteral("w-p1"), kAppId,
                                  QStringLiteral("c-1")),
            TaskListTest::primary(QStringLiteral("w-p2"), kAppId,
                                  QStringLiteral("c-1")),
        });
        QCOMPARE(twoPrimaries.error.code,
                 TaskListErrorCode::DuplicateContainerPrimary);
    }

    void suppressedMemberStateAndActiveCardinalityAreEnforced()
    {
        TaskListSource source;
        auto activeMember =
            TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1"));
        activeMember.active = true;
        QCOMPARE(source.publishGeneration(
                     {TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                            QStringLiteral("c-1")),
                      activeMember})
                     .error.code,
                 TaskListErrorCode::InvalidMemberState);

        auto minimizedMember =
            TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1"));
        minimizedMember.minimized = true;
        QCOMPARE(source.publishGeneration(
                     {TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                            QStringLiteral("c-1")),
                      minimizedMember})
                     .error.code,
                 TaskListErrorCode::InvalidMemberState);

        auto firstActive =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        firstActive.active = true;
        auto secondActive =
            TaskListTest::standalone(QStringLiteral("w-2"), kAppId);
        secondActive.active = true;
        QCOMPARE(source.publishGeneration({firstActive, secondActive})
                     .error.code,
                 TaskListErrorCode::MultipleActiveWindows);

        auto activeMinimized =
            TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        activeMinimized.active = true;
        activeMinimized.minimized = true;
        QCOMPARE(source.publishGeneration({activeMinimized}).error.code,
                 TaskListErrorCode::ActiveMinimizedConflict);
    }

    void taskIdentitySpacesMustStayDisjoint()
    {
        TaskListSource source;
        const auto rejected = source.publishGeneration({
            TaskListTest::standalone(QStringLiteral("c-1"), kAppId),
            TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                  QStringLiteral("c-1")),
        });
        QCOMPARE(rejected.error.code, TaskListErrorCode::EntryIdentityCollision);
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListSourceValidationTest)
#include "tst_task_list_source_validation.moc"
