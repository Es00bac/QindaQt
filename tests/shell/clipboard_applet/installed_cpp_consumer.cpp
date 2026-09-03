// SPDX-License-Identifier: GPL-3.0-or-later

// Installed-boundary consumer probe. Builds against only the staged public
// archives and headers of the ClipboardApplet component — no build-tree
// applet paths — and exercises two things at that boundary:
//   1. the exact lock/privacy purge contract of the composed controller and
//      model adapter, and
//   2. the staged compiled QML module: the static plugin registers
//      QindaQt.Shell.ClipboardApplet, the QST-1 theme publishes through the
//      staged Tokens/Controls modules, and the packaged ClipboardApplet
//      surface instantiates offscreen against the real controller.
//
// Stage locations arrive as STAGE-RELATIVE compile definitions from the
// harness script; the probe resolves them against its own executable location
// (<stage>/consumer-build) at runtime, so the whole stage can be relocated
// and rerun without LD_LIBRARY_PATH. The probe itself contains no build-tree
// or source-tree path.

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtGlobal>

#include <memory>

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_applet_controller.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>
#include <qindaqt/themes/theme_loader.h>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_ClipboardAppletPlugin)

using namespace QindaQt::ShellClipboardApplet;
using namespace QindaQt::Services::ClipboardModel;

namespace {

QString stagedPath(const char *relative)
{
    static const QString stageRoot = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/.."));
    return stageRoot + QLatin1Char('/') + QString::fromUtf8(relative);
}

bool publishStagedTheme(QQmlEngine &engine)
{
    // Mirror of the Controls test-support publication path, kept local so the
    // consumer exercises only the staged public modules.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    if (registration.isError()) {
        qCritical("installed consumer: QindaQt.Tokens did not resolve: %s",
                  qPrintable(registration.errorString()));
        return false;
    }

    auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (!facade) {
        qCritical("installed consumer: Tokens singleton not registered");
        return false;
    }

    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        stagedPath(QINDAQT_STAGED_THEME_RELATIVE));
    if (!loaded.ok) {
        qCritical("installed consumer: staged theme refused: %s", qPrintable(loaded.error));
        return false;
    }
    QString error;
    if (!facade->publish(loaded.theme, {}, &error)) {
        qCritical("installed consumer: theme publish failed: %s", qPrintable(error));
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    ClipboardHistoryModel model;
    model.setHistoryEnabled(true);
    model.setPrivacyAllowed(true);

    ClipboardValue value;
    value.formats = { { QStringLiteral("text/plain"), "installed-consumer-payload" } };
    const auto admitted = model.admit(
        value, model.generation(), QStringLiteral("InstalledConsumer"), 1);
    if (!admitted.accepted()) {
        qCritical("installed consumer: admission refused");
        return 1;
    }

    ClipboardModelClientAdapter adapter(&model);
    ClipboardAppletController controller(&adapter, true, true);

    if (controller.phaseText() != QStringLiteral("ready") || controller.entryCount() != 1) {
        qCritical("installed consumer: expected ready phase with one entry");
        return 2;
    }

    const quint32 generationBeforeLock = model.generation();

    // AGENT-GUARD: the authenticated lock must deny model privacy, purge the
    // entry, and advance the generation before the lock becomes observable.
    adapter.setLocked(true);
    if (controller.phaseText() != QStringLiteral("locked") || controller.entryCount() != 0) {
        qCritical("installed consumer: lock did not withhold presentation");
        return 3;
    }
    if (model.generation() != generationBeforeLock + 1) {
        qCritical("installed consumer: lock did not fence the generation");
        return 4;
    }
    if (!model.snapshot().entries.isEmpty()) {
        qCritical("installed consumer: lock did not purge model content");
        return 5;
    }

    // Unlock must not redisclose the pre-lock entry.
    adapter.setLocked(false);
    if (controller.phaseText() != QStringLiteral("ready") || controller.entryCount() != 0) {
        qCritical("installed consumer: unlock redisclosed pre-lock content");
        return 6;
    }

    // Staged compiled QML module proof: publish the staged theme through the
    // staged Tokens/Controls modules, then instantiate the packaged
    // ClipboardApplet surface from the staged QML file against the real
    // controller.
    QQmlEngine engine;
    engine.addImportPath(stagedPath(QINDAQT_STAGED_QML_RELATIVE));
    if (!publishStagedTheme(engine)) {
        return 7;
    }

    QQmlComponent surface(
        &engine,
        QUrl::fromLocalFile(stagedPath(QINDAQT_STAGED_SURFACE_RELATIVE)));
    if (surface.isError()) {
        qCritical("installed consumer: staged ClipboardApplet.qml did not load: %s",
                  qPrintable(surface.errorString()));
        return 8;
    }
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("controller"),
                             QVariant::fromValue<QObject *>(&controller));
    std::unique_ptr<QObject> surfaceObject(surface.createWithInitialProperties(initialProperties));
    if (!surfaceObject) {
        qCritical("installed consumer: staged surface did not instantiate: %s",
                  qPrintable(surface.errorString()));
        return 9;
    }
    if (surfaceObject->property("objectName").toString() != QStringLiteral("clipboardApplet")) {
        qCritical("installed consumer: staged surface is not the clipboard applet");
        return 10;
    }
    const QVariant phase = surfaceObject->property("controller")
        .value<QObject *>()->property("phaseText");
    if (phase.toString() != QStringLiteral("ready")) {
        qCritical("installed consumer: staged surface lost the controller boundary");
        return 11;
    }

    qInfo("installed consumer: ClipboardApplet component boundary and staged module verified");
    return 0;
}
