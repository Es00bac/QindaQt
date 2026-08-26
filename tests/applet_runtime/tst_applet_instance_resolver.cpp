// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"

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
    void rejectsMissingManifestsAndUnsupportedPlacements();
    void requiresTheCompiledImplementationRegistry();
    void exposesCapabilitiesOnlyForRegisteredImplementations();
    void carriesDeniedCapabilitiesWithoutInventingAuthority();
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

    QCOMPARE(fixture.registry.entryPoints(),
             QStringList{QStringLiteral("qindaqt.applets.clock")});
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
        instance(QStringLiteral("clock")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, empty);

    QCOMPARE(AppletRuntime::toString(rejected.status),
             QStringLiteral("implementation-unavailable"));
}

void AppletInstanceResolverTests::exposesCapabilitiesOnlyForRegisteredImplementations()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    const auto unavailable = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("launcher")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(unavailable.status),
             QStringLiteral("implementation-unavailable"));
    QVERIFY(unavailable.grantedCapabilities.isEmpty());

    const AppletRuntime::BuiltinAppletRegistry registry(
        QStringList{QStringLiteral("qindaqt.applets.launcher")});
    const auto registered = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("launcher")), Profiles::Edge::Bottom,
        fixture.catalog, fixture.policy, registry);
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

QTEST_GUILESS_MAIN(AppletInstanceResolverTests)
#include "tst_applet_instance_resolver.moc"
