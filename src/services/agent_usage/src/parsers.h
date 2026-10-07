// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/agent_usage/agent_usage_types.h>
#include <QJsonObject>
namespace QindaQt::Services::AgentUsage::Private {
ProviderUsage emptyProvider(const QString &id);
ProviderUsage parseReport(const QByteArray &bytes, const QString &id, QDateTime now);
bool parseRateLimits(const QJsonObject &result, ProviderUsage &usage);
bool parseCodexUsage(const QJsonObject &result, ProviderUsage &usage);
}
