// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

const QString kEditorId = QStringLiteral("app.editor");
const QString kFilesId = QStringLiteral("app.files");

// Tests stay on the public source boundary: canonical entries always come
// from an accepted generation rather than grouping internals.
QVector<TaskEntry> entriesFor(const QVector<TaskWindowFact> &facts) {
  TaskListSource source;
  return TaskListTest::publish(source, facts).entries;
}

QVector<TaskWindowFact> sampleFacts()
{
    return {
        TaskListTest::standalone(QStringLiteral("w-editor"), kEditorId),
        TaskListTest::withOutput(
            TaskListTest::standalone(QStringLiteral("w-out2"), kFilesId),
            QStringLiteral("output-2")),
        TaskListTest::allWorkspaces(
            TaskListTest::standalone(QStringLiteral("w-sticky"), kFilesId)),
    };
}

class TaskListScopeFilterTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyScopeAdmitsEveryEntry()
    {
        const auto entries = entriesFor(sampleFacts());
        QCOMPARE(TaskListFilter::filter(entries, TaskListScope{}).size(), 3);
    }

    void outputScopeKeepsOnlyAssignedEntries()
    {
        const auto entries = entriesFor(sampleFacts());
        TaskListScope scope;
        scope.outputId = QStringLiteral("output-2");

        const auto visible = TaskListFilter::filter(entries, scope);
        QCOMPARE(visible.size(), 2);
        for (const TaskEntry &entry : visible) {
            QCOMPARE(entry.outputId, QStringLiteral("output-2"));
        }
    }

    void workspaceScopeIncludesAllWorkspaceEntries()
    {
        const auto entries = entriesFor(sampleFacts());
        TaskListScope scope;
        scope.workspaceId = QStringLiteral("ws-1");
        QCOMPARE(TaskListFilter::filter(entries, scope).size(), 3);

        scope.workspaceId = QStringLiteral("ws-2");
        const auto onSecondWorkspace =
            TaskListFilter::filter(entries, scope);
        QCOMPARE(onSecondWorkspace.size(), 1);
        QCOMPARE(onSecondWorkspace.first().taskId, QStringLiteral("w-sticky"));
    }

    void multiWorkspaceMembershipParticipatesInEachWorkspace()
    {
        const auto entries = entriesFor({
            TaskListTest::withWorkspaces(
                TaskListTest::standalone(QStringLiteral("w-multi"), kEditorId),
                {QStringLiteral("ws-1"), QStringLiteral("ws-3")}),
        });
        for (const QString &workspaceId :
             {QStringLiteral("ws-1"), QStringLiteral("ws-3")}) {
            TaskListScope scope;
            scope.workspaceId = workspaceId;
            QCOMPARE(TaskListFilter::filter(entries, scope).size(), 1);
        }
        TaskListScope missing;
        missing.workspaceId = QStringLiteral("ws-9");
        QCOMPARE(TaskListFilter::filter(entries, missing).size(), 0);
    }

    void combinedScopeRequiresBothAxes()
    {
        // w-foreign satisfies the workspace axis but sits on another output;
        // w-offscope satisfies the output axis but sits on another workspace.
        const auto entries = entriesFor({
            TaskListTest::standalone(QStringLiteral("w-editor"), kEditorId),
            TaskListTest::withWorkspaces(
                TaskListTest::withOutput(
                    TaskListTest::standalone(QStringLiteral("w-foreign"),
                                             kFilesId),
                    QStringLiteral("output-1")),
                {QStringLiteral("ws-1")}),
            TaskListTest::withWorkspaces(
                TaskListTest::withOutput(
                    TaskListTest::standalone(QStringLiteral("w-offscope"),
                                             kFilesId),
                    QStringLiteral("output-2")),
                {QStringLiteral("ws-9")}),
            TaskListTest::withWorkspaces(
                TaskListTest::withOutput(
                    TaskListTest::standalone(QStringLiteral("w-match"),
                                             kFilesId),
                    QStringLiteral("output-2")),
                {QStringLiteral("ws-1")}),
        });
        TaskListScope scope;
        scope.outputId = QStringLiteral("output-2");
        scope.workspaceId = QStringLiteral("ws-1");

        const auto visible = TaskListFilter::filter(entries, scope);
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.first().taskId, QStringLiteral("w-match"));
    }

    void filteringPreservesCanonicalOrder()
    {
        const auto entries = entriesFor({
            TaskListTest::withOutput(
                TaskListTest::standalone(QStringLiteral("w-b"), kEditorId),
                QStringLiteral("output-2")),
            TaskListTest::withOutput(
                TaskListTest::standalone(QStringLiteral("w-a"), kEditorId),
                QStringLiteral("output-2")),
        });
        TaskListScope scope;
        scope.outputId = QStringLiteral("output-2");

        const auto visible = TaskListFilter::filter(entries, scope);
        QCOMPARE(visible.size(), 2);
        QCOMPARE(visible.first().taskId, QStringLiteral("w-a"));
        QCOMPARE(visible.last().taskId, QStringLiteral("w-b"));
    }

    void containerVisibilityFollowsItsPrimaryPlacement()
    {
        auto primaryFact = TaskListTest::primary(QStringLiteral("w-p"), kFilesId,
                                                 QStringLiteral("c-1"));
        primaryFact.outputId = QStringLiteral("output-2");
        const auto entries = entriesFor({
            TaskListTest::standalone(QStringLiteral("w-loose"), kEditorId),
            primaryFact,
            TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
        });

        TaskListScope first;
        first.outputId = QStringLiteral("output-1");
        QCOMPARE(TaskListFilter::filter(entries, first).size(), 1);

        TaskListScope second;
        second.outputId = QStringLiteral("output-2");
        const auto visible = TaskListFilter::filter(entries, second);
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.first().kind, TaskEntryKind::Container);
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListScopeFilterTest)
#include "tst_task_list_scope_filter.moc"
