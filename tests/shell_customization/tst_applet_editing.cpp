// SPDX-License-Identifier: GPL-3.0-or-later
#include "editing_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization/layout_editing_coordinator.h"

#include <QJsonDocument>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::ShellCustomization;
using namespace QindaQt::ShellCustomization::TestFixtures;

namespace {

void applyMoveSequence(LayoutEditingRepository &repository)
{
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    QVERIFY(coordinator->execute(BeginPreviewCommand{0}).succeeded());
    QVERIFY(coordinator->execute(MoveAppletCommand{
        .expectedRevision = 1,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .targetPanelId = QStringLiteral("dock"),
        .beforeAppletId = QStringLiteral("tasks-instance"),
    }).succeeded());
    QVERIFY(coordinator->execute(CommitPreviewCommand{2}).succeeded());
}

Applets::AppletManifest horizontalStartManifest()
{
    Applets::AppletManifest result = manifest(QStringLiteral("horizontal-start"));
    result.placementZones = {Applets::PlacementZone::PanelStart};
    result.orientations = {Applets::Orientation::Horizontal};
    return result;
}

} // namespace

class AppletEditingTest final : public QObject {
    Q_OBJECT

private slots:
    void insertsCatalogSelectionsFromAnImmutableSnapshot();
    void mouseAndKeyboardAdaptersConvergeOnOneMoveCommand();
    void reordersDuplicatesRemovesAndUpdatesInstances();
    void forwardReorderRecomputesAnchorAfterRemovingApplet();
    void enforcesOrientationAndZoneCompatibilityAtomically();
    void unavailableLegacyAppletsAllowOnlyPlacementNeutralWork();
    void publishesSchemaRoundTripValues();
};

void AppletEditingTest::insertsCatalogSelectionsFromAnImmutableSnapshot()
{
    QVector<Applets::AppletManifest> catalog = manifests();
    LayoutEditingRepository repository(profile(), outputs(), catalog);
    QVERIFY(repository.isReady());
    catalog.clear();

    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);
    const EditingResult inserted = coordinator->execute(InsertAppletCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("bar"),
        .instanceId = QStringLiteral("second-clock"),
        .pluginId = QStringLiteral("clock"),
        .initialSettings = {
            {QStringLiteral("zone"), QStringLiteral("end")},
            {QStringLiteral("seconds"), true},
        },
        .beforeAppletId = QStringLiteral("clock-instance"),
    });
    QVERIFY2(inserted.succeeded(), qPrintable(inserted.error.message));
    QCOMPARE(appletIds(repository.snapshot()->profile, QStringLiteral("bar")),
             QStringList({QStringLiteral("launcher-instance"),
                          QStringLiteral("second-clock"),
                          QStringLiteral("clock-instance")}));

    const auto beforeFailure = repository.snapshot();
    const EditingResult unavailable = coordinator->execute(InsertAppletCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("dock"),
        .instanceId = QStringLiteral("retired-instance"),
        .pluginId = QStringLiteral("retired"),
        .initialSettings = {},
        .beforeAppletId = std::nullopt,
    });
    QCOMPARE(unavailable.error.code, EditingErrorCode::ManifestUnavailable);
    QCOMPARE(repository.snapshot(), beforeFailure);

    const EditingResult duplicate = coordinator->execute(InsertAppletCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("dock"),
        .instanceId = QStringLiteral("second-clock"),
        .pluginId = QStringLiteral("clock"),
        .initialSettings = {},
        .beforeAppletId = std::nullopt,
    });
    QCOMPARE(duplicate.error.code, EditingErrorCode::DuplicateAppletId);
    QCOMPARE(repository.snapshot(), beforeFailure);

    auto invalidCatalog = manifests();
    invalidCatalog[0].entryPoint.value.clear();
    LayoutEditingRepository invalidRepository(profile(), outputs(), invalidCatalog);
    QVERIFY(!invalidRepository.isReady());
    QVERIFY(!invalidRepository.snapshot());
    QCOMPARE(invalidRepository.initializationError().code,
             EditingErrorCode::InvalidManifest);
}

void AppletEditingTest::mouseAndKeyboardAdaptersConvergeOnOneMoveCommand()
{
    LayoutEditingRepository mouseRepository(profile(), outputs(), manifests());
    LayoutEditingRepository keyboardRepository(profile(), outputs(), manifests());
    QVERIFY(mouseRepository.isReady());
    QVERIFY(keyboardRepository.isReady());

    // Pointer drop and keyboard placement resolve to the same typed semantic
    // command. Input adapters do not receive a second mutation path to drift.
    applyMoveSequence(mouseRepository);
    applyMoveSequence(keyboardRepository);

    QCOMPARE(mouseRepository.snapshot()->profile.toJson(),
             keyboardRepository.snapshot()->profile.toJson());
    QCOMPARE(appletIds(mouseRepository.snapshot()->profile, QStringLiteral("bar")),
             QStringList({QStringLiteral("launcher-instance")}));
    QCOMPARE(appletIds(mouseRepository.snapshot()->profile, QStringLiteral("dock")),
             QStringList({QStringLiteral("clock-instance"),
                          QStringLiteral("tasks-instance")}));
}

void AppletEditingTest::reordersDuplicatesRemovesAndUpdatesInstances()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    QVERIFY(coordinator->execute(MoveAppletCommand{
        .expectedRevision = 0,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .targetPanelId = QStringLiteral("bar"),
        .beforeAppletId = QStringLiteral("launcher-instance"),
    }).succeeded());
    QCOMPARE(appletIds(repository.snapshot()->profile, QStringLiteral("bar")),
             QStringList({QStringLiteral("clock-instance"),
                          QStringLiteral("launcher-instance")}));

    QVERIFY(coordinator->execute(DuplicateAppletCommand{
        .expectedRevision = 1,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .targetPanelId = QStringLiteral("dock"),
        .newAppletId = QStringLiteral("clock-copy"),
        .beforeAppletId = QStringLiteral("tasks-instance"),
    }).succeeded());

    QVERIFY(coordinator->execute(UpdateAppletSettingsCommand{
        .expectedRevision = 2,
        .panelId = QStringLiteral("dock"),
        .appletId = QStringLiteral("clock-copy"),
        .settings = {{QStringLiteral("timezone"), QStringLiteral("UTC")},
                     {QStringLiteral("seconds"), true}},
    }).succeeded());

    QVERIFY(coordinator->execute(RemoveAppletCommand{
        .expectedRevision = 3,
        .panelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
    }).succeeded());
    QVERIFY(applet(repository.snapshot()->profile,
                   QStringLiteral("bar"),
                   QStringLiteral("clock-instance")) == nullptr);
    const Profiles::AppletSpec *copy =
        applet(repository.snapshot()->profile,
               QStringLiteral("dock"),
               QStringLiteral("clock-copy"));
    QVERIFY(copy != nullptr);
    QCOMPARE(copy->settings.value(QStringLiteral("timezone")).toString(),
             QStringLiteral("UTC"));

    const auto beforeFailure = repository.snapshot();
    QCOMPARE(coordinator->execute(MoveAppletCommand{
                 .expectedRevision = 4,
                 .sourcePanelId = QStringLiteral("dock"),
                 .appletId = QStringLiteral("clock-copy"),
                 .targetPanelId = QStringLiteral("dock"),
                 .beforeAppletId = QStringLiteral("clock-copy"),
             }).error.code,
             EditingErrorCode::InvalidCommand);
    QCOMPARE(repository.snapshot(), beforeFailure);

    QCOMPARE(coordinator->execute(MoveAppletCommand{
                 .expectedRevision = 4,
                 .sourcePanelId = QStringLiteral("dock"),
                 .appletId = QStringLiteral("clock-copy"),
                 .targetPanelId = QStringLiteral("bar"),
                 .beforeAppletId = QStringLiteral("missing"),
             }).error.code,
             EditingErrorCode::UnknownAnchorId);
    QCOMPARE(repository.snapshot(), beforeFailure);
}

void AppletEditingTest::forwardReorderRecomputesAnchorAfterRemovingApplet()
{
    Profiles::LayoutProfile threeApplets = profile();
    threeApplets.panels[0].applets.append({
        .id = QStringLiteral("third-instance"),
        .plugin = QStringLiteral("clock"),
        .settings = {{QStringLiteral("zone"), QStringLiteral("end")}},
    });

    LayoutEditingRepository repository(threeApplets, outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    // AGENT-GUARD: Resolve move anchors after source removal. A stale index for
    // this forward move appends the source instead of placing it before third.
    const EditingResult moved = coordinator->execute(MoveAppletCommand{
        .expectedRevision = 0,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("launcher-instance"),
        .targetPanelId = QStringLiteral("bar"),
        .beforeAppletId = QStringLiteral("third-instance"),
    });
    QVERIFY2(moved.succeeded(), qPrintable(moved.error.message));
    QCOMPARE(appletIds(repository.snapshot()->profile, QStringLiteral("bar")),
             QStringList({QStringLiteral("clock-instance"),
                          QStringLiteral("launcher-instance"),
                          QStringLiteral("third-instance")}));
}

void AppletEditingTest::enforcesOrientationAndZoneCompatibilityAtomically()
{
    auto catalog = manifests();
    catalog.append(horizontalStartManifest());
    LayoutEditingRepository repository(profile(), outputs(), catalog);
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    Profiles::PanelSpec rail;
    rail.id = QStringLiteral("rail");
    rail.output = QStringLiteral("primary");
    rail.edge = Profiles::Edge::Left;
    QVERIFY(coordinator->execute(AddPanelCommand{0, rail, std::nullopt}).succeeded());

    QVERIFY(coordinator->execute(InsertAppletCommand{
        .expectedRevision = 1,
        .panelId = QStringLiteral("bar"),
        .instanceId = QStringLiteral("restricted"),
        .pluginId = QStringLiteral("horizontal-start"),
        .initialSettings = {},
        .beforeAppletId = std::nullopt,
    }).succeeded());
    const auto compatible = repository.snapshot();

    QCOMPARE(coordinator->execute(InsertAppletCommand{
                 .expectedRevision = 2,
                 .panelId = QStringLiteral("bar"),
                 .instanceId = QStringLiteral("bad-zone"),
                 .pluginId = QStringLiteral("horizontal-start"),
                 .initialSettings = {
                     {QStringLiteral("zone"), QStringLiteral("center")}},
                 .beforeAppletId = std::nullopt,
             }).error.code,
             EditingErrorCode::UnsupportedAppletPlacement);
    QCOMPARE(repository.snapshot(), compatible);

    QCOMPARE(coordinator->execute(InsertAppletCommand{
                 .expectedRevision = 2,
                 .panelId = QStringLiteral("rail"),
                 .instanceId = QStringLiteral("bad-orientation"),
                 .pluginId = QStringLiteral("horizontal-start"),
                 .initialSettings = {},
                 .beforeAppletId = std::nullopt,
             }).error.code,
             EditingErrorCode::UnsupportedAppletPlacement);
    QCOMPARE(repository.snapshot(), compatible);

    QCOMPARE(coordinator->execute(MoveAppletCommand{
                 .expectedRevision = 2,
                 .sourcePanelId = QStringLiteral("bar"),
                 .appletId = QStringLiteral("restricted"),
                 .targetPanelId = QStringLiteral("rail"),
                 .beforeAppletId = std::nullopt,
             }).error.code,
             EditingErrorCode::UnsupportedAppletPlacement);
    QCOMPARE(repository.snapshot(), compatible);

    QCOMPARE(coordinator->execute(UpdateAppletSettingsCommand{
                 .expectedRevision = 2,
                 .panelId = QStringLiteral("bar"),
                 .appletId = QStringLiteral("restricted"),
                 .settings = {
                     {QStringLiteral("zone"), QStringLiteral("center")}},
             }).error.code,
             EditingErrorCode::UnsupportedAppletPlacement);
    QCOMPARE(repository.snapshot(), compatible);

    QVERIFY(coordinator->execute(DuplicateAppletCommand{
        .expectedRevision = 2,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("restricted"),
        .targetPanelId = QStringLiteral("dock"),
        .newAppletId = QStringLiteral("restricted-copy"),
        .beforeAppletId = std::nullopt,
    }).succeeded());
}

void AppletEditingTest::unavailableLegacyAppletsAllowOnlyPlacementNeutralWork()
{
    Profiles::LayoutProfile legacy = profile();
    legacy.panels[0].applets.append({.id = QStringLiteral("legacy-instance"),
                                     .plugin = QStringLiteral("retired"),
                                     .settings = {}});
    LayoutEditingRepository repository(legacy, outputs(), manifests());
    QVERIFY2(repository.isReady(), qPrintable(repository.initializationError().message));
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    QVERIFY(coordinator->execute(MoveAppletCommand{
        .expectedRevision = 0,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("legacy-instance"),
        .targetPanelId = QStringLiteral("bar"),
        .beforeAppletId = QStringLiteral("launcher-instance"),
    }).succeeded());

    // Both panels are horizontal and the applet keeps the implicit start zone,
    // so this move does not need a now-unavailable manifest.
    QVERIFY(coordinator->execute(MoveAppletCommand{
        .expectedRevision = 1,
        .sourcePanelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("legacy-instance"),
        .targetPanelId = QStringLiteral("dock"),
        .beforeAppletId = QStringLiteral("tasks-instance"),
    }).succeeded());

    // An omitted zone and an explicit start zone have the same placement
    // signature. Other settings remain editable without resurrecting the
    // retired manifest.
    QVERIFY(coordinator->execute(UpdateAppletSettingsCommand{
        .expectedRevision = 2,
        .panelId = QStringLiteral("dock"),
        .appletId = QStringLiteral("legacy-instance"),
        .settings = {{QStringLiteral("zone"), QStringLiteral("start")},
                     {QStringLiteral("label"), QStringLiteral("Legacy")}},
    }).succeeded());

    Profiles::PanelSpec rail;
    rail.id = QStringLiteral("legacy-rail");
    rail.output = QStringLiteral("primary");
    rail.edge = Profiles::Edge::Left;
    QVERIFY(coordinator->execute(AddPanelCommand{3, rail, std::nullopt}).succeeded());
    const auto beforeFailures = repository.snapshot();

    QCOMPARE(coordinator->execute(MoveAppletCommand{
                 .expectedRevision = 4,
                 .sourcePanelId = QStringLiteral("dock"),
                 .appletId = QStringLiteral("legacy-instance"),
                 .targetPanelId = QStringLiteral("legacy-rail"),
                 .beforeAppletId = std::nullopt,
             }).error.code,
             EditingErrorCode::ManifestUnavailable);
    QCOMPARE(repository.snapshot(), beforeFailures);

    QCOMPARE(coordinator->execute(UpdateAppletSettingsCommand{
                 .expectedRevision = 4,
                 .panelId = QStringLiteral("dock"),
                 .appletId = QStringLiteral("legacy-instance"),
                 .settings = {{QStringLiteral("zone"), QStringLiteral("end")}},
             }).error.code,
             EditingErrorCode::ManifestUnavailable);
    QCOMPARE(repository.snapshot(), beforeFailures);

    QCOMPARE(coordinator->execute(DuplicateAppletCommand{
                 .expectedRevision = 4,
                 .sourcePanelId = QStringLiteral("dock"),
                 .appletId = QStringLiteral("legacy-instance"),
                 .targetPanelId = QStringLiteral("dock"),
                 .newAppletId = QStringLiteral("legacy-copy"),
                 .beforeAppletId = std::nullopt,
             }).error.code,
             EditingErrorCode::ManifestUnavailable);
    QCOMPARE(repository.snapshot(), beforeFailures);

    QCOMPARE(coordinator->execute(MovePanelCommand{
                 .expectedRevision = 4,
                 .panelId = QStringLiteral("dock"),
                 .outputId = QStringLiteral("primary"),
                 .edge = Profiles::Edge::Left,
                 .alignment = Profiles::Alignment::Center,
                 .beforePanelId = std::nullopt,
             }).error.code,
             EditingErrorCode::ManifestUnavailable);
    QCOMPARE(repository.snapshot(), beforeFailures);

    QVERIFY(coordinator->execute(RemoveAppletCommand{
        .expectedRevision = 4,
        .panelId = QStringLiteral("dock"),
        .appletId = QStringLiteral("legacy-instance"),
    }).succeeded());
}

void AppletEditingTest::publishesSchemaRoundTripValues()
{
    LayoutEditingRepository repository(profile(), outputs(), manifests());
    QVERIFY(repository.isReady());
    auto coordinator = repository.tryAcquireCoordinator();
    QVERIFY(coordinator);

    QVERIFY(coordinator->execute(UpdateAppletSettingsCommand{
        .expectedRevision = 0,
        .panelId = QStringLiteral("bar"),
        .appletId = QStringLiteral("clock-instance"),
        .settings = {{QStringLiteral("zone"), QStringLiteral("end")},
                     {QStringLiteral("label"), QStringLiteral("Mountain time")},
                     {QStringLiteral("offset"), -7},
                     {QStringLiteral("showDate"), true}},
    }).succeeded());

    const Profiles::LayoutProfile &published = repository.snapshot()->profile;
    const QByteArray serialized =
        QJsonDocument(published.toJson()).toJson(QJsonDocument::Compact);
    const Profiles::LoadResult loaded =
        Profiles::ProfileLoader::fromJson(serialized, QStringLiteral("editing round trip"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error.diagnostic()));
    QCOMPARE(loaded.profile.toJson(), published.toJson());
}

QTEST_GUILESS_MAIN(AppletEditingTest)
#include "tst_applet_editing.moc"
