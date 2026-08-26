// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace QindaQt::Profiles {
namespace {

LoadResult failure(const QString &origin, const QString &message)
{
    return {.ok = false, .profile = {}, .error = origin + QStringLiteral(": ") + message};
}

bool parseApplet(const QJsonObject &object, AppletSpec *applet, QString *error)
{
    applet->id = object.value(QStringLiteral("id")).toString();
    applet->plugin = object.value(QStringLiteral("plugin")).toString();
    applet->settings = object.value(QStringLiteral("settings")).toObject().toVariantMap();
    if (applet->id.isEmpty() || applet->plugin.isEmpty()) {
        *error = QStringLiteral("every applet requires non-empty id and plugin values");
        return false;
    }
    return true;
}

bool parsePanel(const QJsonObject &object, PanelSpec *panel, QString *error)
{
    panel->id = object.value(QStringLiteral("id")).toString();
    panel->output = object.value(QStringLiteral("output")).toString(QStringLiteral("*"));
    panel->rows = object.value(QStringLiteral("rows")).toInt(1);
    panel->thickness = object.value(QStringLiteral("thickness")).toInt(32);
    panel->length = object.value(QStringLiteral("length")).toDouble(1.0);

    if (panel->id.isEmpty()) {
        *error = QStringLiteral("every panel requires a non-empty id");
        return false;
    }
    if (!parseEdge(object.value(QStringLiteral("edge")).toString(QStringLiteral("top")), &panel->edge)
        || !parseLayer(object.value(QStringLiteral("layer")).toString(QStringLiteral("above")), &panel->layer)
        || !parseHideMode(object.value(QStringLiteral("hideMode")).toString(QStringLiteral("never")),
                          &panel->hideMode)
        || !parseAlignment(object.value(QStringLiteral("alignment")).toString(QStringLiteral("fill")),
                           &panel->alignment)) {
        *error = QStringLiteral("panel %1 contains an unknown enum value").arg(panel->id);
        return false;
    }
    if (panel->rows < 1 || panel->rows > 4 || panel->thickness < 20 || panel->thickness > 192
        || panel->length < 0.1 || panel->length > 1.0) {
        *error = QStringLiteral("panel %1 has invalid rows, thickness, or length").arg(panel->id);
        return false;
    }

    QSet<QString> appletIds;
    const auto applets = object.value(QStringLiteral("applets")).toArray();
    for (const auto &value : applets) {
        AppletSpec applet;
        if (!value.isObject() || !parseApplet(value.toObject(), &applet, error)) {
            return false;
        }
        if (appletIds.contains(applet.id)) {
            *error = QStringLiteral("panel %1 repeats applet id %2").arg(panel->id, applet.id);
            return false;
        }
        appletIds.insert(applet.id);
        panel->applets.append(applet);
    }
    return true;
}

} // namespace

LoadResult ProfileLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path, file.errorString());
    }
    return fromJson(file.readAll(), path);
}

LoadResult ProfileLoader::fromJson(const QByteArray &json, const QString &origin)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(origin, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }

    const auto root = document.object();
    LayoutProfile profile;
    profile.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    profile.id = root.value(QStringLiteral("id")).toString();
    profile.name = root.value(QStringLiteral("name")).toString();
    profile.description = root.value(QStringLiteral("description")).toString();
    profile.defaultTheme = root.value(QStringLiteral("defaultTheme")).toString(QStringLiteral("qinda-dark"));

    if (profile.schemaVersion != QINDAQT_PROFILE_SCHEMA_VERSION) {
        return failure(origin, QStringLiteral("unsupported schemaVersion %1").arg(profile.schemaVersion));
    }
    if (profile.id.isEmpty() || profile.name.isEmpty()) {
        return failure(origin, QStringLiteral("profile requires non-empty id and name values"));
    }

    const auto workflow = root.value(QStringLiteral("workflow")).toObject();
    profile.workflow.overview = workflow.value(QStringLiteral("overview")).toString(QStringLiteral("compact"));
    profile.workflow.workspacePolicy =
        workflow.value(QStringLiteral("workspacePolicy")).toString(QStringLiteral("static"));
    profile.workflow.launcher = workflow.value(QStringLiteral("launcher")).toString(QStringLiteral("shelf"));
    profile.workflow.menu = workflow.value(QStringLiteral("menu")).toString(QStringLiteral("global"));
    profile.workflow.taskList = workflow.value(QStringLiteral("taskList")).toString(QStringLiteral("grouped"));
    profile.workflow.globalMenu = workflow.value(QStringLiteral("globalMenu")).toBool(true);

    QSet<QString> panelIds;
    QString error;
    for (const auto &value : root.value(QStringLiteral("panels")).toArray()) {
        PanelSpec panel;
        if (!value.isObject() || !parsePanel(value.toObject(), &panel, &error)) {
            return failure(origin, error);
        }
        if (panelIds.contains(panel.id)) {
            return failure(origin, QStringLiteral("repeated panel id %1").arg(panel.id));
        }
        panelIds.insert(panel.id);
        profile.panels.append(panel);
    }
    if (profile.panels.isEmpty()) {
        return failure(origin, QStringLiteral("profile must define at least one panel or dock"));
    }
    return {.ok = true, .profile = profile, .error = {}};
}

QVector<LoadResult> ProfileLoader::fromDirectory(const QString &path)
{
    QDir directory(path);
    const auto names = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVector<LoadResult> results;
    results.reserve(names.size());
    for (const auto &name : names) {
        results.append(fromFile(directory.filePath(name)));
    }
    return results;
}

} // namespace QindaQt::Profiles
