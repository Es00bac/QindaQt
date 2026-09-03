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
    void rejectsMissingManifestsAndUnsupportedPlacements();
    void requiresTheCompiledImplementationRegistry();
    void exposesCapabilitiesOnlyForRegisteredImplementations();
    void carriesDeniedCapabilitiesWithoutInventingAuthority();
    void stockProfilesPlaceOneResolvedLauncher();
    void globalMenuUsesLeastAuthorityAndStockTopPanels();
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
        QStringLiteral("qindaqt.applets.audio"),
        QStringLiteral("qindaqt.applets.bluetooth"),
        QStringLiteral("qindaqt.applets.clipboard"),
        QStringLiteral("qindaqt.applets.clock"),
        QStringLiteral("qindaqt.applets.global-menu"),
        QStringLiteral("qindaqt.applets.launcher"),
        QStringLiteral("qindaqt.applets.notification-center"),
        QStringLiteral("qindaqt.applets.power")};
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
        instance(QStringLiteral("workspace-switcher")), Profiles::Edge::Top,
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

void AppletInstanceResolverTests::stockProfilesPlaceOneResolvedLauncher()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    Profiles::ProfileCatalog profiles;
    QVERIFY2(profiles.loadDirectory(
                 QStringLiteral(QINDAQT_SOURCE_DIR "/data/profiles"), &error),
             qPrintable(error));
    QCOMPARE(profiles.profiles().size(), 10);

    for (const auto &profile : profiles.profiles()) {
        int launcherCount = 0;
        for (const auto &panel : profile.panels) {
            for (const auto &applet : panel.applets) {
                if (applet.plugin != QLatin1String("launcher")) {
                    continue;
                }
                ++launcherCount;
                const auto resolved =
                    AppletRuntime::AppletInstanceResolver::resolveBuiltin(
                        applet, panel.edge, fixture.catalog, fixture.policy,
                        fixture.registry);
                QVERIFY2(resolved.ready(),
                         qPrintable(profile.id + QStringLiteral(": ")
                                    + resolved.diagnostic));
                QCOMPARE(resolved.entryPoint,
                         QStringLiteral("qindaqt.applets.launcher"));
                QCOMPARE(resolved.grantedCapabilities,
                         QStringList{QStringLiteral("applications.launch")});
            }
        }
        QCOMPARE(launcherCount, 1);
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

QTEST_GUILESS_MAIN(AppletInstanceResolverTests)
#include "tst_applet_instance_resolver.moc"
