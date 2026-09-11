// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/profile_catalog.h"

#include <QtTest>

using namespace QindaQt;

namespace {

struct Fixture {
    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;
    AppletRuntime::BuiltinAppletRegistry registry =
        AppletRuntime::BuiltinAppletRegistry::firstParty();

    bool load(QString *error)
    {
        if (!catalog.loadDirectory(
                QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
            return false;
        }
        const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/applet-policy/default.json"));
        if (!loaded.ok) {
            *error = loaded.error;
            return false;
        }
        policy = loaded.policy;
        return true;
    }
};

Profiles::AppletSpec instance(QString plugin)
{
    return {.id = QStringLiteral("instance"),
            .plugin = std::move(plugin),
            .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}};
}

} // namespace

class AppletInstanceResolverTests final : public QObject {
    Q_OBJECT

private slots:
    void resolvesAuditedBuiltinsAndCapabilities();
    void resolvesNotificationCenterForEveryPanelPlacement();
    void resolvesLegacySystemTrayThroughCompiledRenderer();
    void rejectsMissingManifestsAndUnsupportedPlacements();
    void requiresTheCompiledImplementationRegistry();
    void exposesCapabilitiesOnlyForRegisteredImplementations();
    void carriesDeniedCapabilitiesWithoutInventingAuthority();
    void stockProfilesPlaceOneResolvedLauncher();
    void stockProfilesPlaceOneResolvedClipboardInUtilitySlot();
    void stockProfilesPlaceHostedTaskListWhereWorkflowExposesTasks();
    void stockProfilesPlaceOneResolvedStatusNotifier();
    void resolvesDesktopZoneWithoutAPanelEdge();
    void globalMenuUsesLeastAuthorityAndStockTopPanels();
    void desktopControlsResolveReadyInEveryStockPlacement();
};

void AppletInstanceResolverTests::resolvesAuditedBuiltinsAndCapabilities()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    const auto clock = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("clock")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(clock.ready(), qPrintable(clock.diagnostic));
    QCOMPARE(clock.entryPoint, QStringLiteral("qindaqt.applets.clock"));
    QVERIFY(clock.grantedCapabilities.isEmpty());
    QCOMPARE(clock.toVariantMap().value(QStringLiteral("runtime"))
                 .toMap().value(QStringLiteral("status")).toString(),
             QStringLiteral("ready"));

    const QStringList expectedEntryPoints{
        QStringLiteral("qindaqt.applets.active-application"),
        QStringLiteral("qindaqt.applets.application-tiles"),
        QStringLiteral("qindaqt.applets.audio"),
        QStringLiteral("qindaqt.applets.bluetooth"),
        QStringLiteral("qindaqt.applets.clipboard"),
        QStringLiteral("qindaqt.applets.clock"),
        QStringLiteral("qindaqt.applets.command-hud"),
        QStringLiteral("qindaqt.applets.command-palette"),
        QStringLiteral("qindaqt.applets.dashboard"),
        // Worn Luna desktop experience (ADR-0124/ADR-0125); sorted position.
        QStringLiteral("qindaqt.applets.desktop-icons"),
        QStringLiteral("qindaqt.applets.global-menu"),
        QStringLiteral("qindaqt.applets.launcher"),
        QStringLiteral("qindaqt.applets.notification-center"),
        QStringLiteral("qindaqt.applets.overview-trigger"),
        QStringLiteral("qindaqt.applets.places-menu"),
        QStringLiteral("qindaqt.applets.power"),
        QStringLiteral("qindaqt.applets.quick-launch"),
        QStringLiteral("qindaqt.applets.show-desktop"),
        QStringLiteral("qindaqt.applets.start-menu"),
        QStringLiteral("qindaqt.applets.status-notifier"),
        QStringLiteral("qindaqt.applets.system-menu"),
        QStringLiteral("qindaqt.applets.system-status"),
        QStringLiteral("qindaqt.applets.task-list"),
        QStringLiteral("qindaqt.applets.workspace-switcher"),
        QStringLiteral("qindaqt.applets.workspace-tiles")};
    QCOMPARE(fixture.registry.entryPoints(), expectedEntryPoints);

    const auto audio = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("audio")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(audio.ready(), qPrintable(audio.diagnostic));
    QCOMPARE(audio.entryPoint, QStringLiteral("qindaqt.applets.audio"));
    QCOMPARE(audio.grantedCapabilities,
             QStringList({QStringLiteral("audio.control"),
                          QStringLiteral("audio.read")}));

    const auto power = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("power")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(power.ready(), qPrintable(power.diagnostic));
    QCOMPARE(power.entryPoint, QStringLiteral("qindaqt.applets.power"));
    QCOMPARE(power.grantedCapabilities,
             QStringList({QStringLiteral("power.control"),
                          QStringLiteral("power.read")}));

    const auto bluetooth = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("bluetooth")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(bluetooth.ready(), qPrintable(bluetooth.diagnostic));
    QCOMPARE(bluetooth.entryPoint, QStringLiteral("qindaqt.applets.bluetooth"));
    QCOMPARE(bluetooth.grantedCapabilities,
             QStringList({QStringLiteral("bluetooth.control"),
                          QStringLiteral("bluetooth.read")}));

    const auto clipboard = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("clipboard")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(clipboard.ready(), qPrintable(clipboard.diagnostic));
    QCOMPARE(clipboard.entryPoint, QStringLiteral("qindaqt.applets.clipboard"));
    QCOMPARE(clipboard.grantedCapabilities,
             QStringList({QStringLiteral("clipboard.read"),
                          QStringLiteral("clipboard.write")}));

    const auto taskList = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("task-list")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(taskList.ready(), qPrintable(taskList.diagnostic));
    QCOMPARE(taskList.entryPoint, QStringLiteral("qindaqt.applets.task-list"));
    QCOMPARE(taskList.grantedCapabilities,
             QStringList({QStringLiteral("windows.activate"),
                          QStringLiteral("windows.manage"),
                          QStringLiteral("windows.read")}));
    const auto statusNotifier = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("status-notifier")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(statusNotifier.ready(), qPrintable(statusNotifier.diagnostic));
    QCOMPARE(statusNotifier.entryPoint, QStringLiteral("qindaqt.applets.status-notifier"));
    QCOMPARE(statusNotifier.grantedCapabilities,
             QStringList({QStringLiteral("status-items.activate"),
                          QStringLiteral("status-items.read")}));
}

void AppletInstanceResolverTests::resolvesLegacySystemTrayThroughCompiledRenderer()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    const auto legacy = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("system-tray")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, fixture.registry);
    const auto current = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("status-notifier")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(legacy.ready(), qPrintable(legacy.diagnostic));
    QVERIFY(current.ready());
    QCOMPARE(legacy.entryPoint, current.entryPoint);
    QCOMPARE(legacy.grantedCapabilities, current.grantedCapabilities);
}

void AppletInstanceResolverTests::resolvesNotificationCenterForEveryPanelPlacement()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    const QList<Profiles::Edge> edges{Profiles::Edge::Top,
                                      Profiles::Edge::Bottom,
                                      Profiles::Edge::Left,
                                      Profiles::Edge::Right};
    const QStringList zones{QStringLiteral("start"),
                            QStringLiteral("center"),
                            QStringLiteral("end"),
                            QStringLiteral("fill")};
    for (const Profiles::Edge edge : edges) {
        for (const QString &zone : zones) {
            auto notificationCenter = instance(QStringLiteral("notification-center"));
            notificationCenter.settings[QStringLiteral("zone")] = zone;
            const auto resolved = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                notificationCenter, edge, fixture.catalog, fixture.policy,
                fixture.registry);
            QVERIFY2(resolved.ready(), qPrintable(resolved.diagnostic));
            QCOMPARE(resolved.entryPoint,
                     QStringLiteral("qindaqt.applets.notification-center"));
            QVERIFY(resolved.grantedCapabilities.isEmpty());
        }
    }
}

void AppletInstanceResolverTests::rejectsMissingManifestsAndUnsupportedPlacements()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    const auto missing = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("weather-forecast")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(missing.status),
             QStringLiteral("missing-manifest"));
    QVERIFY(!missing.diagnostic.isEmpty());

    const auto verticalMenu = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("global-menu")), Profiles::Edge::Left,
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(verticalMenu.status),
             QStringLiteral("placement-rejected"));
    QCOMPARE(verticalMenu.entryPoint,
             QStringLiteral("qindaqt.applets.global-menu"));

    auto badZone = instance(QStringLiteral("clock"));
    badZone.settings[QStringLiteral("zone")] = QStringLiteral("diagonal");
    const auto rejectedZone = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        badZone, Profiles::Edge::Top, fixture.catalog, fixture.policy,
        fixture.registry);
    QCOMPARE(AppletRuntime::toString(rejectedZone.status),
             QStringLiteral("placement-rejected"));

    const auto invalidEdge = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("clock")), static_cast<Profiles::Edge>(99),
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(invalidEdge.status),
             QStringLiteral("placement-rejected"));
}

void AppletInstanceResolverTests::requiresTheCompiledImplementationRegistry()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    const AppletRuntime::BuiltinAppletRegistry empty(QStringList{});

    const auto rejected = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("notification-center")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, empty);

    QCOMPARE(AppletRuntime::toString(rejected.status),
             QStringLiteral("implementation-unavailable"));
}

void AppletInstanceResolverTests::exposesCapabilitiesOnlyForRegisteredImplementations()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    // The first-party registry now contains the launcher (L1); an
    // implementation missing from the evaluated registry still fails closed.
    const AppletRuntime::BuiltinAppletRegistry withoutLauncher(
        QStringList{QStringLiteral("qindaqt.applets.clock")});
    const auto unavailable = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("launcher")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, withoutLauncher);
    QCOMPARE(AppletRuntime::toString(unavailable.status),
             QStringLiteral("implementation-unavailable"));
    QVERIFY(unavailable.grantedCapabilities.isEmpty());

    const auto registered = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("launcher")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(registered.ready(), qPrintable(registered.diagnostic));
    QCOMPARE(registered.grantedCapabilities,
             QStringList{QStringLiteral("applications.launch")});
}

void AppletInstanceResolverTests::carriesDeniedCapabilitiesWithoutInventingAuthority()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    fixture.policy.auditedBuiltinDefault =
        AppletHost::CapabilityDisposition::Deny;
    const AppletRuntime::BuiltinAppletRegistry registry(
        QStringList{QStringLiteral("qindaqt.applets.launcher")});

    const auto launcher = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("launcher")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, registry);

    QVERIFY2(launcher.ready(), qPrintable(launcher.diagnostic));
    QVERIFY(launcher.grantedCapabilities.isEmpty());
}

void AppletInstanceResolverTests::resolvesDesktopZoneWithoutAPanelEdge()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    // The desktop-icons instance anchors to the desktop surface (ADR-0125):
    // it resolves without a panel edge through the dedicated desktop gate.
    auto desktopInstance = instance(QStringLiteral("desktop-icons"));
    desktopInstance.settings[QStringLiteral("zone")] = QStringLiteral("desktop");
    const auto resolved = AppletRuntime::AppletInstanceResolver::resolveDesktopBuiltin(
        desktopInstance, fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(resolved.ready(), qPrintable(resolved.diagnostic));
    QCOMPARE(resolved.entryPoint, QStringLiteral("qindaqt.applets.desktop-icons"));
    QCOMPARE(resolved.grantedCapabilities,
             QStringList{QStringLiteral("applications.launch")});

    // Fail closed: a panel applet has no desktop zone in its manifest, so the
    // desktop resolver rejects it instead of hosting it on the surface.
    const auto rejected = AppletRuntime::AppletInstanceResolver::resolveDesktopBuiltin(
        instance(QStringLiteral("clock")), fixture.catalog, fixture.policy,
        fixture.registry);
    QCOMPARE(AppletRuntime::toString(rejected.status),
             QStringLiteral("placement-rejected"));
}

void AppletInstanceResolverTests::stockProfilesPlaceOneResolvedLauncher()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));
    QCOMPARE(profiles.profiles().size(), 11);

    for (const auto &profile : profiles.profiles()) {
        // AGENT-NOTE: the menu slot is the launcher by default, but the Bliss
        // profile opts its menu instance into the start-menu presentation
        // (ADR-0124). Exactly one resolved menu applet per stock profile.
        int menuAppletCount = 0;
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                const bool isMenuApplet =
                    applet.plugin == QLatin1String("launcher")
                    || applet.plugin == QLatin1String("start-menu");
                if (!isMenuApplet) {
                    continue;
                }
                ++menuAppletCount;
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(),
                         qPrintable(profile.id + QStringLiteral(": ")
                                    + resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.")
                             + (applet.plugin == QLatin1String("launcher")
                                    ? QStringLiteral("launcher")
                                    : QStringLiteral("start-menu")));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList{QStringLiteral("applications.launch")});
            }
        }
        QCOMPARE(menuAppletCount, 1);
    }
}

void AppletInstanceResolverTests::stockProfilesPlaceOneResolvedClipboardInUtilitySlot()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));

    for (const auto &profile : profiles.profiles()) {
        int clipboardCount = 0;
        for (const auto &panel : profile.panels) {
            QString notificationZone;
            QString clipboardZone;
            for (const auto &applet : panel.applets) {
                const QString zone = applet.settings
                    .value(QStringLiteral("zone"), QStringLiteral("start"))
                    .toString();
                if (applet.plugin == QLatin1String("notification-center")) {
                    notificationZone = zone;
                }
                if (applet.plugin != QLatin1String("clipboard")) {
                    continue;
                }
                ++clipboardCount;
                clipboardZone = zone;
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(), qPrintable(profile.id + QStringLiteral(": ")
                                                       + resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.clipboard"));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList({QStringLiteral("clipboard.read"),
                                      QStringLiteral("clipboard.write")}));
            }
            if (!clipboardZone.isEmpty()) {
                QVERIFY2(!notificationZone.isEmpty(), qPrintable(profile.id));
                QCOMPARE(clipboardZone, notificationZone);
            }
        }
        // AGENT-NOTE: qinda-bliss deliberately ships without a clipboard tray
        // slot — the XP taskbar it reproduces has no utility chip there
        // (ADR-0124). Every other stock profile keeps exactly one.
        const int expectedClipboardCount =
            profile.id == QLatin1String("qinda-bliss") ? 0 : 1;
        QCOMPARE(clipboardCount, expectedClipboardCount);
    }
}

void AppletInstanceResolverTests::stockProfilesPlaceHostedTaskListWhereWorkflowExposesTasks()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));

    for (const auto &profile : profiles.profiles()) {
        int taskListCount = 0;
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                if (applet.plugin != QLatin1String("task-list")) {
                    continue;
                }
                ++taskListCount;
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(), qPrintable(profile.id
                                                       + QStringLiteral(": ")
                                                       + resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.task-list"));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList({QStringLiteral("windows.activate"),
                                      QStringLiteral("windows.manage"),
                                      QStringLiteral("windows.read")}));
            }
        }
        const bool panelTasksExpected = profile.workflow.taskList
                != QLatin1String("hidden")
            && profile.workflow.taskList != QLatin1String("overview-only");
        QCOMPARE(taskListCount, panelTasksExpected ? 1 : 0);
    }
}

void AppletInstanceResolverTests::stockProfilesPlaceOneResolvedStatusNotifier()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));

    for (const auto &profile : profiles.profiles()) {
        int statusNotifierCount = 0;
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                if (applet.plugin != QLatin1String("status-notifier")) {
                    continue;
                }
                ++statusNotifierCount;
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(), qPrintable(profile.id + QStringLiteral(": ")
                                                       + resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.status-notifier"));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList({QStringLiteral("status-items.activate"),
                                      QStringLiteral("status-items.read")}));
            }
        }
        QCOMPARE(statusNotifierCount, 1);
    }
}

void AppletInstanceResolverTests::globalMenuUsesLeastAuthorityAndStockTopPanels()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));

    int enabledFamilies = 0;
    for (const auto &profile : profiles.profiles()) {
        int menuCount = 0;
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                if (applet.plugin != QLatin1String("global-menu")) {
                    continue;
                }
                ++menuCount;
                QCOMPARE(static_cast<int>(panel.edge),
                         static_cast<int>(Profiles::Edge::Top));
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(), qPrintable(resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.global-menu"));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList{QStringLiteral("global-menu.read")});
            }
        }
        QCOMPARE(menuCount, profile.workflow.globalMenu ? 1 : 0);
        if (profile.workflow.globalMenu) {
            ++enabledFamilies;
        }
    }
    QCOMPARE(enabledFamilies, 3);
}

void AppletInstanceResolverTests::desktopControlsResolveReadyInEveryStockPlacement()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));

    // AGENT-CONTRACT: these are the thirteen stock plugin ids the desktop
    // controls lane owns (docs/wiki/shell/desktop-controls.md). Every stock
    // instance of each must pass all five gates in its own panel placement.
    const QStringList desktopControls{
        QStringLiteral("active-application"), QStringLiteral("application-tiles"),
        QStringLiteral("command-hud"),
        QStringLiteral("command-palette"), QStringLiteral("dashboard"),
        QStringLiteral("overview-trigger"), QStringLiteral("places-menu"),
        QStringLiteral("quick-launch"), QStringLiteral("show-desktop"),
        QStringLiteral("system-menu"), QStringLiteral("system-status"),
        QStringLiteral("workspace-switcher"), QStringLiteral("workspace-tiles")};
    QSet<QString> seen;
    for (const auto &profile : profiles.profiles()) {
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                if (!desktopControls.contains(applet.plugin)) {
                    continue;
                }
                const auto resolved = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                    applet, panel.edge, fixture.catalog, fixture.policy, fixture.registry);
                QVERIFY2(resolved.ready(),
                         qPrintable(QStringLiteral("%1/%2/%3: %4")
                                        .arg(profile.id, panel.id, applet.plugin,
                                             resolved.diagnostic)));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.") + applet.plugin);
                seen.insert(applet.plugin);
            }
        }
    }
    for (const QString &plugin : desktopControls) {
        QVERIFY2(seen.contains(plugin),
                 qPrintable(QStringLiteral("no stock profile places %1").arg(plugin)));
    }

    // Least authority: the workspace controls never receive window activation
    // and the HUD receives only menu observation.
    const auto switcher = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("workspace-switcher")), Profiles::Edge::Left,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(switcher.ready(), qPrintable(switcher.diagnostic));
    QCOMPARE(switcher.grantedCapabilities,
             QStringList({QStringLiteral("windows.manage"), QStringLiteral("windows.read")}));
    const auto hud = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("command-hud")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QVERIFY2(hud.ready(), qPrintable(hud.diagnostic));
    QCOMPARE(hud.grantedCapabilities, QStringList{QStringLiteral("global-menu.read")});
    const auto activeApplication = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("active-application")), Profiles::Edge::Left,
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(activeApplication.status),
             QStringLiteral("placement-rejected"));
}

QTEST_GUILESS_MAIN(AppletInstanceResolverTests)
#include "tst_applet_instance_resolver.moc"
