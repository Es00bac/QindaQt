// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>
namespace QindaQt::Services::AgentUsage {
enum class UsageState { Unavailable, Ready, Stale, Error };
struct QuotaWindow {
    QString label;
    std::optional<double> usedPercent;
    std::optional<QDateTime> resetAt;
};
struct ProviderUsage {
    QString providerId, displayName;
    UsageState state = UsageState::Unavailable;
    QString detail, source, scope, tokenScope, costScope;
    QDateTime observedAt;
    std::optional<quint64> inputTokens, outputTokens, totalTokens;
    std::optional<double> reportedCostUsd;
    QVector<QuotaWindow> quotaWindows;
};
using UsageSnapshot = QVector<ProviderUsage>;
}
