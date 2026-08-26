// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QStringList>

#include <algorithm>
#include <utility>

namespace QindaQt::ShellCustomization::TestFixtures {

inline Profiles::LayoutProfile profile()
{
    Profiles::LayoutProfile result;
    result.id = QStringLiteral("editor-fixture");
    result.name = QStringLiteral("Editor Fixture");

    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.output = QStringLiteral("*");
    bar.edge = Profiles::Edge::Top;
    bar.thickness = 32;
    bar.applets = {
        {.id = QStringLiteral("launcher-instance"),
         .plugin = QStringLiteral("launcher"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}},
        {.id = QStringLiteral("clock-instance"),
         .plugin = QStringLiteral("clock"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("end")}}},
    };

    Profiles::PanelSpec dock;
    dock.id = QStringLiteral("dock");
    dock.output = QStringLiteral("primary");
    dock.edge = Profiles::Edge::Bottom;
    dock.layer = Profiles::Layer::Overlay;
    dock.alignment = Profiles::Alignment::Center;
    dock.length = 0.6;
    dock.thickness = 48;
    dock.applets = {
        {.id = QStringLiteral("tasks-instance"),
         .plugin = QStringLiteral("task-list"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("center")}}},
    };

    result.panels = {bar, dock};
    return result;
}

inline QVector<ShellLayout::LogicalOutput> outputs()
{
    return {
        {QStringLiteral("external"), {-1920, 0, 1920, 1200}, 1.0},
        {QStringLiteral("primary"), {0, 0, 2048, 1152}, 1.25},
    };
}

inline Applets::AppletManifest manifest(QString id = QStringLiteral("clock"))
{
    Applets::AppletManifest result;
    result.id = std::move(id);
    result.name = QStringLiteral("Fixture Applet");
    result.apiVersion = Applets::ApiVersion::current();
    result.entryPoint = {Applets::EntryPointKind::Builtin, result.id};
    result.placementZones = {Applets::PlacementZone::PanelStart,
                             Applets::PlacementZone::PanelCenter,
                             Applets::PlacementZone::PanelEnd};
    result.orientations = {Applets::Orientation::Horizontal,
                           Applets::Orientation::Vertical};
    result.sizing.mainAxis = {16, 32, 256, false};
    result.sizing.crossAxis = {16, 32, 128, false};
    result.settingsSchema = {
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
    };
    return result;
}

inline QVector<Applets::AppletManifest> manifests()
{
    return {
        manifest(QStringLiteral("launcher")),
        manifest(QStringLiteral("clock")),
        manifest(QStringLiteral("task-list")),
    };
}

inline const Profiles::PanelSpec *panel(const Profiles::LayoutProfile &profile,
                                        const QString &id)
{
    const auto found = std::find_if(profile.panels.cbegin(),
                                    profile.panels.cend(),
                                    [&id](const Profiles::PanelSpec &candidate) {
                                        return candidate.id == id;
                                    });
    return found == profile.panels.cend() ? nullptr : &*found;
}

inline const Profiles::AppletSpec *applet(const Profiles::LayoutProfile &profile,
                                          const QString &panelId,
                                          const QString &appletId)
{
    const Profiles::PanelSpec *owner = panel(profile, panelId);
    if (owner == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(owner->applets.cbegin(),
                                    owner->applets.cend(),
                                    [&appletId](const Profiles::AppletSpec &candidate) {
                                        return candidate.id == appletId;
                                    });
    return found == owner->applets.cend() ? nullptr : &*found;
}

inline QStringList appletIds(const Profiles::LayoutProfile &profile,
                             const QString &panelId)
{
    QStringList result;
    if (const Profiles::PanelSpec *owner = panel(profile, panelId)) {
        for (const Profiles::AppletSpec &candidate : owner->applets) {
            result.append(candidate.id);
        }
    }
    return result;
}

} // namespace QindaQt::ShellCustomization::TestFixtures
