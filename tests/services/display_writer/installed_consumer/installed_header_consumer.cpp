// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_writer/output_management_port.h>
#include <qindaqt/services/display_writer/production_output_management_port.h>
#include <qindaqt/services/display_writer/writer_transaction_port.h>

#include <memory>
#include <type_traits>

using namespace QindaQt::DisplayWriter;

static_assert(std::is_same_v<decltype(makeProductionOutputManagementPort()),
                             std::unique_ptr<OutputManagementPort>>);

Configuration installedHeaderConfiguration()
{
    return {.requestId = 1,
            .scope = ConfigurationScope::SurvivingProperties,
            .outputs = {{.connectorName = QStringLiteral("Virtual-1"),
                         .enabled = true,
                         .mode = {.pixelSize = QSize(1920, 1080),
                                  .refreshMilliHertz = 60'000}}}};
}
