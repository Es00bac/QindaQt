// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_writer/output_management_port.h>

#include <memory>

namespace QindaQt::DisplayWriter
{

// Creates the production adapter for KDE's public output-management protocol.
// The returned port owns its protocol connection and remains thread-confined;
// no platform type or compositor-private ABI crosses this installed boundary.
[[nodiscard]] std::unique_ptr<OutputManagementPort>
makeProductionOutputManagementPort();

} // namespace QindaQt::DisplayWriter
