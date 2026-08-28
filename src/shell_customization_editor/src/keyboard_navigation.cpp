// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_customization_editor/keyboard_navigation.h"

#include <QVector>

#include <algorithm>

namespace QindaQt::ShellCustomizationEditor {

namespace {

constexpr int zoneCount = 3;

QString zoneAt(int index)
{
    static const QString zones[zoneCount] = {
        QStringLiteral("start"),
        QStringLiteral("center"),
        QStringLiteral("end"),
    };
    return zones[index];
}

std::optional<int> zoneIndex(const QString &zone)
{
    for (int index = 0; index < zoneCount; ++index) {
        if (zone == zoneAt(index)) {
            return index;
        }
    }
    return std::nullopt;
}

const Profiles::PanelSpec *findPanel(const Profiles::LayoutProfile &profile,
                                     const QString &panelId)
{
    const auto found = std::find_if(profile.panels.cbegin(),
                                    profile.panels.cend(),
                                    [&panelId](const Profiles::PanelSpec &candidate) {
                                        return candidate.id == panelId;
                                    });
    return found == profile.panels.cend() ? nullptr : &*found;
}

QVector<const Profiles::AppletSpec *> appletsInZone(const Profiles::PanelSpec &panel,
                                                    const QString &zone)
{
    QVector<const Profiles::AppletSpec *> result;
    for (const Profiles::AppletSpec &candidate : panel.applets) {
        const QString candidateZone = candidate.settings
                                          .value(QStringLiteral("zone"),
                                                 QStringLiteral("start"))
                                          .toString();
        if (candidateZone == zone) {
            result.append(&candidate);
        }
    }
    return result;
}

std::optional<qsizetype> anchorIndex(
    const QVector<const Profiles::AppletSpec *> &zoneApplets,
    const DropTarget &target)
{
    if (!target.beforeAppletId.has_value()) {
        return zoneApplets.size();
    }
    for (qsizetype index = 0; index < zoneApplets.size(); ++index) {
        if (zoneApplets.at(index)->id == *target.beforeAppletId) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<QString> anchorIdAt(
    const QVector<const Profiles::AppletSpec *> &zoneApplets,
    qsizetype index)
{
    return index >= 0 && index < zoneApplets.size()
        ? std::optional<QString>{zoneApplets.at(index)->id}
        : std::nullopt;
}

DropTarget clampedTarget(const Profiles::PanelSpec &panel, const DropTarget &current)
{
    DropTarget target;
    target.panelId = panel.id;
    target.zone = current.zone;
    target.beforeAppletId = current.beforeAppletId;
    return target;
}

std::optional<DropTarget> steppedPanelTarget(const Profiles::LayoutProfile &profile,
                                             const DropTarget &current,
                                             int stepDelta)
{
    qsizetype index = 0;
    for (; index < profile.panels.size(); ++index) {
        if (profile.panels.at(index).id == current.panelId) {
            break;
        }
    }
    if (index >= profile.panels.size()) {
        return std::nullopt;
    }
    const qsizetype stepped = index + stepDelta;
    if (stepped < 0 || stepped >= profile.panels.size()) {
        return std::nullopt;
    }
    DropTarget target;
    target.panelId = profile.panels.at(stepped).id;
    target.zone = current.zone;
    target.beforeAppletId.reset();
    return target;
}

} // namespace

std::optional<DropTarget> nextSlotInPanel(const Profiles::LayoutProfile &profile,
                                          const DropTarget &current)
{
    const Profiles::PanelSpec *panel = findPanel(profile, current.panelId);
    if (panel == nullptr) {
        return std::nullopt;
    }
    const auto zoneApplets = appletsInZone(*panel, current.zone);
    const auto index = anchorIndex(zoneApplets, current);
    if (!index.has_value() || *index >= zoneApplets.size()) {
        return std::nullopt;
    }
    DropTarget target = clampedTarget(*panel, current);
    target.beforeAppletId = anchorIdAt(zoneApplets, *index + 1);
    return target;
}

std::optional<DropTarget> previousSlotInPanel(const Profiles::LayoutProfile &profile,
                                              const DropTarget &current)
{
    const Profiles::PanelSpec *panel = findPanel(profile, current.panelId);
    if (panel == nullptr) {
        return std::nullopt;
    }
    const auto zoneApplets = appletsInZone(*panel, current.zone);
    const auto index = anchorIndex(zoneApplets, current);
    if (!index.has_value() || *index == 0) {
        // Position 1 is the first slot; there is no earlier anchor.
        return std::nullopt;
    }
    DropTarget target = clampedTarget(*panel, current);
    target.beforeAppletId = anchorIdAt(zoneApplets, *index - 1);
    return target;
}

std::optional<DropTarget> nextZoneInPanel(const Profiles::LayoutProfile &profile,
                                          const DropTarget &current)
{
    const std::optional<int> index = zoneIndex(current.zone);
    if (!index.has_value() || *index + 1 >= zoneCount) {
        return std::nullopt;
    }
    const Profiles::PanelSpec *panel = findPanel(profile, current.panelId);
    if (panel == nullptr) {
        return std::nullopt;
    }
    DropTarget target = clampedTarget(*panel, current);
    target.zone = zoneAt(*index + 1);
    return target;
}

std::optional<DropTarget> previousZoneInPanel(const Profiles::LayoutProfile &profile,
                                              const DropTarget &current)
{
    const std::optional<int> index = zoneIndex(current.zone);
    if (!index.has_value() || *index == 0) {
        return std::nullopt;
    }
    const Profiles::PanelSpec *panel = findPanel(profile, current.panelId);
    if (panel == nullptr) {
        return std::nullopt;
    }
    DropTarget target = clampedTarget(*panel, current);
    target.zone = zoneAt(*index - 1);
    return target;
}

std::optional<DropTarget> nextPanelTarget(const Profiles::LayoutProfile &profile,
                                          const DropTarget &current)
{
    return steppedPanelTarget(profile, current, 1);
}

std::optional<DropTarget> previousPanelTarget(const Profiles::LayoutProfile &profile,
                                              const DropTarget &current)
{
    return steppedPanelTarget(profile, current, -1);
}

MovePanelIntent steppedPanelEdge(const Profiles::LayoutProfile &profile,
                                 const QString &panelId,
                                 int stepDelta)
{
    MovePanelIntent intent;
    const Profiles::PanelSpec *panel = findPanel(profile, panelId);
    if (panel == nullptr) {
        return intent;
    }

    using Edge = Profiles::Edge;
    static constexpr Edge edgeCycle[4] = {Edge::Top, Edge::Right, Edge::Bottom, Edge::Left};

    int index = 0;
    for (int candidate = 0; candidate < 4; ++candidate) {
        if (edgeCycle[candidate] == panel->edge) {
            index = candidate;
            break;
        }
    }
    const int stepped = ((index + stepDelta) % 4 + 4) % 4;

    intent.panelId = panel->id;
    intent.outputId = panel->output;
    intent.edge = edgeCycle[stepped];
    intent.alignment = panel->alignment;
    intent.beforePanelId.reset();
    return intent;
}

} // namespace QindaQt::ShellCustomizationEditor
