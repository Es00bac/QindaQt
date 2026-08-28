// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_topology/topology_types.h>

namespace QindaQt::DisplayTopology::Private
{

[[nodiscard]] ValidationResult failure(TopologyError error, const char *reason,
                                       const QString &stableId = {});
[[nodiscard]] const Display::Output *findOutput(const Display::Snapshot &snapshot,
                                                const QString &stableId);
[[nodiscard]] const Display::Mode *findMode(const Display::Output &output,
                                            const QString &modeId);
[[nodiscard]] bool checkedRect(const QPoint &position, const QSize &size, QRect &rect);

} // namespace QindaQt::DisplayTopology::Private
