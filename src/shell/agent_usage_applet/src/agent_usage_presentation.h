// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/agent_usage/agent_usage_source.h>
#include <QVariantList>
namespace QindaQt::Shell::AgentUsageApplet {
// Pure projection: unavailable facts remain unknown; cost is provider-reported,
// never derived from token counts or an assumed subscription price.
QVariantList providerRows(const Services::AgentUsage::UsageSnapshot &snapshot);
}
