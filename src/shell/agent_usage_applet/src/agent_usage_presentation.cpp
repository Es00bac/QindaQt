// SPDX-License-Identifier: GPL-3.0-or-later
#include "agent_usage_presentation.h"
#include <QLocale>
#include <QStringList>
namespace QindaQt::Shell::AgentUsageApplet {
using namespace Services::AgentUsage;
namespace {
QString count(const std::optional<quint64> &value)
{
    return value ? QLocale().toString(*value) : QStringLiteral("Not reported");
}
QString stateLabel(UsageState state)
{
    switch (state) {
    case UsageState::Ready: return QStringLiteral("Current");
    case UsageState::Stale: return QStringLiteral("Stale");
    case UsageState::Error: return QStringLiteral("Unavailable");
    case UsageState::Unavailable: return QStringLiteral("Not configured");
    }
    return QStringLiteral("Unavailable");
}
}
QVariantList providerRows(const UsageSnapshot &snapshot)
{
    QVariantList result;
    for (const auto &provider : snapshot) {
        bool metricsAvailable = provider.inputTokens || provider.outputTokens
            || provider.totalTokens || provider.reportedCostUsd;
        QStringList quotas;
        for (const auto &quota : provider.quotaWindows) {
            metricsAvailable = metricsAvailable || quota.usedPercent.has_value()
                || quota.resetAt.has_value();
            QString text = quota.label + QStringLiteral(": ");
            text += quota.usedPercent ? QStringLiteral("%1% remaining").arg(
                        QLocale().toString(qMax(0.0, 100.0 - *quota.usedPercent), 'f', 1))
                                      : QStringLiteral("Remaining limit not reported");
            text += quota.resetAt && quota.resetAt->isValid()
                ? QStringLiteral(" · resets %1").arg(QLocale().toString(
                      quota.resetAt->toLocalTime(), QLocale::ShortFormat))
                : QStringLiteral(" · reset not reported");
            quotas.append(text);
        }
        if (quotas.isEmpty())
            quotas.append(QStringLiteral("Limits and reset times not reported"));
        const QString observed = provider.observedAt.isValid()
            ? QStringLiteral("Observed %1").arg(QLocale().toString(
                  provider.observedAt.toLocalTime(), QLocale::ShortFormat))
            : QStringLiteral("No observation yet");
        result.append(QVariantMap{
            {QStringLiteral("id"), provider.providerId},
            {QStringLiteral("metricsAvailable"), metricsAvailable},
            {QStringLiteral("hasObservation"), provider.observedAt.isValid()},
            {QStringLiteral("name"), provider.displayName},
            {QStringLiteral("state"), stateLabel(provider.state)},
            {QStringLiteral("detail"), provider.detail},
            {QStringLiteral("source"), provider.source},
            {QStringLiteral("observed"), observed},
            {QStringLiteral("scope"), provider.scope.isEmpty()
                ? QStringLiteral("Scope not reported") : provider.scope},
            {QStringLiteral("tokenScope"), provider.tokenScope.isEmpty() ? QStringLiteral("Token scope not reported") : QStringLiteral("Tokens: %1").arg(provider.tokenScope)},
            {QStringLiteral("costScope"), provider.costScope.isEmpty() ? QStringLiteral("Cost scope not reported") : QStringLiteral("Cost: %1").arg(provider.costScope)},
            {QStringLiteral("tokens"), QStringLiteral("Total tokens: %1 · input: %2 · output: %3")
                .arg(count(provider.totalTokens), count(provider.inputTokens), count(provider.outputTokens))},
            {QStringLiteral("cost"), provider.reportedCostUsd
                ? QStringLiteral("Reported cost: US$%1").arg(
                      QLocale().toString(*provider.reportedCostUsd, 'f', 4))
                : QStringLiteral("Reported cost: not reported")},
            {QStringLiteral("quotas"), quotas}});
    }
    return result;
}
}
