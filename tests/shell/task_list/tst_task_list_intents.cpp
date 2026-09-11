// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

const QString kAppId = QStringLiteral("app.editor");

class TaskListIntentsTest final : public QObject {
    Q_OBJECT

private slots:
    void intentsAreRefusedWithoutAnAcceptedGeneration()
    {
        const TaskListSource source;
        TaskIntentRequest request;
        request.taskId = QStringLiteral("w-1");
        request.expectedRevision = 1;

        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::NoGeneration);

        // Empty task ids fail before any generation lookup.
        TaskIntentRequest blank;
        blank.expectedRevision = 1;
        QCOMPARE(source.requestIntent(blank).code,
                 TaskIntentErrorCode::InvalidRequest);
    }

    void activationTargetsTheContainerPrimary()
    {
        // AGENT-GUARD: Container activation must resolve to the primary window,
        // and a primary-only container must still enumerate that primary as the
        // only member target.
        TaskListSource source;
        QVERIFY(TaskListTest::publish(
                    source,
                    {TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                           QStringLiteral("c-1"))})
                    .ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("c-1");
        request.kind = TaskIntentKind::Activate;
        request.expectedRevision = source.revision();

        const auto outcome = source.requestIntent(request);
        QCOMPARE(outcome.ok(), true);
        QCOMPARE(outcome.entryKind, TaskEntryKind::Container);
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-p"));
        QCOMPARE(outcome.memberWindowIds,
                 (QStringList{QStringLiteral("w-p")}));
    }

    void activationTargetsTheStandaloneWindow()
    {
        TaskListSource source;
        QVERIFY(TaskListTest::publish(
                    source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)})
                    .ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("w-1");
        request.kind = TaskIntentKind::Activate;
        request.expectedRevision = source.revision();

        const auto outcome = source.requestIntent(request);
        QCOMPARE(outcome.ok(), true);
        QCOMPARE(outcome.entryKind, TaskEntryKind::Window);
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-1"));
        QCOMPARE(outcome.memberWindowIds,
                 QStringList{QStringLiteral("w-1")});
    }

    void minimizedStateIsFencedIntoTheAcceptedIntent()
    {
        TaskListSource source;
        auto fact = TaskListTest::standalone(QStringLiteral("w-1"), kAppId);
        fact.minimized = true;
        QVERIFY(TaskListTest::publish(source, {fact}).ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("w-1");
        request.kind = TaskIntentKind::Minimize;
        request.expectedRevision = source.revision();
        QCOMPARE(source.requestIntent(request).minimized, true);
    }

    void containerIntentsExposeEveryMemberDeterministically()
    {
        TaskListSource source;
        QVERIFY(TaskListTest::publish(source, {
                                          TaskListTest::member(QStringLiteral("w-m2"), QStringLiteral("c-1")),
                                          TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                                                QStringLiteral("c-1")),
                                          TaskListTest::member(QStringLiteral("w-m1"), QStringLiteral("c-1")),
                                      })
                    .ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("c-1");
        request.kind = TaskIntentKind::Close;
        request.expectedRevision = source.revision();

        const auto outcome = source.requestIntent(request);
        QCOMPARE(outcome.ok(), true);
        QCOMPARE(outcome.entryKind, TaskEntryKind::Container);
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-p"));
        // AGENT-CONTRACT: The adapter decides Close All/Ungroup/Cancel policy;
        // this boundary only guarantees a deterministic member enumeration.
        QCOMPARE(outcome.memberWindowIds,
                 (QStringList{QStringLiteral("w-m1"), QStringLiteral("w-m2"),
                              QStringLiteral("w-p")}));
    }

    void namedWindowNarrowsActivateAndCloseToThatMember()
    {
        // AGENT-GUARD: an ungrouped strip names the member it shows. Activate
        // and Close must target exactly that window, because the shell router
        // acts on outcome targets verbatim; Minimize and Raise keep the whole
        // container; and a window outside the entry is refused like a
        // vanished task, so a stale row can never reach a foreign window.
        TaskListSource source;
        QVERIFY(TaskListTest::publish(source, {
                    TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                          QStringLiteral("c-1")),
                    TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
                    TaskListTest::standalone(QStringLiteral("w-loose"), kAppId),
                }).ok());
        const QStringList everyMember{QStringLiteral("w-m"), QStringLiteral("w-p")};

        TaskIntentRequest request;
        request.taskId = QStringLiteral("c-1");
        request.windowId = QStringLiteral("w-m");
        request.expectedRevision = source.revision();

        request.kind = TaskIntentKind::Activate;
        auto outcome = source.requestIntent(request);
        QVERIFY(outcome.ok());
        QCOMPARE(outcome.entryKind, TaskEntryKind::Container);
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-m"));
        QCOMPARE(outcome.memberWindowIds, everyMember);

        request.kind = TaskIntentKind::Close;
        outcome = source.requestIntent(request);
        QVERIFY(outcome.ok());
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-m"));
        QCOMPARE(outcome.memberWindowIds, QStringList{QStringLiteral("w-m")});

        request.kind = TaskIntentKind::Minimize;
        outcome = source.requestIntent(request);
        QVERIFY(outcome.ok());
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-p"));
        QCOMPARE(outcome.memberWindowIds, everyMember);

        request.kind = TaskIntentKind::Raise;
        outcome = source.requestIntent(request);
        QVERIFY(outcome.ok());
        QCOMPARE(outcome.primaryWindowId, QStringLiteral("w-p"));

        request.kind = TaskIntentKind::Activate;
        request.windowId = QStringLiteral("w-loose");
        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::UnknownTask);

        // Staleness is still judged before membership.
        request.expectedRevision = source.revision() + 1;
        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::StaleRevision);
    }

    void staleRevisionIsRejectedBeforeUnknownTask()
    {
        TaskListSource source;
        QVERIFY(TaskListTest::publish(
                    source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)})
                    .ok());
        QVERIFY(TaskListTest::publish(
                    source, {TaskListTest::standalone(QStringLiteral("w-2"), kAppId)})
                    .ok());

        TaskIntentRequest stale;
        stale.taskId = QStringLiteral("w-1");
        stale.expectedRevision = 1;
        // The id is also absent from the current generation, but the stale
        // revision is the contract failure and must be reported first.
        QCOMPARE(source.requestIntent(stale).code,
                 TaskIntentErrorCode::StaleRevision);

        TaskIntentRequest future;
        future.taskId = QStringLiteral("w-2");
        future.expectedRevision = source.revision() + 1;
        QCOMPARE(source.requestIntent(future).code,
                 TaskIntentErrorCode::StaleRevision);

        TaskIntentRequest unknown;
        unknown.taskId = QStringLiteral("w-missing");
        unknown.expectedRevision = source.revision();
        QCOMPARE(source.requestIntent(unknown).code,
                 TaskIntentErrorCode::UnknownTask);
    }

    void regroupedTasksRejectTheirVanishedIdentity()
    {
        TaskListSource source;
        QVERIFY(TaskListTest::publish(source, {
                                          TaskListTest::primary(QStringLiteral("w-p"), kAppId,
                                                                QStringLiteral("c-1")),
                                          TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
                                      })
                    .ok());
        // A later generation ungroups the container: the container identity
        // must never resolve again even with a current revision.
        QVERIFY(TaskListTest::publish(source, {TaskListTest::standalone(
                                                   QStringLiteral("w-p"), kAppId)})
                    .ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("c-1");
        request.expectedRevision = source.revision();
        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::UnknownTask);

        request.taskId = QStringLiteral("w-p");
        const auto outcome = source.requestIntent(request);
        QCOMPARE(outcome.ok(), true);
        QCOMPARE(outcome.entryKind, TaskEntryKind::Window);
    }

    void degradedSourcesRefuseEveryIntentUntilRecovery()
    {
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(evaluation.ok());

        TaskIntentRequest request;
        request.taskId = QStringLiteral("w-1");
        request.expectedRevision = evaluation.generation.revision;

        source.markDegraded();
        QCOMPARE(source.status(), TaskListSourceStatus::Degraded);
        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::SourceDegraded);

        const auto recoveredEvaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(recoveredEvaluation.ok());
        const auto &recovered = recoveredEvaluation.generation;
        QCOMPARE(recovered.revision, quint64(2));
        request.expectedRevision = recovered.revision;
        QCOMPARE(source.requestIntent(request).ok(), true);
    }

    void degradeBeforeFirstPublishIsKnownUnavailable()
    {
        TaskListSource source;
        source.markDegraded();
        // A completed failed refresh is no longer "loading" merely because it
        // has no retained generation. Availability must fail closed.
        QCOMPARE(source.status(), TaskListSourceStatus::Degraded);

        TaskIntentRequest request;
        request.taskId = QStringLiteral("w-1");
        request.expectedRevision = 0;
        QCOMPARE(source.requestIntent(request).code,
                 TaskIntentErrorCode::SourceDegraded);
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListIntentsTest)
#include "tst_task_list_intents.moc"
