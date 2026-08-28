// SPDX-License-Identifier: LGPL-3.0-or-later
#include "task_list_test_support.h"

#include <QtTest>

using namespace QindaQt::ShellTaskList;

namespace {

const QString kAppId = QStringLiteral("app.editor");

class TaskListPresentationTest final : public QObject {
    Q_OBJECT

private slots:
    void noAcceptedGenerationPresentsLoading()
    {
        // Build a real generation, then project it with the Loading status:
        // no accepted generation exists yet, so whatever is handed in must be
        // ignored.
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(evaluation.ok());
        const auto presentation = TaskListPresentationModel::project(
            TaskListSourceStatus::Loading, evaluation.generation, TaskListScope{});
        QCOMPARE(presentation.state, TaskListState::Loading);
        QCOMPARE(presentation.entries.isEmpty(), true);
        QCOMPARE(presentation.identities.isEmpty(), true);
    }

    void readyGenerationProjectsIdentitiesInCanonicalOrder()
    {
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(source, {
            TaskListTest::standalone(QStringLiteral("w-b"), kAppId),
            TaskListTest::standalone(QStringLiteral("w-a"), kAppId),
        });
        QVERIFY(evaluation.ok());
        const auto presentation = TaskListPresentationModel::project(
            source.status(), evaluation.generation, TaskListScope{});

        QCOMPARE(presentation.state, TaskListState::Ready);
        QCOMPARE(presentation.entries.size(), 2);
        QCOMPARE(presentation.identities.size(), 2);
        QCOMPARE(presentation.identities.first().taskId,
                 QStringLiteral("w-a"));
        QCOMPARE(presentation.identities.first().keyboardIndex, 1);
        QCOMPARE(presentation.identities.last().taskId,
                 QStringLiteral("w-b"));
        QCOMPARE(presentation.identities.last().keyboardIndex, 2);
    }

    void filteredOutSelectionPresentsEmpty()
    {
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(evaluation.ok());
        TaskListScope scope;
        scope.outputId = QStringLiteral("output-absent");

        const auto presentation =
            TaskListPresentationModel::project(source.status(), evaluation.generation, scope);
        QCOMPARE(presentation.state, TaskListState::Empty);
        QCOMPARE(presentation.identities.isEmpty(), true);
    }

    void degradedSourcesRetainTheirVisibleEntries()
    {
        TaskListSource source;
        const auto evaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)});
        QVERIFY(evaluation.ok());
        source.markDegraded();

        const auto presentation = TaskListPresentationModel::project(
            source.status(), evaluation.generation, TaskListScope{});
        QCOMPARE(presentation.state, TaskListState::Degraded);
        QCOMPARE(presentation.entries.size(), 1);
        QCOMPARE(presentation.identities.size(), 1);
        // An empty selection stays Empty even when degraded: there is no
        // visible row to annotate with the degraded state.
        TaskListScope absent;
        absent.outputId = QStringLiteral("output-absent");
        const auto emptyDegraded = TaskListPresentationModel::project(
            source.status(), evaluation.generation, absent);
        QCOMPARE(emptyDegraded.state, TaskListState::Empty);
    }

    void resetReturnsToLoading()
    {
        TaskListSource source;
        QVERIFY(TaskListTest::publish(
                    source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId)})
                    .ok());
        source.reset();
        QCOMPARE(source.status(), TaskListSourceStatus::Loading);
        QCOMPARE(source.revision(), quint64(0));
        QCOMPARE(source.generation().entries.isEmpty(), true);
    }

    void keyboardIndicesStayStableAcrossIdenticalGenerations()
    {
        TaskListSource source;
        const auto firstEvaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId),
                     TaskListTest::standalone(QStringLiteral("w-2"), kAppId)});
        QVERIFY(firstEvaluation.ok());
        const auto firstPresentation = TaskListPresentationModel::project(
            source.status(), firstEvaluation.generation, TaskListScope{});
        const auto secondEvaluation = TaskListTest::publish(
            source, {TaskListTest::standalone(QStringLiteral("w-1"), kAppId),
                     TaskListTest::standalone(QStringLiteral("w-2"), kAppId)});
        QVERIFY(secondEvaluation.ok());
        const auto secondPresentation = TaskListPresentationModel::project(
            source.status(), secondEvaluation.generation, TaskListScope{});

        // AGENT-GUARD: Equal fact batches must not reshuffle keyboard order
        // between generations; shortcuts would otherwise change meaning.
        QCOMPARE(secondPresentation.identities == firstPresentation.identities,
                 true);
    }

    void accessibleNamesComposeApplicationTitleAndState()
    {
        TaskEntry entry;
        entry.applicationName = QStringLiteral("Text Editor");
        entry.title = QStringLiteral("notes.txt");
        entry.active = true;
        QCOMPARE(TaskListPresentationModel::accessibleName(entry),
                 QStringLiteral("Text Editor — notes.txt, active"));

        entry.minimized = true;
        entry.urgent = true;
        QCOMPARE(TaskListPresentationModel::accessibleName(entry),
                 QStringLiteral(
                     "Text Editor — notes.txt, active, minimized, urgent"));
    }

    void accessibleNamesFallBackWithoutATitleAndCountGroupedWindows()
    {
        TaskEntry untitled;
        untitled.applicationName = QStringLiteral("Text Editor");
        QCOMPARE(TaskListPresentationModel::accessibleName(untitled),
                 QStringLiteral("Text Editor"));

        TaskEntry container;
        container.kind = TaskEntryKind::Container;
        container.applicationName = QStringLiteral("Files");
        container.title = QStringLiteral("Projects");
        container.windowCount = 3;
        container.minimized = true;
        QCOMPARE(TaskListPresentationModel::accessibleName(container),
                 QStringLiteral("Files — Projects, 3 windows, minimized"));

        TaskEntry singletonContainer = container;
        singletonContainer.windowCount = 1;
        singletonContainer.minimized = false;
        QCOMPARE(TaskListPresentationModel::accessibleName(singletonContainer),
                 QStringLiteral("Files — Projects"));
    }
};

} // namespace

QTEST_APPLESS_MAIN(TaskListPresentationTest)
#include "tst_task_list_presentation.moc"
