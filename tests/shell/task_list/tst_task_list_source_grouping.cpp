// SPDX-License-Identifier: GPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

#include <utility>

using namespace QindaQt::ShellTaskList;

namespace {

const QString kEditorId = QStringLiteral("app.editor");
const QString kFilesId = QStringLiteral("app.files");

QVector<TaskEntry> ids(const QVector<TaskEntry> &entries) {
  QVector<TaskEntry> projected;
  projected.reserve(entries.size());
  for (const TaskEntry &entry : entries) {
    // Keep only identity/order-relevant fields for readable comparisons.
    TaskEntry projectedEntry;
    projectedEntry.taskId = entry.taskId;
    projectedEntry.kind = entry.kind;
    projectedEntry.applicationId = entry.applicationId;
    projected.append(projectedEntry);
  }
  return projected;
}

class TaskListSourceGroupingTest final : public QObject {
    Q_OBJECT

private slots:
    void entriesAreGroupedByApplicationInCanonicalOrder()
    {
        TaskListSource source;
        // Deliberately scrambled producer order across applications and kinds.
        const auto evaluation = TaskListTest::publish(source, {
            TaskListTest::standalone(QStringLiteral("w-editor-2"), kEditorId),
            TaskListTest::primary(QStringLiteral("w-files-p"), kFilesId,
                                  QStringLiteral("c-files")),
            TaskListTest::standalone(QStringLiteral("w-editor-1"), kEditorId),
            TaskListTest::member(QStringLiteral("w-files-m"), QStringLiteral("c-files")),
        });
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;
        QCOMPARE(source.status(), TaskListSourceStatus::Ready);
        QCOMPARE(generation.revision, quint64(1));

        const auto expected = [] {
            QVector<TaskEntry> entries;
            TaskEntry entry;
            entry.taskId = QStringLiteral("w-editor-1");
            entry.kind = TaskEntryKind::Window;
            entry.applicationId = kEditorId;
            entries.append(entry);
            entry.taskId = QStringLiteral("w-editor-2");
            entries.append(entry);
            entry.taskId = QStringLiteral("c-files");
            entry.kind = TaskEntryKind::Container;
            entry.applicationId = kFilesId;
            entries.append(entry);
            return entries;
        }();
        QCOMPARE(ids(generation.entries), expected);
    }

    void containerCollapsesToOneEntryWithSortedMembership()
    {
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(source, {
            TaskListTest::member(QStringLiteral("w-m2"), QStringLiteral("c-1")),
            TaskListTest::standalone(QStringLiteral("w-loose"), kEditorId),
            TaskListTest::member(QStringLiteral("w-m1"), QStringLiteral("c-1")),
            TaskListTest::primary(QStringLiteral("w-p"), kFilesId, QStringLiteral("c-1")),
        });
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;

        QCOMPARE(generation.entries.size(), 2);
        const TaskEntry &container = generation.entries.at(1);
        QCOMPARE(container.kind, TaskEntryKind::Container);
        QCOMPARE(container.taskId, QStringLiteral("c-1"));
        QCOMPARE(container.primaryWindowId, QStringLiteral("w-p"));
        QCOMPARE(container.windowCount, quint32(3));
        QCOMPARE(container.memberWindowIds,
                 (QStringList{QStringLiteral("w-m1"), QStringLiteral("w-m2"),
                              QStringLiteral("w-p")}));
    }

    void suppressedMembersNeverCreateTheirOwnEntries()
    {
        TaskListSource source;
        // A grouped member of a foreign application must not surface its own
        // application group; the container speaks with the primary identity.
        const auto evaluation = TaskListTest::publish(source, {
            TaskListTest::primary(QStringLiteral("w-p"), kFilesId, QStringLiteral("c-1")),
            TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
        });
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;
        QCOMPARE(generation.entries.size(), 1);
        QCOMPARE(generation.entries.first().applicationId, kFilesId);
        QCOMPARE(generation.entries.first().applicationName,
                 QStringLiteral("App app.files"));
    }

    void containerEntryInheritsPrimaryStateAndMemberUrgency()
    {
        TaskListSource source;
        auto primaryFact =
            TaskListTest::primary(QStringLiteral("w-p"), kFilesId, QStringLiteral("c-1"));
        primaryFact.minimized = true;
        const auto evaluation = TaskListTest::publish(source, {
            TaskListTest::withWorkspaces(
                TaskListTest::member(QStringLiteral("w-m"), QStringLiteral("c-1")),
                {QStringLiteral("ws-2")}),
            primaryFact,
        });
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;
        QCOMPARE(generation.entries.size(), 1);
        const TaskEntry &container = generation.entries.first();
        QCOMPARE(container.minimized, true);
        QCOMPARE(container.active, false);
        // Urgency is the one member state that must surface while grouped.
        auto urgentMember =
            TaskListTest::member(QStringLiteral("w-urgent"), QStringLiteral("c-2"));
        urgentMember.urgent = true;
        const auto nextEvaluation = TaskListTest::publish(source, {
            TaskListTest::primary(QStringLiteral("w-p2"), kFilesId, QStringLiteral("c-2")),
            urgentMember,
        });
        QVERIFY(nextEvaluation.ok());
        const auto &next = nextEvaluation.generation;
        QCOMPARE(next.entries.first().urgent, true);
        QCOMPARE(next.entries.first().windowCount, quint32(2));
    }

    void activeAndMinimizedFlagsProjectOntoEntries()
    {
        TaskListSource source;
        auto activeFact = TaskListTest::standalone(QStringLiteral("w-active"), kEditorId);
        activeFact.active = true;
        auto minimizedFact =
            TaskListTest::standalone(QStringLiteral("w-min"), kEditorId);
        minimizedFact.minimized = true;
        auto urgentFact = TaskListTest::standalone(QStringLiteral("w-urgent"), kFilesId);
        urgentFact.urgent = true;

        const auto evaluation =
            TaskListTest::publish(source, {activeFact, minimizedFact, urgentFact});
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;
        QCOMPARE(generation.entries.size(), 3);
        QCOMPARE(generation.entries.at(0).active, true);
        QCOMPARE(generation.entries.at(0).urgent, false);
        QCOMPARE(generation.entries.at(1).minimized, true);
        QCOMPARE(generation.entries.at(2).urgent, true);
    }

    void identicalFactsPublishIdenticalCanonicalGenerations()
    {
        const QVector<TaskWindowFact> facts = {
            TaskListTest::standalone(QStringLiteral("w-b"), kFilesId),
            TaskListTest::primary(QStringLiteral("w-p"), kEditorId, QStringLiteral("c-a")),
            TaskListTest::standalone(QStringLiteral("w-a"), kEditorId),
            TaskListTest::member(QStringLiteral("w-a-m"), QStringLiteral("c-a")),
        };
        auto shuffled = facts;
        std::swap(shuffled[0], shuffled[3]);

        TaskListSource first;
        TaskListSource second;
        const auto fromFactsEvaluation = TaskListTest::publish(first, facts);
        QVERIFY(fromFactsEvaluation.ok());
        const auto &fromFacts = fromFactsEvaluation.generation;
        const auto fromShuffledEvaluation = TaskListTest::publish(second, shuffled);
        QVERIFY(fromShuffledEvaluation.ok());
        const auto &fromShuffled = fromShuffledEvaluation.generation;
        // AGENT-GUARD: Keyboard traversal order is the canonical order, so two
        // equal fact batches must produce byte-identical generations.
        QCOMPARE(fromShuffled.entries == fromFacts.entries, true);
    }

    void primaryOnlyContainerCountsItself()
    {
        // AGENT-GUARD: A container with no additional members must still count
        // its representative primary; the old implementation left the primary
        // out of memberWindowIds and reported windowCount == 0.
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(
            source,
            {TaskListTest::primary(QStringLiteral("w-p"), kFilesId,
                                   QStringLiteral("c-1"))});
        QVERIFY(evaluation.ok());
        const auto &generation = evaluation.generation;
        QCOMPARE(generation.entries.size(), 1);
        const TaskEntry &container = generation.entries.first();
        QCOMPARE(container.kind, TaskEntryKind::Container);
        QCOMPARE(container.windowCount, quint32(1));
        QCOMPARE(container.memberWindowIds,
                 (QStringList{QStringLiteral("w-p")}));
    }

    void revisionAdvancesExactlyOncePerAcceptedPublish()
    {
        TaskListSource source;
        const auto firstEvaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kEditorId)});
        QVERIFY(firstEvaluation.ok());
        const auto &first = firstEvaluation.generation;
        QCOMPARE(first.revision, quint64(1));
        const auto secondEvaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-2"), kEditorId)});
        QVERIFY(secondEvaluation.ok());
        const auto &second = secondEvaluation.generation;
        QCOMPARE(second.revision, quint64(2));
        QCOMPARE(source.revision(), quint64(2));
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListSourceGroupingTest)
#include "tst_task_list_source_grouping.moc"
