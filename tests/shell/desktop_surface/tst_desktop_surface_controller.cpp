// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_runtime/applet_instance_resolver.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell/desktop_surface/desktop_surface_controller.h"

#include <QGuiApplication>
#include <QMargins>
#include <QQmlEngine>
#include <QRect>
#include <QScreen>
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
    void panelReservationsCutEachOutputsWorkArea();
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

namespace {

QRect rectOf(const QVariantMap &map)
{
    return QRect(map.value(QStringLiteral("x")).toInt(), map.value(QStringLiteral("y")).toInt(),
                 map.value(QStringLiteral("width")).toInt(),
                 map.value(QStringLiteral("height")).toInt());
}

QVariantMap entryNamed(const QVariantList &rects, const QString &name)
{
    for (const QVariant &value : rects) {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("name")).toString() == name) {
            return entry;
        }
    }
    return {};
}

} // namespace

// ADR-0261: what every surface receives as `outputRects`. The output itself
// is always the whole screen - the surface spans it, behind the panels - and
// `workArea` is that screen minus the depths the runtime published for it.
void DesktopSurfaceControllerTests::panelReservationsCutEachOutputsWorkArea()
{
    QQmlEngine engine;
    DesktopSurfaceController controller(*qGuiApp, engine, {}, this);
    const QScreen *screen = qGuiApp->primaryScreen();
    QVERIFY(screen != nullptr);
    const QString name = screen->name();
    const QRect geometry = screen->geometry();
    const auto workArea = [&controller, &name] {
        return rectOf(entryNamed(controller.outputRects(), name)
                          .value(QStringLiteral("workArea"))
                          .toMap());
    };

    // Before any plan is published the whole output is work area.
    QCOMPARE(rectOf(entryNamed(controller.outputRects(), name)), geometry);
    QCOMPARE(workArea(), geometry);

    controller.setOutputReservations({{name, QMargins(52, 26, 0, 0)}});
    QCOMPARE(controller.outputReservations().value(name), QMargins(52, 26, 0, 0));
    QCOMPARE(workArea(), geometry.adjusted(52, 26, 0, 0));
    QCOMPARE(rectOf(entryNamed(controller.outputRects(), name)), geometry);

    controller.setOutputReservations({{name, QMargins(0, 24, 0, 72)}});
    QCOMPARE(workArea(), geometry.adjusted(0, 24, 0, -72));

    // A negative depth reserves nothing; depths that would leave no room
    // cannot be a real panel layout and fail open to the whole output.
    controller.setOutputReservations({{name, QMargins(-8, 26, 0, 0)}});
    QCOMPARE(workArea(), geometry.adjusted(0, 26, 0, 0));
    controller.setOutputReservations({{name, QMargins(0, geometry.height(), 0, 0)}});
    QCOMPARE(workArea(), geometry);

    // Depths for an output that is not connected, or none at all (the
    // auto-hidden panel was the only one), leave this output whole.
    controller.setOutputReservations({{QStringLiteral("NOT-CONNECTED"), QMargins(0, 26, 0, 0)}});
    QCOMPARE(workArea(), geometry);
    controller.setOutputReservations({});
    QVERIFY(controller.outputReservations().isEmpty());
    QCOMPARE(workArea(), geometry);
}

QTEST_MAIN(DesktopSurfaceControllerTests)
#include "tst_desktop_surface_controller.moc"
