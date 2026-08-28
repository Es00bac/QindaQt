// SPDX-License-Identifier: GPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

class TaskListValuesTest final : public QObject {
    Q_OBJECT

private slots:
    void identicalFactsCompareEqualAndCopyIndependently()
    {
        const auto original = TaskListTest::standalone(
            QStringLiteral("w1"), QStringLiteral("app.editor"));
        auto copy = original;
        copy.minimized = true;

        QCOMPARE(original == copy, false);
        QCOMPARE(original == TaskListTest::standalone(
                                  QStringLiteral("w1"),
                                  QStringLiteral("app.editor")),
                 true);
        // AGENT-GUARD: Facts are immutable published values; a mutated local
        // copy must never be observable through the generation it came from.
        QCOMPARE(original.minimized, false);
        QCOMPARE(original.role, TaskWindowRole::Standalone);
        QCOMPARE(original.containerId.isEmpty(), true);
    }

    void entryDefaultsDescribeAStandaloneWindow()
    {
        TaskEntry entry;
        QCOMPARE(entry.kind, TaskEntryKind::Window);
        QCOMPARE(entry.windowCount, quint32(1));
        QCOMPARE(entry.active, false);
        QCOMPARE(entry.minimized, false);
        QCOMPARE(entry.urgent, false);
        QCOMPARE(entry.onAllWorkspaces, false);
    }

    void generationEqualityCoversRevisionAndEntries()
    {
        TaskGeneration left;
        left.revision = 3;
        TaskGeneration right = left;
        QCOMPARE(left == right, true);
        right.entries.append(TaskEntry{});
        QCOMPARE(left == right, false);
        right = left;
        right.revision = 4;
        QCOMPARE(left == right, false);
    }

    void intentValuesRoundTripDefaults()
    {
        TaskIntentRequest request;
        QCOMPARE(request.kind, TaskIntentKind::Activate);
        QCOMPARE(request.expectedRevision, quint64(0));
        QCOMPARE(request.taskId.isEmpty(), true);

        TaskIntentOutcome outcome;
        QCOMPARE(outcome.ok(), true);
        QCOMPARE(outcome.entryKind, TaskEntryKind::Window);

        TaskIntentRequest other = request;
        other.taskId = QStringLiteral("w1");
        QCOMPARE(request == other, false);
    }

    void evaluationDefaultsToSuccess()
    {
        TaskListEvaluation evaluation;
        QCOMPARE(evaluation.ok(), true);
        QCOMPARE(evaluation.error.code, TaskListErrorCode::None);
        QCOMPARE(evaluation.error.hasError(), false);

        TaskListError error;
        error.code = TaskListErrorCode::DuplicateWindowId;
        QCOMPARE(error.hasError(), true);
    }

    void boundsStaySaneForBoundedGenerations()
    {
        QVERIFY(kMaxWindowFacts > 0);
        QVERIFY(kMaxWindowFacts <= 65536);
        QVERIFY(kMaxWorkspacesPerWindow > 0);
        QVERIFY(kMaxIdLength >= 64);
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListValuesTest)
#include "tst_task_list_values.moc"
