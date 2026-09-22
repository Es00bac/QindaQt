// SPDX-License-Identifier: GPL-3.0-or-later
#include "livecustomizationcontroller.h"
#include "livecustomizationmodel.h"

#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt;
using QindaQt::Shell::LiveCustomizationController;
namespace Model = QindaQt::Shell::LiveCustomizationModel;

namespace {

Profiles::LayoutProfile fixtureProfile()
{
    Profiles::LayoutProfile profile;
    profile.id = QStringLiteral("live-fixture");
    profile.name = QStringLiteral("Live fixture");
    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.edge = Profiles::Edge::Top;
    bar.thickness = 30;
    bar.applets = {
        {.id = QStringLiteral("launcher-1"), .plugin = QStringLiteral("launcher"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}},
        {.id = QStringLiteral("clock-1"), .plugin = QStringLiteral("clock"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}},
        {.id = QStringLiteral("status-1"), .plugin = QStringLiteral("system-status"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("end")}}},
    };
    Profiles::PanelSpec tray;
    tray.id = QStringLiteral("tray");
    tray.edge = Profiles::Edge::Bottom;
    tray.thickness = 40;
    tray.applets = {
        {.id = QStringLiteral("tasks-1"), .plugin = QStringLiteral("task-list"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("center")}}},
    };
    profile.panels = {bar, tray};
    profile.desktopApplets = {
        {.id = QStringLiteral("desktop-icons"), .plugin = QStringLiteral("desktop-icons"),
         .settings = {}},
    };
    return profile;
}

QVector<ShellLayout::LogicalOutput> fixtureOutputs()
{
    return {{QStringLiteral("OUT-1"), QRect(0, 0, 1920, 1080), 1.0}};
}

Profiles::LayoutProfile readWritten(const QString &directory, const QString &id)
{
    QFile file(QDir(directory).filePath(
        ShellCustomizationEditor::UserProfileStore::fileNameForId(id)));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto loaded = Profiles::ProfileLoader::fromJson(file.readAll(), QStringLiteral("test"));
    return loaded.ok ? loaded.profile : Profiles::LayoutProfile{};
}

QStringList appletIds(const Profiles::LayoutProfile &profile, const QString &panelId)
{
    QStringList ids;
    if (const auto *panel = Model::findPanel(profile, panelId)) {
        for (const auto &applet : panel->applets) {
            ids.append(applet.id);
        }
    }
    return ids;
}

} // namespace

class LiveCustomizationControllerTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void menuActionsApplyOneGestureAndPersist();
    void adoptionKeepsHistoryForOwnWritesAndRebuildsForForeignOnes();
    void paletteAndSettingRowsFollowTheManifests();
    void panelOptionsRouteThroughMoveAndConfigure();
    void editModeDragsResolveAndPersist();
    void chordDefaultsAndModifiers();
    void modelArithmetic();
    void survivesAPinnedDisplayGoingAway();

private:
    Applets::ManifestCatalog m_catalog;
};

void LiveCustomizationControllerTest::initTestCase()
{
    QString error;
    QVERIFY2(m_catalog.loadDirectory(QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), &error),
             qPrintable(error));
}

void LiveCustomizationControllerTest::menuActionsApplyOneGestureAndPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    QVERIFY(!controller.available());
    QVERIFY(!controller.removeApplet(QStringLiteral("bar"), QStringLiteral("clock-1")));
    controller.adoptProfile(fixtureProfile());
    QVERIFY(controller.available());
    QVERIFY(!controller.canUndo());
    QSignalSpy reports(&controller, &LiveCustomizationController::actionReported);

    // Move to end: the clock leaves the start zone and appends after status.
    QVERIFY2(controller.moveAppletToZone(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                         QStringLiteral("end")),
             qPrintable(controller.statusText()));
    auto written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-1"), QStringLiteral("status-1"),
                          QStringLiteral("clock-1")}));
    QCOMPARE(Model::findApplet(*Model::findPanel(written, QStringLiteral("bar")),
                               QStringLiteral("clock-1"))->settings.value(QStringLiteral("zone")).toString(),
             QStringLiteral("end"));
    QVERIFY(controller.canUndo());
    QCOMPARE(reports.size(), 1);
    QCOMPARE(reports.last().at(0).toBool(), true);
    QCOMPARE(reports.last().at(1).toString(), QStringLiteral("move-to-zone"));

    // Move left within the end zone swaps with status.
    QVERIFY(controller.moveAppletStep(QStringLiteral("bar"), QStringLiteral("clock-1"), -1));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-1"), QStringLiteral("clock-1"),
                          QStringLiteral("status-1")}));
    // At the zone edge the step is refused without an engine call.
    QVERIFY(!controller.moveAppletStep(QStringLiteral("bar"), QStringLiteral("clock-1"), -1));
    QVERIFY(!controller.statusText().isEmpty());

    // Move to another panel keeps the zone.
    QVERIFY(controller.moveAppletToPanel(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                         QStringLiteral("tray")));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("tray")),
             (QStringList{QStringLiteral("tasks-1"), QStringLiteral("clock-1")}));

    // Remove, add, undo, redo: each one durable step.
    QVERIFY(controller.removeApplet(QStringLiteral("tray"), QStringLiteral("clock-1")));
    QVERIFY(controller.addApplet(QStringLiteral("bar"), QStringLiteral("center"),
                                 QStringLiteral("show-desktop")));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QVERIFY(appletIds(written, QStringLiteral("bar")).contains(QStringLiteral("show-desktop-instance-1")));
    QVERIFY(controller.undo());
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QVERIFY(!appletIds(written, QStringLiteral("bar")).contains(QStringLiteral("show-desktop-instance-1")));
    QVERIFY(controller.canRedo());
    QVERIFY(controller.redo());
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QVERIFY(appletIds(written, QStringLiteral("bar")).contains(QStringLiteral("show-desktop-instance-1")));

    // Panels: add at the left, remove it again.
    QVERIFY(controller.addPanel(QStringLiteral("left")));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(written.panels.size(), 3);
    QVERIFY(Model::findPanel(written, QStringLiteral("panel-1")) != nullptr);
    QVERIFY(Model::findPanel(written, QStringLiteral("panel-1"))->edge == Profiles::Edge::Left);
    QVERIFY(!controller.addPanel(QStringLiteral("sideways")));
    QVERIFY(controller.removePanel(QStringLiteral("panel-1")));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(written.panels.size(), 2);
    QVERIFY(!controller.removePanel(QStringLiteral("missing")));
    QCOMPARE(reports.last().at(0).toBool(), false);
    QCOMPARE(reports.last().at(1).toString(), QStringLiteral("remove-panel"));
}

void LiveCustomizationControllerTest::adoptionKeepsHistoryForOwnWritesAndRebuildsForForeignOnes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    controller.adoptProfile(fixtureProfile());
    QVERIFY(controller.removeApplet(QStringLiteral("bar"), QStringLiteral("clock-1")));
    QVERIFY(controller.canUndo());

    // The store watcher hands our own write back: history survives.
    controller.adoptProfile(readWritten(directory.path(), QStringLiteral("live-fixture")));
    QVERIFY(controller.available());
    QVERIFY(controller.canUndo());

    // A foreign edit (the Settings route, say) rebuilds the session.
    Profiles::LayoutProfile foreign = fixtureProfile();
    foreign.panels.first().thickness = 44;
    controller.adoptProfile(foreign);
    QVERIFY(controller.available());
    QVERIFY(!controller.canUndo());
    QCOMPARE(controller.panelOptions(QStringLiteral("bar")).value(QStringLiteral("thickness")).toInt(), 44);

    // Regression: an output hotplug must leave the controller offering the
    // chord. available() is what disables AppletEditHandle's TapHandler and
    // PanelLiveCustomization's menu entry points, and those are the only
    // callers that would ever reach the lazy ensureHost() rebuild. Staling
    // without rebuilding therefore deadlocked the UI - one display hotplug
    // removed Meta+right-click customization for the rest of the session.
    QVERIFY(controller.removeApplet(QStringLiteral("bar"), QStringLiteral("status-1")));
    QVERIFY(controller.canUndo());
    controller.outputGenerationChanged();
    QVERIFY(controller.available());
    // The session really was replaced: the stale edit run's history is gone.
    QVERIFY(!controller.canUndo());
    QVERIFY(controller.removeApplet(QStringLiteral("bar"), QStringLiteral("clock-1")));
    QVERIFY(controller.available());
}

void LiveCustomizationControllerTest::survivesAPinnedDisplayGoingAway()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    // The live shape this regressed in: a panel pinned to one display (the
    // r10 dock is pinned to the built-in output) while that display is
    // unplugged or being reconfigured.
    Profiles::LayoutProfile profile = fixtureProfile();
    profile.panels[1].output = QStringLiteral("OUT-2");

    bool pinnedDisplayPresent = true;
    auto outputs = [&pinnedDisplayPresent] {
        QVector<ShellLayout::LogicalOutput> result = fixtureOutputs();
        if (pinnedDisplayPresent) {
            result.append({QStringLiteral("OUT-2"), QRect(1920, 0, 1920, 1080), 1.0});
        }
        return result;
    };

    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           outputs, nullptr, nullptr);
    controller.adoptProfile(profile);
    QVERIFY(controller.available());

    // Unplug it. available() is what gates every chord entry point, so a
    // false here is the whole defect: no panel menu, no applet menu, no
    // desktop menu, until something happens to rebuild the editor host.
    pinnedDisplayPresent = false;
    controller.outputGenerationChanged();
    QVERIFY2(controller.available(), qPrintable(controller.statusText()));

    // The absent display's panel is not editable - it has nowhere to be.
    QVERIFY(controller.panelIds().contains(QStringLiteral("bar")));
    QVERIFY(!controller.panelIds().contains(QStringLiteral("tray")));

    // Customization still works on the display that is actually there.
    QVERIFY2(controller.removeApplet(QStringLiteral("bar"), QStringLiteral("clock-1")),
             qPrintable(controller.statusText()));

    // ...and applying that edit did not erase the absent display's panel.
    // The stored profile is the only record that the user configured it.
    const auto written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("bar")),
             (QStringList{QStringLiteral("launcher-1"), QStringLiteral("status-1")}));
    QVERIFY2(Model::findPanel(written, QStringLiteral("tray")) != nullptr,
             "the panel pinned to the unplugged display was erased by Apply");
    QCOMPARE(appletIds(written, QStringLiteral("tray")),
             QStringList{QStringLiteral("tasks-1")});
    // Stored panel order survives the round trip.
    QCOMPARE(written.panels.size(), 2);
    QCOMPARE(written.panels.at(1).id, QStringLiteral("tray"));

    // Plugging it back in brings the panel back into the editor.
    pinnedDisplayPresent = true;
    controller.adoptProfile(written);
    controller.outputGenerationChanged();
    QVERIFY2(controller.available(), qPrintable(controller.statusText()));
    QVERIFY(controller.panelIds().contains(QStringLiteral("tray")));
}

void LiveCustomizationControllerTest::paletteAndSettingRowsFollowTheManifests()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    controller.adoptProfile(fixtureProfile());

    const QVariantList palette = controller.palette(QStringLiteral("bar"));
    QVERIFY(palette.size() > 3);
    QStringList pluginIds;
    for (const auto &row : palette) {
        pluginIds.append(row.toMap().value(QStringLiteral("pluginId")).toString());
    }
    QVERIFY(pluginIds.contains(QStringLiteral("clock")));
    QVERIFY(!pluginIds.contains(QStringLiteral("desktop-icons")));
    QCOMPARE(controller.palette(QStringLiteral("missing")).size(), 0);
    QCOMPARE(controller.appletDisplayName(QStringLiteral("bar"), QStringLiteral("clock-1")),
             QStringLiteral("Clock"));
    QCOMPARE(controller.appletDisplayName(QStringLiteral("@desktop"), QStringLiteral("desktop-icons")),
             QStringLiteral("Desktop Icons"));

    // Typed rows for the clock: three closed choices and two switches; the
    // zone is never offered as a setting.
    const QVariantList rows = controller.appletSettingRows(QStringLiteral("bar"), QStringLiteral("clock-1"));
    QStringList keys;
    for (const auto &row : rows) {
        keys.append(row.toMap().value(QStringLiteral("key")).toString());
    }
    QVERIFY(keys.contains(QStringLiteral("format")));
    QVERIFY(keys.contains(QStringLiteral("showSeconds")));
    QVERIFY(!keys.contains(QStringLiteral("zone")));

    // Validation mirrors the Settings route: wrong kinds and unknown keys
    // are refused before any engine call; a valid value persists.
    QVERIFY(!controller.setAppletSetting(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                         QStringLiteral("format"), QStringLiteral("binary")));
    QVERIFY(!controller.setAppletSetting(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                         QStringLiteral("showSeconds"), QStringLiteral("yes")));
    QVERIFY(!controller.setAppletSetting(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                         QStringLiteral("nope"), true));
    QVERIFY(controller.setAppletSetting(QStringLiteral("bar"), QStringLiteral("clock-1"),
                                        QStringLiteral("showSeconds"), true));
    auto written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    const auto *clock = Model::findApplet(*Model::findPanel(written, QStringLiteral("bar")),
                                          QStringLiteral("clock-1"));
    QCOMPARE(clock->settings.value(QStringLiteral("showSeconds")).toBool(), true);
    QCOMPARE(clock->settings.value(QStringLiteral("zone")).toString(), QStringLiteral("start"));

    // Desktop applets are reachable through the engine's "@desktop" owner.
    const QVariantList iconRows = controller.appletSettingRows(QStringLiteral("@desktop"),
                                                               QStringLiteral("desktop-icons"));
    QVERIFY(iconRows.size() >= 4);
    QVERIFY(controller.setAppletSetting(QStringLiteral("@desktop"), QStringLiteral("desktop-icons"),
                                        QStringLiteral("iconSize"), 64));
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(written.desktopApplets.first().settings.value(QStringLiteral("iconSize")).toInt(), 64);
    QVERIFY(!controller.setAppletSetting(QStringLiteral("@desktop"), QStringLiteral("desktop-icons"),
                                         QStringLiteral("iconSize"), 999));
}

void LiveCustomizationControllerTest::panelOptionsRouteThroughMoveAndConfigure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    controller.adoptProfile(fixtureProfile());
    QCOMPARE(controller.panelIds(), (QVariantList{QStringLiteral("bar"), QStringLiteral("tray")}));

    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("thickness"), 36));
    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("hideMode"),
                                      QStringLiteral("intelligent")));
    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("edge"),
                                      QStringLiteral("bottom")));
    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("alignment"),
                                      QStringLiteral("center")));
    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("length"), 0.5));
    QVERIFY(!controller.configurePanel(QStringLiteral("bar"), QStringLiteral("edge"),
                                       QStringLiteral("diagonal")));
    QVERIFY(!controller.configurePanel(QStringLiteral("bar"), QStringLiteral("colour"), 1));
    QVERIFY(!controller.configurePanel(QStringLiteral("missing"), QStringLiteral("rows"), 2));
    // An unchanged edge is a no-op success, not an engine "no change" failure.
    QVERIFY(controller.configurePanel(QStringLiteral("bar"), QStringLiteral("edge"),
                                      QStringLiteral("bottom")));

    const auto written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    const auto *bar = Model::findPanel(written, QStringLiteral("bar"));
    QVERIFY(bar != nullptr);
    QCOMPARE(bar->thickness, 36);
    QVERIFY(bar->hideMode == Profiles::HideMode::Intelligent);
    QVERIFY(bar->edge == Profiles::Edge::Bottom);
    QVERIFY(bar->alignment == Profiles::Alignment::Center);
    QCOMPARE(bar->length, 0.5);
    const QVariantMap options = controller.panelOptions(QStringLiteral("bar"));
    QCOMPARE(options.value(QStringLiteral("edge")).toString(), QStringLiteral("bottom"));
    QCOMPARE(options.value(QStringLiteral("appletCount")).toInt(), 3);
}

void LiveCustomizationControllerTest::editModeDragsResolveAndPersist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    controller.adoptProfile(fixtureProfile());
    QSignalSpy editMode(&controller, &LiveCustomizationController::editModeChanged);
    controller.enterEditMode();
    QVERIFY(controller.editMode());
    QCOMPARE(editMode.size(), 1);

    // Solved surfaces: the top bar spans the output; the tray sits below.
    const QVariantMap bar = controller.panelSurface(QStringLiteral("OUT-1"), QStringLiteral("bar"));
    QCOMPARE(bar.value(QStringLiteral("panelId")).toString(), QStringLiteral("bar"));
    QCOMPARE(bar.value(QStringLiteral("horizontal")).toBool(), true);
    const QVariantMap under = controller.panelSurfaceAt(QStringLiteral("OUT-1"), 960, 1075);
    QCOMPARE(under.value(QStringLiteral("panelId")).toString(), QStringLiteral("tray"));
    QVERIFY(controller.panelSurfaceAt(QStringLiteral("OUT-1"), 960, 540).isEmpty());
    QVERIFY(controller.panelSurfaceAt(QStringLiteral("OUT-9"), 960, 1075).isEmpty());

    // A drag of the clock onto the tray's end zone: arm/begin, hover, drop.
    QVERIFY(!controller.dropApplet());
    QVERIFY(controller.beginAppletDrag(QStringLiteral("bar"), QStringLiteral("clock-1")));
    QVERIFY(controller.dragActive());
    QVERIFY(controller.hoverDropTarget(QStringLiteral("tray"), QStringLiteral("end"), QString()));
    QVERIFY(controller.dropAccepted());
    QVERIFY(controller.dropApplet());
    QVERIFY(!controller.dragActive());
    auto written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("tray")),
             (QStringList{QStringLiteral("tasks-1"), QStringLiteral("clock-1")}));
    QCOMPARE(Model::findApplet(*Model::findPanel(written, QStringLiteral("tray")),
                               QStringLiteral("clock-1"))->settings.value(QStringLiteral("zone")).toString(),
             QStringLiteral("end"));

    // A cancelled drag changes nothing; leaving edit mode cancels an open one.
    QVERIFY(controller.beginAppletDrag(QStringLiteral("tray"), QStringLiteral("clock-1")));
    QVERIFY(controller.hoverDropTarget(QStringLiteral("bar"), QStringLiteral("start"), QString()));
    QVERIFY(controller.cancelDrag());
    QVERIFY(!controller.dragActive());
    QVERIFY(controller.beginAppletDrag(QStringLiteral("tray"), QStringLiteral("clock-1")));
    controller.exitEditMode();
    QVERIFY(!controller.editMode());
    QVERIFY(!controller.dragActive());
    written = readWritten(directory.path(), QStringLiteral("live-fixture"));
    QCOMPARE(appletIds(written, QStringLiteral("tray")),
             (QStringList{QStringLiteral("tasks-1"), QStringLiteral("clock-1")}));
    controller.toggleEditMode();
    QVERIFY(controller.editMode());
}

void LiveCustomizationControllerTest::chordDefaultsAndModifiers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LiveCustomizationController controller(m_catalog.manifests(), directory.path(),
                                           fixtureOutputs, nullptr, nullptr);
    QCOMPARE(controller.chord(), QStringLiteral("meta-right"));
    QCOMPARE(controller.chordModifiers(), static_cast<int>(Qt::MetaModifier));
    QCOMPARE(LiveCustomizationController::modifiersForChord(QStringLiteral("meta-alt-right")),
             static_cast<int>(Qt::MetaModifier | Qt::AltModifier));
    QCOMPARE(LiveCustomizationController::modifiersForChord(QStringLiteral("garbage")),
             static_cast<int>(Qt::MetaModifier));
    QCOMPARE(LiveCustomizationController::chordSettingsKey(), QStringLiteral("shell.customization.chord"));
    QVERIFY(!controller.customizeRouteAvailable());
    QVERIFY(!controller.openCustomize());
    QVERIFY(!controller.openWallpaperSettings());
}

void LiveCustomizationControllerTest::modelArithmetic()
{
    const auto profile = fixtureProfile();
    const auto *bar = Model::findPanel(profile, QStringLiteral("bar"));
    // Clock to end: after status, appended.
    auto move = Model::zoneMove(*bar, QStringLiteral("clock-1"), QStringLiteral("end"));
    QVERIFY(move.has_value());
    QVERIFY(!move->target.beforeAppletId.has_value());
    QVERIFY(!move->flatOrderUnchanged);
    // Status to start: after the clock, i.e. before nothing (append) but the
    // flat order [launcher, clock, status] is already that: zone-only.
    move = Model::zoneMove(*bar, QStringLiteral("status-1"), QStringLiteral("start"));
    QVERIFY(move.has_value());
    QVERIFY(move->flatOrderUnchanged);
    // Launcher to end: after status, appended -> a real move.
    move = Model::zoneMove(*bar, QStringLiteral("launcher-1"), QStringLiteral("end"));
    QVERIFY(move.has_value() && !move->flatOrderUnchanged);
    QVERIFY(!Model::zoneMove(*bar, QStringLiteral("nope"), QStringLiteral("end")).has_value());
    QVERIFY(!Model::zoneMove(*bar, QStringLiteral("clock-1"), QStringLiteral("desktop")).has_value());

    // Steps: clock left -> before launcher; launcher right -> before status.
    auto step = Model::stepMove(*bar, QStringLiteral("clock-1"), -1);
    QVERIFY(step.has_value() && step->beforeAppletId == QStringLiteral("launcher-1"));
    step = Model::stepMove(*bar, QStringLiteral("launcher-1"), 1);
    QVERIFY(step.has_value() && step->beforeAppletId == QStringLiteral("status-1"));
    QVERIFY(!Model::stepMove(*bar, QStringLiteral("launcher-1"), -1).has_value());
    QVERIFY(!Model::stepMove(*bar, QStringLiteral("status-1"), 1).has_value());

    QCOMPARE(Model::nextInstanceId(profile, QStringLiteral("clock")), QStringLiteral("clock-instance-1"));
    QCOMPARE(Model::nextPanelId(profile), QStringLiteral("panel-1"));
    QCOMPARE(Model::zoneAtFraction(0.1), QStringLiteral("start"));
    QCOMPARE(Model::zoneAtFraction(0.5), QStringLiteral("center"));
    QCOMPARE(Model::zoneAtFraction(0.9), QStringLiteral("end"));
    const auto panel = Model::defaultPanel(QStringLiteral("p"), Profiles::Edge::Right);
    QCOMPARE(panel.thickness, 32);
    QVERIFY(panel.alignment == Profiles::Alignment::Fill);
}

QTEST_GUILESS_MAIN(LiveCustomizationControllerTest)
#include "tst_livecustomizationcontroller.moc"
