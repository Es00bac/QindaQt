// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/layout_profile.h"

#include <QJsonArray>

namespace QindaQt::Profiles {

QVariantMap AppletSpec::toVariantMap() const
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("plugin"), plugin},
            {QStringLiteral("settings"), settings}};
}

QVariantMap PanelSpec::toVariantMap() const
{
    QVariantList appletValues;
    appletValues.reserve(applets.size());
    for (const auto &applet : applets) {
        appletValues.append(applet.toVariantMap());
    }
    return {{QStringLiteral("id"), id},
            {QStringLiteral("output"), output},
            {QStringLiteral("edge"), toString(edge)},
            {QStringLiteral("layer"), toString(layer)},
            {QStringLiteral("hideMode"), toString(hideMode)},
            {QStringLiteral("alignment"), toString(alignment)},
            {QStringLiteral("rows"), rows},
            {QStringLiteral("thickness"), thickness},
            {QStringLiteral("length"), length},
            {QStringLiteral("applets"), appletValues}};
}

QVariantMap WorkflowSpec::toVariantMap() const
{
    return {{QStringLiteral("overview"), overview},
            {QStringLiteral("workspacePolicy"), workspacePolicy},
            {QStringLiteral("launcher"), launcher},
            {QStringLiteral("menu"), menu},
            {QStringLiteral("taskList"), taskList},
            {QStringLiteral("globalMenu"), globalMenu}};
}

QVariantMap LayoutProfile::toVariantMap() const
{
    QVariantList panelValues;
    panelValues.reserve(panels.size());
    for (const auto &panel : panels) {
        panelValues.append(panel.toVariantMap());
    }
    QVariantMap values = {{QStringLiteral("schemaVersion"), schemaVersion},
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("defaultTheme"), defaultTheme},
            {QStringLiteral("workflow"), workflow.toVariantMap()},
            {QStringLiteral("panels"), panelValues}};
    if (!desktopApplets.isEmpty()) {
        QVariantList desktopAppletValues;
        desktopAppletValues.reserve(desktopApplets.size());
        for (const auto &applet : desktopApplets) {
            desktopAppletValues.append(applet.toVariantMap());
        }
        values.insert(QStringLiteral("desktop"),
                      QVariantMap{{QStringLiteral("applets"), desktopAppletValues}});
    }
    return values;
}

QJsonObject LayoutProfile::toJson() const
{
    return QJsonObject::fromVariantMap(toVariantMap());
}

} // namespace QindaQt::Profiles
