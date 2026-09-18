// SPDX-License-Identifier: GPL-3.0-or-later
// Parity replay tool for the live customization nested rows: runs a JSON
// intent script through the Settings Customize route's own editor host
// (RepositoryCustomizeEditorHost) and writes the resulting user profile, so
// the row can compare the shell's persisted bytes with the route's bytes for
// the same intents. No GUI, no bus.
#include "qindaqt/apps/settings_customize/customize_editor_host.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <optional>

using namespace QindaQt;
using namespace QindaQt::ShellCustomizationEditor;

namespace {

DropTarget targetOf(const QJsonObject &step)
{
    DropTarget target;
    target.panelId = step.value(QStringLiteral("panel")).toString();
    target.zone = step.value(QStringLiteral("zone")).toString(QStringLiteral("start"));
    const QString before = step.value(QStringLiteral("before")).toString();
    if (!before.isEmpty()) {
        target.beforeAppletId = before;
    }
    return target;
}

std::optional<EditorOutcome> runStep(Apps::SettingsCustomize::RepositoryCustomizeEditorHost &host,
                                     const QJsonObject &step)
{
    const QString kind = step.value(QStringLiteral("kind")).toString();
    const QString panel = step.value(QStringLiteral("panel")).toString();
    const QString applet = step.value(QStringLiteral("applet")).toString();
    if (kind == QLatin1String("insert")) {
        return host.applyGesture(InsertAppletIntent{step.value(QStringLiteral("plugin")).toString()},
                                 targetOf(step), step.value(QStringLiteral("instanceId")).toString());
    }
    if (kind == QLatin1String("move")) {
        DropTarget target = targetOf(step);
        const QString targetPanel = step.value(QStringLiteral("targetPanel")).toString();
        if (!targetPanel.isEmpty()) {
            target.panelId = targetPanel;
        }
        return host.applyGesture(MoveAppletIntent{panel, applet}, target);
    }
    if (kind == QLatin1String("remove")) {
        return host.applyGesture(removeIntent(panel, applet), targetOf(step));
    }
    if (kind == QLatin1String("appletSettings")) {
        return host.applyGesture(configureAppletSettingsIntent(
                                     panel, applet, step.value(QStringLiteral("settings")).toObject().toVariantMap()),
                                 targetOf(step));
    }
    if (kind == QLatin1String("addPanel")) {
        Profiles::PanelSpec spec;
        spec.id = step.value(QStringLiteral("panelId")).toString();
        spec.output = QStringLiteral("*");
        Profiles::parseEdge(step.value(QStringLiteral("edge")).toString(), &spec.edge);
        spec.thickness = 32;
        return host.applyGesture(addPanelIntent(spec, std::nullopt), targetOf(step));
    }
    if (kind == QLatin1String("removePanel")) {
        return host.applyGesture(removePanelIntent(panel), targetOf(step));
    }
    if (kind == QLatin1String("undo")) {
        return host.undo();
    }
    if (kind == QLatin1String("redo")) {
        return host.redo();
    }
    if (kind == QLatin1String("apply")) {
        return host.apply();
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOptions({
        {QStringLiteral("profile-dir"), QStringLiteral("profile catalog directory"), QStringLiteral("dir")},
        {QStringLiteral("profile-id"), QStringLiteral("profile id"), QStringLiteral("id")},
        {QStringLiteral("applet-dir"), QStringLiteral("manifest directory"), QStringLiteral("dir")},
        {QStringLiteral("output-dir"), QStringLiteral("user profile store directory"), QStringLiteral("dir")},
        {QStringLiteral("intents"), QStringLiteral("JSON array of intent steps"), QStringLiteral("file")},
        {QStringLiteral("width"), QStringLiteral("logical output width"), QStringLiteral("px"), QStringLiteral("1920")},
        {QStringLiteral("height"), QStringLiteral("logical output height"), QStringLiteral("px"), QStringLiteral("1080")},
        {QStringLiteral("scale"), QStringLiteral("output scale"), QStringLiteral("factor"), QStringLiteral("1")},
    });
    parser.process(application);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const auto loaded = Profiles::ProfileLoader::fromFile(
        QDir(parser.value(QStringLiteral("profile-dir")))
            .filePath(UserProfileStore::fileNameForId(parser.value(QStringLiteral("profile-id")))));
    if (!loaded.ok) {
        err << "profile: " << loaded.error.message << '\n';
        return 2;
    }
    Applets::ManifestCatalog catalog;
    QString error;
    if (!catalog.loadDirectory(parser.value(QStringLiteral("applet-dir")), &error)) {
        err << "manifests: " << error << '\n';
        return 2;
    }
    QFile intentsFile(parser.value(QStringLiteral("intents")));
    if (!intentsFile.open(QIODevice::ReadOnly)) {
        err << "intents: cannot read " << intentsFile.fileName() << '\n';
        return 2;
    }
    const QJsonArray steps = QJsonDocument::fromJson(intentsFile.readAll()).array();
    const QVector<ShellLayout::LogicalOutput> outputs{
        {QStringLiteral("OUT-1"),
         QRect(0, 0, parser.value(QStringLiteral("width")).toInt(),
               parser.value(QStringLiteral("height")).toInt()),
         parser.value(QStringLiteral("scale")).toDouble()}};
    Apps::SettingsCustomize::RepositoryCustomizeEditorHost host(
        loaded.profile, outputs, catalog.manifests(), parser.value(QStringLiteral("output-dir")));
    if (!host.ready()) {
        err << "host: " << host.unavailableReason() << '\n';
        return 2;
    }
    int index = 0;
    for (const QJsonValue &value : steps) {
        const auto outcome = runStep(host, value.toObject());
        if (!outcome.has_value()) {
            err << "step " << index << ": unknown kind\n";
            return 3;
        }
        if (!outcome->ok) {
            err << "step " << index << ": " << outcome->message << '\n';
            return 3;
        }
        ++index;
    }
    out << QDir(parser.value(QStringLiteral("output-dir")))
               .filePath(UserProfileStore::fileNameForId(loaded.profile.id))
        << '\n';
    return 0;
}
