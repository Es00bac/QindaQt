// SPDX-License-Identifier: GPL-3.0-or-later
#include "editing_test_fixtures.h"

#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QtTest>

#include <limits>

using namespace QindaQt;
using namespace QindaQt::ShellCustomization;
using namespace QindaQt::ShellCustomization::TestFixtures;

namespace {

void compareRejectedEvaluationAndExecution(
    LayoutEditingRepository &repository,
    LayoutEditingCoordinator &coordinator,
    const EditingCommand &command,
    EditingErrorCode expectedCode)
{
    const auto beforeSnapshot = repository.snapshot();
    const LayoutEditingStatus beforeStatus = repository.status();
    const auto beforeCommitted = coordinator.committedProfile();

    const EditingEvaluation evaluation = coordinator.evaluate(command);
    QVERIFY(!evaluation.accepted());
    QCOMPARE(evaluation.error.code, expectedCode);
    QCOMPARE(evaluation.revision, beforeSnapshot->revision);
    QCOMPARE(repository.snapshot(), beforeSnapshot);
    QCOMPARE(repository.status(), beforeStatus);
    QCOMPARE(coordinator.committedProfile().get(), beforeCommitted.get());

    const EditingResult execution = coordinator.execute(command);
    QVERIFY(!execution.succeeded());
    QCOMPARE(execution.error.code, evaluation.error.code);
    QCOMPARE(execution.error.message, evaluation.error.message);
    QCOMPARE(execution.error.panelId, evaluation.error.panelId);
    QCOMPARE(execution.error.appletId, evaluation.error.appletId);
    QCOMPARE(repository.snapshot(), beforeSnapshot);
    QCOMPARE(repository.status(), beforeStatus);
    QCOMPARE(coordinator.committedProfile().get(), beforeCommitted.get());
}

Applets::AppletManifest horizontalStartOnlyManifest()
{
    Applets::AppletManifest result = manifest(QStringLiteral("horizontal-start"));
    result.placementZones = {Applets::PlacementZone::PanelStart};
    result.orientations = {Applets::Orientation::Horizontal};
    return result;
}

ConfigurePanelCommand changedDock(quint64 revision)
{
    return {
        .expectedRevision = revision,
        .panelId = QStringLiteral("dock"),
        .layer = Profiles::Layer::Above,
        .hideMode = Profiles::HideMode::Never,
        .rows = 1,
        .thickness = 48,
        .length = 0.6,
    };
}

} // namespace

class EditorQueriesTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsImmutableStatusAcrossPreviewAndDurableHistory();
    void acceptsAValidEditWithoutPublishingOrCreatingHistory();
    void rejectedEvaluationMatchesEveryCandidateGate();
    void previewAndHistoryEvaluationNeverConsumesState();
    void evaluationMatchesRepositoryAndRevisionPreflight();
};

void EditorQueriesTest::reportsImmutableStatusAcrossPreviewAndDurableHistory()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    QCOMPARE(repository.status(), LayoutEditingStatus{});
    LayoutEditingStatus callerCopy = repository.status();
    callerCopy.canUndo = true;
    QVERIFY(callerCopy.canUndo);
    QVERIFY(!repository.status().canUndo);

    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{true, false, false, false}));

    QVERIFY(coordinator->execute(changedDock(1)).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{true, true, true, false}));

    QVERIFY(coordinator->execute(UndoCommand{2}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{true, false, false, true}));

    QVERIFY(coordinator->execute(RedoCommand{3}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{true, true, true, false}));

    QVERIFY(coordinator->execute(CommitPreviewCommand{4}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{false, false, true, false}));

    QVERIFY(coordinator->execute(UndoCommand{5}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{false, false, false, true}));

    QVERIFY(coordinator->execute(RedoCommand{6}).succeeded());
    QCOMPARE(repository.status(),
             (LayoutEditingStatus{false, false, true, false}));
}

void EditorQueriesTest::acceptsAValidEditWithoutPublishingOrCreatingHistory()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    const EditingCommand command = InsertAppletCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("bar"),
        .instanceId = QStringLiteral("query-clock"),
        .pluginId = QStringLiteral("clock"),
        .initialSettings = {{QStringLiteral("zone"), QStringLiteral("end")}},
        .beforeAppletId = QStringLiteral("clock-instance"),
    };
    const auto beforeSnapshot = repository.snapshot();
    const auto beforeCommitted = coordinator->committedProfile();
    const LayoutEditingStatus beforeStatus = repository.status();

    for (int repetition = 0; repetition < 2; ++repetition) {
        const EditingEvaluation evaluation = coordinator->evaluate(command);
        QVERIFY2(evaluation.accepted(), qPrintable(evaluation.error.message));
        QCOMPARE(evaluation.kind, EditingCommandKind::InsertApplet);
        QCOMPARE(evaluation.revision, quint64(0));
        QCOMPARE(repository.snapshot(), beforeSnapshot);
        QCOMPARE(repository.status(), beforeStatus);
        QCOMPARE(coordinator->committedProfile().get(), beforeCommitted.get());
    }

    QCOMPARE(coordinator->evaluate(UndoCommand{0}).error.code,
             EditingErrorCode::NothingToUndo);
    const EditingResult execution = coordinator->execute(command);
    QVERIFY2(execution.succeeded(), qPrintable(execution.error.message));
    QVERIFY(applet(repository.snapshot()->profile,
                   QStringLiteral("bar"),
                   QStringLiteral("query-clock")) != nullptr);
    QVERIFY(repository.status().canUndo);
}

void EditorQueriesTest::rejectedEvaluationMatchesEveryCandidateGate()
{
    QVector<Applets::AppletManifest> catalog = manifests();
    catalog.append(horizontalStartOnlyManifest());
    LayoutEditingRepository repository(profile(), outputs(), catalog);
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    compareRejectedEvaluationAndExecution(
        repository,
        *coordinator,
        MovePanelCommand{
            .expectedRevision = 0,
            .panelId = QStringLiteral("dock"),
            .outputId = QStringLiteral("missing-output"),
            .edge = Profiles::Edge::Bottom,
            .alignment = Profiles::Alignment::Center,
            .beforePanelId = std::nullopt,
        },
        EditingErrorCode::InvalidLayout);

    compareRejectedEvaluationAndExecution(
        repository,
        *coordinator,
        InsertAppletCommand{
            .expectedRevision = 0,
            .panelId = QStringLiteral("dock"),
            .instanceId = QStringLiteral("unsupported-zone"),
            .pluginId = QStringLiteral("horizontal-start"),
            .initialSettings = {{QStringLiteral("zone"), QStringLiteral("end")}},
            .beforeAppletId = std::nullopt,
        },
        EditingErrorCode::UnsupportedAppletPlacement);

    compareRejectedEvaluationAndExecution(
        repository,
        *coordinator,
        ConfigurePanelCommand{
            .expectedRevision = 0,
            .panelId = QStringLiteral("dock"),
            .layer = Profiles::Layer::Overlay,
            .hideMode = Profiles::HideMode::Never,
            .rows = 1,
            .thickness = 48,
            .length = 0.6,
        },
        EditingErrorCode::NoChange);

    Profiles::LayoutProfile singlePanel = profile();
    singlePanel.panels.removeLast();
    LayoutEditingRepository profileRepository(singlePanel, outputs(), manifests());
    QVERIFY(profileRepository.isReady());
    auto profileCoordinator = profileRepository.tryAcquireCoordinator();
    QVERIFY(profileCoordinator);
    compareRejectedEvaluationAndExecution(
        profileRepository,
        *profileCoordinator,
        RemovePanelCommand{0, QStringLiteral("bar")},
        EditingErrorCode::InvalidProfile);
}

void EditorQueriesTest::previewAndHistoryEvaluationNeverConsumesState()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    const auto initialSnapshot = repository.snapshot();
    QVERIFY(coordinator->evaluate(BeginPreviewCommand{0}).accepted());
    QVERIFY(coordinator->evaluate(BeginPreviewCommand{0}).accepted());
    QCOMPARE(repository.snapshot(), initialSnapshot);
    QCOMPARE(repository.status(), LayoutEditingStatus{});

    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QVERIFY(coordinator->execute(changedDock(1)).succeeded());
    const auto editedSnapshot = repository.snapshot();
    const LayoutEditingStatus editedStatus = repository.status();

    QVERIFY(coordinator->evaluate(UndoCommand{2}).accepted());
    QVERIFY(coordinator->evaluate(UndoCommand{2}).accepted());
    QVERIFY(coordinator->evaluate(CommitPreviewCommand{2}).accepted());
    QVERIFY(coordinator->evaluate(CancelPreviewCommand{2}).accepted());
    QCOMPARE(repository.snapshot(), editedSnapshot);
    QCOMPARE(repository.status(), editedStatus);

    QVERIFY(coordinator->execute(UndoCommand{2}).succeeded());
    const auto undoneSnapshot = repository.snapshot();
    const LayoutEditingStatus undoneStatus = repository.status();
    QVERIFY(coordinator->evaluate(RedoCommand{3}).accepted());
    QVERIFY(coordinator->evaluate(RedoCommand{3}).accepted());
    QCOMPARE(repository.snapshot(), undoneSnapshot);
    QCOMPARE(repository.status(), undoneStatus);

    QVERIFY(coordinator->execute(RedoCommand{3}).succeeded());
    QVERIFY(coordinator->execute(CancelPreviewCommand{4}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(),
             initialSnapshot->profile.toJson());
    QCOMPARE(repository.status(), LayoutEditingStatus{});
}

void EditorQueriesTest::evaluationMatchesRepositoryAndRevisionPreflight()
{
    Profiles::LayoutProfile invalidProfile = profile();
    invalidProfile.panels.clear();
    LayoutEditingRepository invalidRepository(invalidProfile, outputs(), manifests(), 7);
    QVERIFY(!invalidRepository.isReady());
    auto invalidCoordinator = invalidRepository.tryAcquireCoordinator();
    QVERIFY(invalidCoordinator);
    const EditingEvaluation notReady =
        invalidCoordinator->evaluate(BeginPreviewCommand{7});
    QCOMPARE(notReady.error.code, EditingErrorCode::RepositoryNotReady);
    QCOMPARE(invalidCoordinator->execute(BeginPreviewCommand{7}).error.code,
             notReady.error.code);
    QVERIFY(!invalidRepository.snapshot());

    LayoutEditingRepository repository(profile(), outputs(), manifests());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const EditingEvaluation stale = coordinator->evaluate(changedDock(9));
    QCOMPARE(stale.error.code, EditingErrorCode::StaleRevision);
    QCOMPARE(coordinator->execute(changedDock(9)).error.code, stale.error.code);
    QCOMPARE(repository.snapshot()->revision, quint64(0));

    constexpr quint64 maximum = std::numeric_limits<quint64>::max();
    LayoutEditingRepository exhausted(profile(), outputs(), manifests(), maximum - 1);
    auto exhaustedCoordinator = exhausted.tryAcquireCoordinator();
    QVERIFY(exhaustedCoordinator);
    const EditingEvaluation noHeadroom =
        exhaustedCoordinator->evaluate(BeginPreviewCommand{maximum - 1});
    QCOMPARE(noHeadroom.error.code, EditingErrorCode::RevisionExhausted);
    QCOMPARE(exhaustedCoordinator->execute(BeginPreviewCommand{maximum - 1})
                 .error.code,
             noHeadroom.error.code);
    QCOMPARE(exhausted.snapshot()->revision, maximum - 1);
}

QTEST_GUILESS_MAIN(EditorQueriesTest)
#include "tst_editor_queries.moc"
