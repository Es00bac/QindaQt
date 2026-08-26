// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;

struct ProbeWindowTitles final
{
    QString primary;
    QString secondary;
    QString page;
};

// Runs the mutation-only portion of the live compositor contract. The caller
// has already established capabilities and the development mutation gate.
[[nodiscard]] std::optional<QJsonObject>
exerciseDevelopmentWorkflow(CompositorProbeClient &client, const ProbeWindowTitles &titles,
                            QString *error);

} // namespace QindaQt::Test
