// SPDX-License-Identifier: GPL-3.0-or-later
#include "editing_test_fixtures.h"

#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QtTest>

#include <limits>
#include <type_traits>

using namespace QindaQt;
using namespace QindaQt::ShellCustomization;
using namespace QindaQt::ShellCustomization::TestFixtures;

static_assert(!std::is_copy_constructible_v<LayoutEditingRepository>);
static_assert(!std::is_move_constructible_v<LayoutEditingRepository>);
static_assert(!std::is_copy_constructible_v<LayoutEditingCoordinator>);
static_assert(!std::is_move_constructible_v<LayoutEditingCoordinator>);

class CoordinatorLeaseTest final : public QObject {
    Q_OBJECT

private slots:
    void permitsOnlyOneMoveOnlyLease();
    void durableHistorySurvivesLeaseHandoff();
    void netZeroPreviewTerminationPreservesDurableRedoAcrossHandoff_data();
    void netZeroPreviewTerminationPreservesDurableRedoAcrossHandoff();
    void previewSurvivesLeaseHandoffForCommitAndUndo();
    void previewCancellationRetainsReservedHeadroomAcrossHandoff();
    void nonReadyRepositoryStillReportsItsCommandFailure_data();
    void nonReadyRepositoryStillReportsItsCommandFailure();
};

void CoordinatorLeaseTest::permitsOnlyOneMoveOnlyLease()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    auto first = repository.tryAcquireCoordinator();
    QVERIFY(first);
    QVERIFY(!repository.tryAcquireCoordinator());

    auto transferred = std::move(first);
    QVERIFY(!first);
    QVERIFY(transferred);
    QVERIFY(!repository.tryAcquireCoordinator());

    transferred.reset();
    auto replacement = repository.tryAcquireCoordinator();
    QVERIFY(replacement);
}

void CoordinatorLeaseTest::durableHistorySurvivesLeaseHandoff()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    const QJsonObject initial = repository.snapshot()->profile.toJson();
    {
        auto first = repository.tryAcquireCoordinator();
        QVERIFY(first);
        QVERIFY(first->execute(ConfigurePanelCommand{
            .expectedRevision = 0,
            .panelId = QStringLiteral("dock"),
            .layer = Profiles::Layer::Above,
            .hideMode = Profiles::HideMode::DodgeAll,
            .rows = 2,
            .thickness = 40,
            .length = 0.7,
        }).succeeded());
    }

    const QJsonObject edited = repository.snapshot()->profile.toJson();
    auto second = repository.tryAcquireCoordinator();
    QVERIFY(second);
    QCOMPARE(second->committedProfile()->toJson(), edited);
    QVERIFY(second->execute(UndoCommand{1}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), initial);
    QVERIFY(second->execute(RedoCommand{2}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), edited);
}

void CoordinatorLeaseTest::netZeroPreviewTerminationPreservesDurableRedoAcrossHandoff_data()
{
    QTest::addColumn<bool>("commitPreview");

    QTest::newRow("commit") << true;
    QTest::newRow("cancel") << false;
}

void CoordinatorLeaseTest::netZeroPreviewTerminationPreservesDurableRedoAcrossHandoff()
{
    QFETCH(bool, commitPreview);

    LayoutEditingRepository repository(profile(), outputs(), manifests());
    const QJsonObject initial = repository.snapshot()->profile.toJson();
    QJsonObject edited;
    {
        auto first = repository.tryAcquireCoordinator();
        QVERIFY(first);
        QVERIFY(first->execute(ConfigurePanelCommand{
            .expectedRevision = 0,
            .panelId = QStringLiteral("dock"),
            .layer = Profiles::Layer::Above,
            .hideMode = Profiles::HideMode::DodgeAll,
            .rows = 2,
            .thickness = 40,
            .length = 0.7,
        }).succeeded());
        edited = repository.snapshot()->profile.toJson();
        QVERIFY(first->execute(UndoCommand{1}).succeeded());
        QCOMPARE(repository.snapshot()->profile.toJson(), initial);
        QVERIFY(first->execute(BeginPreviewCommand{2}).succeeded());
    }

    {
        auto second = repository.tryAcquireCoordinator();
        QVERIFY(second);
        QVERIFY(second->hasPreview());
        const EditingResult terminated =
            commitPreview
            ? second->execute(CommitPreviewCommand{3})
            : second->execute(CancelPreviewCommand{3});
        QVERIFY2(terminated.succeeded(), qPrintable(terminated.error.message));
        QCOMPARE(repository.snapshot()->revision, quint64(4));
        QCOMPARE(repository.snapshot()->profile.toJson(), initial);
        QVERIFY(!second->hasPreview());
    }

    auto third = repository.tryAcquireCoordinator();
    QVERIFY(third);
    QVERIFY(third->execute(RedoCommand{4}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), edited);
    QCOMPARE(third->committedProfile()->toJson(), edited);
}

void CoordinatorLeaseTest::previewSurvivesLeaseHandoffForCommitAndUndo()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    const QJsonObject initial = repository.snapshot()->profile.toJson();
    {
        auto first = repository.tryAcquireCoordinator();
        QVERIFY(first);
        QVERIFY(first->execute(BeginPreviewCommand{0}).succeeded());
        QVERIFY(first->execute(ConfigurePanelCommand{
            .expectedRevision = 1,
            .panelId = QStringLiteral("dock"),
            .layer = Profiles::Layer::Above,
            .hideMode = Profiles::HideMode::Intelligent,
            .rows = 1,
            .thickness = 56,
            .length = 0.7,
        }).succeeded());
        QVERIFY(first->hasPreview());
        QCOMPARE(first->committedProfile()->toJson(), initial);
    }

    const QJsonObject preview = repository.snapshot()->profile.toJson();
    {
        auto second = repository.tryAcquireCoordinator();
        QVERIFY(second);
        QVERIFY(second->hasPreview());
        QCOMPARE(second->committedProfile()->toJson(), initial);
        QVERIFY(second->execute(CommitPreviewCommand{2}).succeeded());
        QCOMPARE(second->committedProfile()->toJson(), preview);
    }

    auto third = repository.tryAcquireCoordinator();
    QVERIFY(third);
    QVERIFY(!third->hasPreview());
    QVERIFY(third->execute(UndoCommand{3}).succeeded());
    QCOMPARE(repository.snapshot()->profile.toJson(), initial);
}

void CoordinatorLeaseTest::previewCancellationRetainsReservedHeadroomAcrossHandoff()
{
    constexpr quint64 maximum = std::numeric_limits<quint64>::max();
    LayoutEditingRepository repository(profile(), outputs(), manifests(), maximum - 3);
    const QJsonObject initial = repository.snapshot()->profile.toJson();
    {
        auto first = repository.tryAcquireCoordinator();
        QVERIFY(first);
        QVERIFY(first->execute(BeginPreviewCommand{maximum - 3}).succeeded());
        QVERIFY(first->execute(ConfigurePanelCommand{
            .expectedRevision = maximum - 2,
            .panelId = QStringLiteral("dock"),
            .layer = Profiles::Layer::Above,
            .hideMode = Profiles::HideMode::Always,
            .rows = 1,
            .thickness = 52,
            .length = 0.5,
        }).succeeded());
    }

    auto second = repository.tryAcquireCoordinator();
    QVERIFY(second);
    QCOMPARE(repository.snapshot()->revision, maximum - 1);
    QCOMPARE(second->execute(UndoCommand{maximum - 1}).error.code,
             EditingErrorCode::RevisionExhausted);
    QVERIFY(second->execute(CancelPreviewCommand{maximum - 1}).succeeded());
    QCOMPARE(repository.snapshot()->revision, maximum);
    QCOMPARE(repository.snapshot()->profile.toJson(), initial);
}

void CoordinatorLeaseTest::nonReadyRepositoryStillReportsItsCommandFailure_data()
{
    QTest::addColumn<bool>("invalidateCatalog");
    QTest::addColumn<int>("initializationCode");
    QTest::addColumn<quint64>("initialRevision");

    QTest::newRow("invalid-profile")
        << false << static_cast<int>(EditingErrorCode::InvalidProfile) << quint64(37);
    QTest::newRow("invalid-catalog")
        << true << static_cast<int>(EditingErrorCode::InvalidManifest) << quint64(91);
}

void CoordinatorLeaseTest::nonReadyRepositoryStillReportsItsCommandFailure()
{
    QFETCH(bool, invalidateCatalog);
    QFETCH(int, initializationCode);
    QFETCH(quint64, initialRevision);

    Profiles::LayoutProfile candidate = profile();
    QVector<Applets::AppletManifest> catalog = manifests();
    if (invalidateCatalog) {
        catalog[0].entryPoint.value.clear();
    } else {
        candidate.panels.clear();
    }

    LayoutEditingRepository repository(candidate,
                                       outputs(),
                                       catalog,
                                       initialRevision);
    QVERIFY(!repository.isReady());
    QVERIFY(!repository.snapshot());
    QCOMPARE(static_cast<int>(repository.initializationError().code),
             initializationCode);
    QVERIFY(!repository.initializationError().message.isEmpty());

    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    QVERIFY(!coordinator->committedProfile());
    QVERIFY(!coordinator->hasPreview());
    const EditingError initialization = repository.initializationError();
    const EditingResult result =
        coordinator->execute(BeginPreviewCommand{initialRevision + 5});
    QCOMPARE(result.kind, EditingCommandKind::BeginPreview);
    QCOMPARE(result.error.code, EditingErrorCode::RepositoryNotReady);
    QCOMPARE(result.error.message, initialization.message);
    QCOMPARE(result.error.panelId, initialization.panelId);
    QCOMPARE(result.error.appletId, initialization.appletId);
    QCOMPARE(result.previousRevision, initialRevision);
    QCOMPARE(result.revision, initialRevision);
    QVERIFY(!repository.snapshot());
    QVERIFY(!coordinator->committedProfile());
    QVERIFY(!coordinator->hasPreview());
}

QTEST_GUILESS_MAIN(CoordinatorLeaseTest)
#include "tst_coordinator_lease.moc"
