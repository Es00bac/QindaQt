// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/shell_customization_editor/editor_intent.h"

#include <QStringList>

#include <algorithm>
#include <utility>

namespace QindaQt::ShellCustomizationEditor::TestFixtures {

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

inline QStringList appletIds(const Profiles::LayoutProfile &profile, const QString &panelId)
{
    QStringList result;
    if (const Profiles::PanelSpec *owner = panel(profile, panelId)) {
        for (const Profiles::AppletSpec &candidate : owner->applets) {
            result.append(candidate.id);
        }
    }
    return result;
}

inline DragPayload palettePayload(QString pluginId)
{
    DragPayload payload;
    payload.kind = PayloadKind::PalettePlugin;
    payload.pluginId = std::move(pluginId);
    return payload;
}

inline DragPayload instancePayload(QString sourcePanelId, QString sourceAppletId)
{
    DragPayload payload;
    payload.kind = PayloadKind::AppletInstance;
    payload.sourcePanelId = std::move(sourcePanelId);
    payload.sourceAppletId = std::move(sourceAppletId);
    return payload;
}

} // namespace QindaQt::ShellCustomizationEditor::TestFixtures
