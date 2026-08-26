// SPDX-License-Identifier: GPL-3.0-or-later
#include "editing_test_fixtures.h"

#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QtTest>

#include <limits>

using namespace QindaQt;
using namespace QindaQt::ShellCustomization;
using namespace QindaQt::ShellCustomization::TestFixtures;

class PreviewHistoryTest final : public QObject {
    Q_OBJECT

private slots:
    void cancelRollsBackTheWholePreviewAtomically();
    void commitCollapsesPreviewIntoOneDurableUndoStep();
    void failedCommandsDoNotDisturbHistory();
    void validatesPreviewStateAndReservesTerminationRevision();
};

void PreviewHistoryTest::cancelRollsBackTheWholePreviewAtomically()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const auto initial = repository.snapshot();

    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QVERIFY(repository.snapshot()->previewActive);
    QVERIFY(coordinator->execute(MovePanelCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("external"),
        .edge = Profiles::Edge::Left,
        .alignment = Profiles::Alignment::End,
        .beforePanelId = std::nullopt,
    }).succeeded());
    QVERIFY(coordinator->execute(UpdateAppletSettingsCommand{
        .expectedRevision = 2,
        .panelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .settings = {{QStringLiteral("zone"), QStringLiteral("end")},
                     {QStringLiteral("seconds"), true}},
    }).succeeded());

    const auto beforeFailure = repository.snapshot();
    const EditingResult invalid = coordinator->execute(MovePanelCommand{
        .expectedRevision = 3,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("missing"),
        .edge = Profiles::Edge::Right,
        .alignment = Profiles::Alignment::Start,
        .beforePanelId = std::nullopt,
    });
    QCOMPARE(invalid.error.code, EditingErrorCode::InvalidLayout);
    QCOMPARE(repository.snapshot(), beforeFailure);
    QCOMPARE(coordinator->committedProfile()->toJson(), initial->profile.toJson());

    const EditingResult cancelled = coordinator->execute(CancelPreviewCommand{3});
    QVERIFY2(cancelled.succeeded(), qPrintable(cancelled.error.message));
    QCOMPARE(repository.snapshot()->revision, quint64(4));
    QVERIFY(!repository.snapshot()->previewActive);
    QCOMPARE(repository.snapshot()->profile.toJson(), initial->profile.toJson());
    QCOMPARE(coordinator->committedProfile()->toJson(), initial->profile.toJson());
}

void PreviewHistoryTest::commitCollapsesPreviewIntoOneDurableUndoStep()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const QJsonObject initial = repository.snapshot()->profile.toJson();

    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QVERIFY(coordinator->execute(MovePanelCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("external"),
        .edge = Profiles::Edge::Right,
        .alignment = Profiles::Alignment::End,
        .beforePanelId = std::nullopt,
    }).succeeded());
    QVERIFY(coordinator->execute(UpdateAppletSettingsCommand{
        .expectedRevision = 2,
        .panelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .settings = {{QStringLiteral("zone"), QStringLiteral("end")},
                     {QStringLiteral("seconds"), true}},
    }).succeeded());

    QVERIFY(coordinator->execute(UndoCommand{3}).succeeded());
    QVERIFY(!applet(repository.snapshot()->profile,
                    QStringLiteral("bar"),
                    QStringLiteral("clock-instance"))
                 ->settings.contains(QStringLiteral("seconds")));
    QVERIFY(coordinator->execute(RedoCommand{4}).succeeded());
    const QJsonObject preview = repository.snapshot()->profile.toJson();

    QVERIFY(coordinator->execute(CommitPreviewCommand{5}).succeeded());
    QVERIFY(!coordinator->hasPreview());
    QCOMPARE(coordinator->committedProfile()->toJson(), preview);

    QVERIFY(coordinator->execute(UndoCommand{6}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), initial);
    QCOMPARE(coordinator->execute(UndoCommand{7}).error.code,
             EditingErrorCode::NothingToUndo);

    QVERIFY(coordinator->execute(RedoCommand{7}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), preview);
}

void PreviewHistoryTest::failedCommandsDoNotDisturbHistory()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const QJsonObject initial = repository.snapshot()->profile.toJson();

    QVERIFY(coordinator->execute(MovePanelCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("external"),
        .edge = Profiles::Edge::Left,
        .alignment = Profiles::Alignment::Start,
        .beforePanelId = std::nullopt,
    }).succeeded());
    const auto validEdit = repository.snapshot();

    QCOMPARE(coordinator->execute(RemovePanelCommand{
                 .expectedRevision = 1,
                 .panelId = QStringLiteral("missing"),
             }).error.code,
             EditingErrorCode::UnknownPanelId);
    QCOMPARE(repository.snapshot(), validEdit);

    QVERIFY(coordinator->execute(UndoCommand{1}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), initial);
    QVERIFY(coordinator->execute(RedoCommand{2}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), validEdit->profile.toJson());
}

void PreviewHistoryTest::validatesPreviewStateAndReservesTerminationRevision()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    QCOMPARE(coordinator->execute(CommitPreviewCommand{0}).error.code,
             EditingErrorCode::PreviewNotActive);
    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QCOMPARE(coordinator->execute(BeginPreviewCommand{1}).error.code,
             EditingErrorCode::PreviewAlreadyActive);
    QVERIFY(coordinator->execute(CommitPreviewCommand{1}).succeeded());
    QCOMPARE(coordinator->execute(UndoCommand{2}).error.code,
             EditingErrorCode::NothingToUndo);

    constexpr quint64 maximum = std::numeric_limits<quint64>::max();
    LayoutEditingRepository noPreviewHeadroom(profile(), outputs(), manifests(), maximum - 1);
    auto noHeadroomCoordinator = noPreviewHeadroom.tryAcquireCoordinator();
    QVERIFY(noHeadroomCoordinator);
    QCOMPARE(noHeadroomCoordinator->execute(BeginPreviewCommand{maximum - 1})
                 .error.code,
             EditingErrorCode::RevisionExhausted);
    QCOMPARE(noPreviewHeadroom.snapshot()->revision, maximum - 1);

    LayoutEditingRepository terminationOnly(profile(), outputs(), manifests(), maximum - 2);
    auto terminationCoordinator = terminationOnly.tryAcquireCoordinator();
    QVERIFY(terminationCoordinator);
    QVERIFY(terminationCoordinator->execute(BeginPreviewCommand{maximum - 2}).succeeded());
    QCOMPARE(terminationCoordinator->execute(ConfigurePanelCommand{
                 .expectedRevision = maximum - 1,
                 .panelId = QStringLiteral("dock"),
                 .layer = Profiles::Layer::Above,
                 .hideMode = Profiles::HideMode::Never,
                 .rows = 1,
                 .thickness = 48,
                 .length = 0.6,
             }).error.code,
             EditingErrorCode::RevisionExhausted);
    QVERIFY(terminationCoordinator->execute(CommitPreviewCommand{maximum - 1}).succeeded());
    QCOMPARE(terminationOnly.snapshot()->revision, maximum);

    LayoutEditingRepository onePreviewEdit(profile(), outputs(), manifests(), maximum - 3);
    auto oneEditCoordinator = onePreviewEdit.tryAcquireCoordinator();
    QVERIFY(oneEditCoordinator);
    QVERIFY(oneEditCoordinator->execute(BeginPreviewCommand{maximum - 3}).succeeded());
    QVERIFY(oneEditCoordinator->execute(ConfigurePanelCommand{
        .expectedRevision = maximum - 2,
        .panelId = QStringLiteral("dock"),
        .layer = Profiles::Layer::Above,
        .hideMode = Profiles::HideMode::Never,
        .rows = 1,
        .thickness = 48,
        .length = 0.6,
    }).succeeded());
    QCOMPARE(oneEditCoordinator->execute(UndoCommand{maximum - 1}).error.code,
             EditingErrorCode::RevisionExhausted);
    QVERIFY(oneEditCoordinator->execute(CancelPreviewCommand{maximum - 1}).succeeded());
    QCOMPARE(onePreviewEdit.snapshot()->revision, maximum);
    QVERIFY(!onePreviewEdit.snapshot()->previewActive);
}

QTEST_GUILESS_MAIN(PreviewHistoryTest)
#include "tst_preview_history.moc"
