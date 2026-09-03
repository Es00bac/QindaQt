// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-package probe: resolves the staged launcher manifest through the
// audited registry/policy path and loads the staged compiled QML module. The
// run_installed_launcher.cmake driver passes only staged paths plus a
// poisoned environment; the probe fails closed on any mismatch.

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applet_runtime/applet_instance_resolver.h>
#include <qindaqt/applet_runtime/builtin_applet_registry.h>
#include <qindaqt/applets/manifest_catalog.h>

#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>

using namespace QindaQt;

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    const QCommandLineOption appletsDir(QStringLiteral("applets-dir"),
                                        QStringLiteral("staged manifest dir"),
                                        QStringLiteral("path"));
    const QCommandLineOption policyFile(QStringLiteral("policy"),
                                        QStringLiteral("staged policy file"),
                                        QStringLiteral("path"));
    const QCommandLineOption stagedQml(QStringLiteral("staged-qml"),
                                       QStringLiteral("staged QML import root"),
                                       QStringLiteral("path"));
    parser.addOptions({ appletsDir, policyFile, stagedQml });
    parser.process(app);

    Applets::ManifestCatalog catalog;
    QString error;
    if (!catalog.loadDirectory(parser.value(appletsDir), &error)) {
        qWarning() << "staged catalog failed:" << error;
        return 1;
    }
    const auto loadedPolicy = AppletHost::CapabilityPolicyLoader::fromFile(
        parser.value(policyFile));
    if (!loadedPolicy.ok) {
        qWarning() << "staged policy failed:" << loadedPolicy.error;
        return 1;
    }

    Profiles::AppletSpec instance {
        .id = QStringLiteral("probe"),
        .plugin = QStringLiteral("launcher"),
        .settings = {{ QStringLiteral("zone"), QStringLiteral("start") }},
    };
    const auto resolved = AppletRuntime::AppletInstanceResolver::resolveBuiltin(
        instance, Profiles::Edge::Top, catalog, loadedPolicy.policy,
        AppletRuntime::BuiltinAppletRegistry::firstParty());
    if (!resolved.ready()
        || resolved.grantedCapabilities
               != QStringList { QStringLiteral("applications.launch") }) {
        qWarning() << "staged launcher did not resolve ready with its grant:"
                   << resolved.diagnostic << resolved.grantedCapabilities;
        return 1;
    }

    QQmlEngine engine;
    // The moved staged root is the only injected import root. Tokens and
    // Controls are siblings inside that prefix; no build-tree fallback can
    // conceal an absolute-path regression.
    engine.addImportPath(parser.value(stagedQml));
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.Launcher"),
                             QStringLiteral("LauncherApplet"));
    if (!component.isReady()) {
        qWarning() << "staged QML module failed:" << component.errorString();
        return 1;
    }
    std::unique_ptr<QObject> applet(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue<QObject *>(nullptr)}}));
    if (!applet) {
        qWarning() << "staged applet did not instantiate";
        return 1;
    }

    qInfo() << "installed launcher probe passed";
    return 0;
}
