// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/agent_usage/agent_usage_types.h>
namespace QindaQt::Services::AgentUsage::Private {
UsageSnapshot readReports(const QString &directory,QDateTime now);
}
