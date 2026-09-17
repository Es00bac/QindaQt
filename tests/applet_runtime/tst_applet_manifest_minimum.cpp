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

// A separate failure model from tst_applet_instance_resolver.cpp: that suite
// proves which applets resolve and with what authority, this one proves the
// one number the shell's panel layout reads back out of a manifest
// (ADR-0188). It also keeps that suite under its decomposition limit.
class AppletManifestMinimumTests final : public QObject {
    Q_OBJECT

private slots:
    void republishesTheDeclaredMainAxisMinimum();
};

// ADR-0188: the panel's zone budget reserves the sum of a zone's applet
// minimums before giving a greedy zone the rest, so this republication is the
// only reason `sizing.mainAxis.minimum` is more than documentation. Without
// this row the shell could silently receive zero for every applet and the
// clock, tray and workspace switcher would be squeezed to nothing by a wide
// global menu.
void AppletManifestMinimumTests::republishesTheDeclaredMainAxisMinimum()
{
    Fixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));

    // Exactly the declared values of the stock top panel's three zones.
    const struct {
        const char *plugin;
        const char *zone;
        int declared;
    } expected[] = {
        {"global-menu", "start", 160},   {"system-menu", "start", 28},
        {"workspace-switcher", "center", 48}, {"clock", "end", 56},
        {"status-notifier", "end", 32},  {"power", "end", 40},
    };
    for (const auto &expectation : expected) {
        const QString plugin = QString::fromLatin1(expectation.plugin);
        const auto *manifest = fixture.catalog.findById(plugin);
        QVERIFY2(manifest != nullptr, qPrintable(plugin));
        // Pinned to the manifest as well as to a literal, so a manifest edit
        // cannot quietly move the panel layout.
        QCOMPARE(manifest->sizing.mainAxis.minimum, expectation.declared);

        auto spec = instance(plugin);
        spec.settings[QStringLiteral("zone")] =
            QString::fromLatin1(expectation.zone);
        const auto resolved = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
            spec, Profiles::Edge::Top, fixture.catalog, fixture.policy,
            fixture.registry);
        QCOMPARE(AppletRuntime::toString(resolved.status), QStringLiteral("ready"));
        QCOMPARE(resolved.mainAxisMinimum, expectation.declared);
        // The shell reads it off the runtime map, so that is what must carry it.
        const QVariantMap runtime =
            resolved.toVariantMap().value(QStringLiteral("runtime")).toMap();
        QVERIFY(runtime.contains(QStringLiteral("mainAxisMinimum")));
        QCOMPARE(runtime.value(QStringLiteral("mainAxisMinimum")).toInt(),
                 expectation.declared);
    }

    // A failed resolution reserves nothing rather than inventing a minimum.
    const auto missing = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance(QStringLiteral("weather-forecast")), Profiles::Edge::Top,
        fixture.catalog, fixture.policy, fixture.registry);
    QCOMPARE(AppletRuntime::toString(missing.status),
             QStringLiteral("missing-manifest"));
    QCOMPARE(missing.mainAxisMinimum, 0);
    QCOMPARE(missing.toVariantMap()
                 .value(QStringLiteral("runtime"))
                 .toMap()
                 .value(QStringLiteral("mainAxisMinimum"))
                 .toInt(),
             0);

    auto rejected = instance(QStringLiteral("clock"));
    rejected.settings[QStringLiteral("zone")] = QStringLiteral("diagonal");
    const auto placementRejected =
        AppletRuntime::AppletInstanceResolver::resolveBuiltin(
            rejected, Profiles::Edge::Top, fixture.catalog, fixture.policy,
            fixture.registry);
    QCOMPARE(AppletRuntime::toString(placementRejected.status),
             QStringLiteral("placement-rejected"));
    QCOMPARE(placementRejected.mainAxisMinimum, 0);
}

QTEST_GUILESS_MAIN(AppletManifestMinimumTests)
#include "tst_applet_manifest_minimum.moc"
