// SPDX-License-Identifier: GPL-3.0-or-later
#include "editing_test_fixtures.h"

#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace QindaQt;
using namespace QindaQt::ShellCustomization;
using namespace QindaQt::ShellCustomization::TestFixtures;

namespace {

const ShellLayout::PanelSurface *surface(
    const ShellLayout::PanelLayoutResult &layout,
    const QString &panelId,
    const QString &outputId)
{
    const auto found = std::find_if(
        layout.surfaces.cbegin(),
        layout.surfaces.cend(),
        [&panelId, &outputId](const ShellLayout::PanelSurface &candidate) {
            return candidate.panelId == panelId && candidate.outputId == outputId;
        });
    return found == layout.surfaces.cend() ? nullptr : &*found;
}

} // namespace

class PanelEditingTest final : public QObject {
    Q_OBJECT

private slots:
    void addsMovesConfiguresAndRemovesPanelsWithStableContents();
    void reorderChangesSameEdgeStackGeometry();
    void forwardReorderRecomputesAnchorAfterRemovingPanel();
    void rejectsStaleDuplicatesSelfUnknownAndNoChange();
    void rejectsInvalidMultiOutputCandidatesWithoutPublication();
    void rejectsRemovingTheLastPanelThroughProfileValidation();
};

void PanelEditingTest::addsMovesConfiguresAndRemovesPanelsWithStableContents()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY2(repository.isReady(), qPrintable(repository.initializationError().message));
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    Profiles::PanelSpec utility;
    utility.id = QStringLiteral("utility");
    utility.output = QStringLiteral("external");
    utility.edge = Profiles::Edge::Right;
    utility.alignment = Profiles::Alignment::End;
    utility.length = 0.5;
    utility.thickness = 40;

    const EditingResult added = coordinator->execute(AddPanelCommand{
        .expectedRevision = 0,
        .panel = utility,
        .beforePanelId = QStringLiteral("dock"),
    });
    QVERIFY2(added.succeeded(), qPrintable(added.error.message));
    QCOMPARE(repository.snapshot()->profile.panels[1].id, QStringLiteral("utility"));

    const EditingResult moved = coordinator->execute(MovePanelCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("utility"),
        .outputId = QStringLiteral("primary"),
        .edge = Profiles::Edge::Left,
        .alignment = Profiles::Alignment::Start,
        .beforePanelId = QStringLiteral("bar"),
    });
    QVERIFY2(moved.succeeded(), qPrintable(moved.error.message));
    QCOMPARE(repository.snapshot()->profile.panels.constFirst().id,
             QStringLiteral("utility"));

    const EditingResult configured = coordinator->execute(ConfigurePanelCommand{
        .expectedRevision = 2,
        .panelId = QStringLiteral("utility"),
        .layer = Profiles::Layer::Normal,
        .hideMode = Profiles::HideMode::DodgeActive,
        .rows = 2,
        .thickness = 44,
        .length = 0.75,
    });
    QVERIFY2(configured.succeeded(), qPrintable(configured.error.message));
    const Profiles::PanelSpec *configuredPanel =
        panel(repository.snapshot()->profile, QStringLiteral("utility"));
    QVERIFY(configuredPanel != nullptr);
    QCOMPARE(configuredPanel->id, QStringLiteral("utility"));
    QCOMPARE(configuredPanel->output, QStringLiteral("primary"));
    QVERIFY(configuredPanel->edge == Profiles::Edge::Left);
    QVERIFY(configuredPanel->alignment == Profiles::Alignment::Start);
    QVERIFY(configuredPanel->layer == Profiles::Layer::Normal);
    QVERIFY(configuredPanel->hideMode == Profiles::HideMode::DodgeActive);
    QCOMPARE(configuredPanel->rows, 2);
    QCOMPARE(configuredPanel->thickness, 44);
    QCOMPARE(configuredPanel->length, 0.75);
    QVERIFY(configuredPanel->applets.isEmpty());

    const EditingResult removed = coordinator->execute(
        RemovePanelCommand{3, QStringLiteral("utility")});
    QVERIFY2(removed.succeeded(), qPrintable(removed.error.message));
    QCOMPARE(repository.snapshot()->revision, quint64(4));
    QVERIFY(panel(repository.snapshot()->profile, QStringLiteral("utility")) == nullptr);
}

void PanelEditingTest::reorderChangesSameEdgeStackGeometry()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    const EditingResult movedFirst = coordinator->execute(MovePanelCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("external"),
        .edge = Profiles::Edge::Top,
        .alignment = Profiles::Alignment::Fill,
        .beforePanelId = QStringLiteral("bar"),
    });
    QVERIFY2(movedFirst.succeeded(), qPrintable(movedFirst.error.message));
    const auto firstLayout = repository.snapshot()->layout;
    const auto *dockFirst = surface(firstLayout, QStringLiteral("dock"),
                                    QStringLiteral("external"));
    const auto *barSecond = surface(firstLayout, QStringLiteral("bar"),
                                    QStringLiteral("external"));
    QVERIFY(dockFirst != nullptr);
    QVERIFY(barSecond != nullptr);
    QCOMPARE(dockFirst->stackIndex, qsizetype(0));
    QCOMPARE(barSecond->stackIndex, qsizetype(1));
    QCOMPARE(dockFirst->geometry.y(), 0);
    QCOMPARE(barSecond->geometry.y(), dockFirst->geometry.bottom() + 1);

    const EditingResult movedLast = coordinator->execute(MovePanelCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("dock"),
        .outputId = QStringLiteral("external"),
        .edge = Profiles::Edge::Top,
        .alignment = Profiles::Alignment::Fill,
        .beforePanelId = std::nullopt,
    });
    QVERIFY2(movedLast.succeeded(), qPrintable(movedLast.error.message));
    const auto secondLayout = repository.snapshot()->layout;
    const auto *barFirst = surface(secondLayout, QStringLiteral("bar"),
                                   QStringLiteral("external"));
    const auto *dockSecond = surface(secondLayout, QStringLiteral("dock"),
                                     QStringLiteral("external"));
    QVERIFY(barFirst != nullptr);
    QVERIFY(dockSecond != nullptr);
    QCOMPARE(barFirst->stackIndex, qsizetype(0));
    QCOMPARE(dockSecond->stackIndex, qsizetype(1));
    QCOMPARE(dockSecond->geometry.y(), barFirst->geometry.bottom() + 1);
}

void PanelEditingTest::forwardReorderRecomputesAnchorAfterRemovingPanel()
{
    Profiles::LayoutProfile threePanels = profile();
    Profiles::PanelSpec utility;
    utility.id = QStringLiteral("utility");
    utility.output = QStringLiteral("external");
    utility.edge = Profiles::Edge::Right;
    threePanels.panels.append(utility);

    LayoutEditingRepository repository(threePanels, outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    // AGENT-GUARD: Resolve move anchors after source removal. A stale index for
    // this forward move appends the source instead of placing it before utility.
    const EditingResult moved = coordinator->execute(MovePanelCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("bar"),
        .outputId = QStringLiteral("*"),
        .edge = Profiles::Edge::Top,
        .alignment = Profiles::Alignment::Fill,
        .beforePanelId = QStringLiteral("utility"),
    });
    QVERIFY2(moved.succeeded(), qPrintable(moved.error.message));

    const auto &panels = repository.snapshot()->profile.panels;
    QCOMPARE(panels.size(), qsizetype(3));
    QCOMPARE(panels[0].id, QStringLiteral("dock"));
    QCOMPARE(panels[1].id, QStringLiteral("bar"));
    QCOMPARE(panels[2].id, QStringLiteral("utility"));
}

void PanelEditingTest::rejectsStaleDuplicatesSelfUnknownAndNoChange()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const auto initial = repository.snapshot();

    QCOMPARE(coordinator->execute(RemovePanelCommand{7, QStringLiteral("bar")})
                 .error.code,
             EditingErrorCode::StaleRevision);

    Profiles::PanelSpec duplicate;
    duplicate.id = QStringLiteral("bar");
    QCOMPARE(coordinator->execute(AddPanelCommand{0, duplicate, std::nullopt})
                 .error.code,
             EditingErrorCode::DuplicatePanelId);

    QCOMPARE(coordinator->execute(MovePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .outputId = QStringLiteral("primary"),
                 .edge = Profiles::Edge::Bottom,
                 .alignment = Profiles::Alignment::Center,
                 .beforePanelId = QStringLiteral("dock"),
             }).error.code,
             EditingErrorCode::InvalidCommand);

    QCOMPARE(coordinator->execute(MovePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .outputId = QStringLiteral("primary"),
                 .edge = Profiles::Edge::Bottom,
                 .alignment = Profiles::Alignment::Center,
                 .beforePanelId = QStringLiteral("missing"),
             }).error.code,
             EditingErrorCode::UnknownAnchorId);

    QCOMPARE(coordinator->execute(MovePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .outputId = QStringLiteral("primary"),
                 .edge = Profiles::Edge::Bottom,
                 .alignment = Profiles::Alignment::Center,
                 .beforePanelId = std::nullopt,
             }).error.code,
             EditingErrorCode::NoChange);

    QCOMPARE(coordinator->execute(ConfigurePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .layer = Profiles::Layer::Overlay,
                 .hideMode = Profiles::HideMode::Never,
                 .rows = 1,
                 .thickness = 48,
                 .length = 0.6,
             }).error.code,
             EditingErrorCode::NoChange);
    QCOMPARE(repository.snapshot(), initial);
}

void PanelEditingTest::rejectsInvalidMultiOutputCandidatesWithoutPublication()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const auto initial = repository.snapshot();

    QCOMPARE(coordinator->execute(MovePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .outputId = QStringLiteral("disconnected"),
                 .edge = Profiles::Edge::Bottom,
                 .alignment = Profiles::Alignment::Center,
                 .beforePanelId = std::nullopt,
             }).error.code,
             EditingErrorCode::InvalidLayout);
    QCOMPARE(repository.snapshot(), initial);

    QCOMPARE(coordinator->execute(ConfigurePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .layer = Profiles::Layer::Overlay,
                 .hideMode = Profiles::HideMode::Never,
                 .rows = 0,
                 .thickness = 48,
                 .length = 0.6,
             }).error.code,
             EditingErrorCode::InvalidProfile);
    QCOMPARE(repository.snapshot(), initial);

    QCOMPARE(coordinator->execute(ConfigurePanelCommand{
                 .expectedRevision = 0,
                 .panelId = QStringLiteral("dock"),
                 .layer = Profiles::Layer::Overlay,
                 .hideMode = Profiles::HideMode::Never,
                 .rows = 1,
                 .thickness = 48,
                 .length = std::numeric_limits<double>::quiet_NaN(),
             }).error.code,
             EditingErrorCode::InvalidProfile);
    QCOMPARE(repository.snapshot(), initial);

    Profiles::PanelSpec deep;
    deep.id = QStringLiteral("deep-one");
    deep.output = QStringLiteral("primary");
    deep.rows = 4;
    deep.thickness = 192;
    QVERIFY(coordinator->execute(AddPanelCommand{0, deep, std::nullopt}).succeeded());
    const auto beforeConstrained = repository.snapshot();

    Profiles::PanelSpec tooDeep = deep;
    tooDeep.id = QStringLiteral("too-deep");
    QCOMPARE(coordinator->execute(AddPanelCommand{1, tooDeep, std::nullopt})
                 .error.code,
             EditingErrorCode::InvalidLayout);
    QCOMPARE(repository.snapshot(), beforeConstrained);
}

void PanelEditingTest::rejectsRemovingTheLastPanelThroughProfileValidation()
{
    Profiles::LayoutProfile single = profile();
    single.panels = {single.panels.constFirst()};
    LayoutEditingRepository repository(single, outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const auto before = repository.snapshot();

    const EditingResult result = coordinator->execute(
        RemovePanelCommand{0, QStringLiteral("bar")});
    QCOMPARE(result.error.code, EditingErrorCode::InvalidProfile);
    QCOMPARE(repository.snapshot(), before);
}

QTEST_GUILESS_MAIN(PanelEditingTest)
#include "tst_panel_editing.moc"
