// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>

#include <optional>

namespace QindaQt::Test {

class CompositorProbeClient;

[[nodiscard]] std::optional<QJsonObject>
exerciseDevelopmentOutputHotplug(CompositorProbeClient &client, QString *error);

[[nodiscard]] std::optional<QJsonObject>
exerciseProductionOutputGate(CompositorProbeClient &client, QString *error);

} // namespace QindaQt::Test
