// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell/desktop_surface/desktop_surface_controller.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QtTest>

using QindaQt::Shell::DesktopSurface::DesktopSurfaceController;

namespace {

struct ResolvedCatalog {
    QindaQt::Applets::ManifestCatalog applets;
    QindaQt::AppletHost::CapabilityPolicy policy;

    static ResolvedCatalog load(QString *error)
    {
        ResolvedCatalog result;
        if (!result.applets.loadDirectory(
                QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
            return {};
        }
        const auto loaded = QindaQt::AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR
                           "/data/applet-policy/default.json"));
        if (!loaded.ok) {
            *error = loaded.error;
            return {};
        }
        result.policy = loaded.policy;
        return result;
    }
};

} // namespace

class DesktopSurfaceControllerTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void emptyProfileProducesZeroWindows();
    void desktopAppletResolvesAgainstTheRealCatalog();
    void unresolvableAppletKeepsTheInventoryFailClosed();
};

void DesktopSurfaceControllerTests::emptyProfileProducesZeroWindows()
{
    ResolvedCatalog catalog;
    QString error;
    catalog = ResolvedCatalog::load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QGuiApplication &app = *qGuiApp;
    QQmlEngine engine;
    DesktopSurfaceController controller(
        app, engine, {}, this);

    // AGENT-GUARD (strictly additive): a profile without a `desktop` section
    // must adopt an empty inventory and create zero windows, even after
    // start() on a session with screens.
    QindaQt::Profiles::LayoutProfile profile;
    QVERIFY(profile.desktopApplets.isEmpty());
    controller.adoptProfile(profile, catalog.applets, catalog.policy);
    QVERIFY(controller.inventory().isEmpty());
    QCOMPARE(controller.windowCount(), 0);
    controller.start();
    QCOMPARE(controller.windowCount(), 0);

    // Adoption back to an empty inventory tears previously kept state down;
    // with nothing live the reconcile stays a no-op.
    QindaQt::Profiles::AppletSpec spec;
    spec.id = QStringLiteral("desktop-icons");
    spec.plugin = QStringLiteral("desktop-icons");
    profile.desktopApplets.append(spec);
    controller.adoptProfile(profile, catalog.applets, catalog.policy);
    QCOMPARE(controller.inventory().size(), 1);
    profile.desktopApplets.clear();
    controller.adoptProfile(profile, catalog.applets, catalog.policy);
    QVERIFY(controller.inventory().isEmpty());
    QCOMPARE(controller.windowCount(), 0);
}

void DesktopSurfaceControllerTests::
    desktopAppletResolvesAgainstTheRealCatalog()
{
    ResolvedCatalog catalog;
    QString error;
    catalog = ResolvedCatalog::load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QQmlEngine engine;
    DesktopSurfaceController controller(*qGuiApp, engine,
                                        {}, this);
    QindaQt::Profiles::LayoutProfile profile;
    QindaQt::Profiles::AppletSpec spec;
    spec.id = QStringLiteral("desktop-icons");
    spec.plugin = QStringLiteral("desktop-icons");
    spec.settings.insert(QStringLiteral("placement"),
                         QStringLiteral("right"));
    profile.desktopApplets.append(spec);

    controller.adoptProfile(profile, catalog.applets, catalog.policy);
    QCOMPARE(controller.inventory().size(), 1);
    const QVariantMap resolved = controller.inventory().constFirst().toMap();
    QCOMPARE(resolved.value(QStringLiteral("id")).toString(),
             QStringLiteral("desktop-icons"));
    const QVariantMap runtime =
        resolved.value(QStringLiteral("runtime")).toMap();
    QCOMPARE(runtime.value(QStringLiteral("ready")).toBool(), true);
    QCOMPARE(runtime.value(QStringLiteral("entryPoint")).toString(),
             QStringLiteral("qindaqt.applets.desktop-icons"));
    // The resolved settings map rides the instance spec for the QML side.
    QCOMPARE(resolved.value(QStringLiteral("settings"))
                 .toMap()
                 .value(QStringLiteral("placement"))
                 .toString(),
             QStringLiteral("right"));
    // Without start() no window exists; a null launcher facade must not be
    // dereferenced by adoption.
    QCOMPARE(controller.windowCount(), 0);
}

void DesktopSurfaceControllerTests::
    unresolvableAppletKeepsTheInventoryFailClosed()
{
    ResolvedCatalog catalog;
    QString error;
    catalog = ResolvedCatalog::load(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QQmlEngine engine;
    DesktopSurfaceController controller(*qGuiApp, engine,
                                        {}, this);
    QindaQt::Profiles::LayoutProfile profile;
    QindaQt::Profiles::AppletSpec unknown;
    unknown.id = QStringLiteral("broken");
    unknown.plugin = QStringLiteral("no-such-applet");
    profile.desktopApplets.append(unknown);

    controller.adoptProfile(profile, catalog.applets, catalog.policy);
    // The entry stays visible in the inventory with a non-ready runtime so
    // diagnostics are possible, but the QML gates presentation on readiness.
    QCOMPARE(controller.inventory().size(), 1);
    const QVariantMap runtime = controller.inventory().constFirst()
                                    .toMap()
                                    .value(QStringLiteral("runtime"))
                                    .toMap();
    QCOMPARE(runtime.value(QStringLiteral("ready")).toBool(), false);
    QVERIFY(!runtime.value(QStringLiteral("diagnostic")).toString().isEmpty());
    QCOMPARE(controller.windowCount(), 0);
}

QTEST_MAIN(DesktopSurfaceControllerTests)
#include "tst_desktop_surface_controller.moc"
