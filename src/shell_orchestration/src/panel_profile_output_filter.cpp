// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_orchestration/panel_profile_output_filter.h"

#include <QSet>
#include <QString>

namespace QindaQt::ShellOrchestration {

Profiles::LayoutProfile PanelProfileOutputOccupancy::presentOutputsOnly(
    const Profiles::LayoutProfile &profile,
    const QVector<ShellLayout::LogicalOutput> &outputs)
{
    QSet<QString> present;
    present.reserve(outputs.size());
    for (const auto &output : outputs) {
        present.insert(output.id);
    }
    Profiles::LayoutProfile result = profile;
    result.panels.clear();
    result.panels.reserve(profile.panels.size());
    for (const auto &panel : profile.panels) {
        if (panel.output == QStringLiteral("*") || present.contains(panel.output)) {
            result.panels.append(panel);
        }
    }
    return result;
}

} // namespace QindaQt::ShellOrchestration
